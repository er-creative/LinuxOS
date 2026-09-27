#include "devai/features/VolumeFeatureEngine.hpp"
#include "devai/features/VolumeFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


using namespace devai::market;
using namespace devai::features;


namespace
{

// ============================================================================
// Configuration
// ============================================================================

const std::filesystem::path DEFAULT_DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";


constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;


constexpr std::int64_t FIVE_MINUTE_SECONDS =
    5 * 60;


constexpr std::int64_t FIFTEEN_MINUTE_SECONDS =
    15 * 60;


constexpr std::size_t AVG20_PERIOD =
    20;


constexpr std::size_t AVG50_PERIOD =
    50;


constexpr std::size_t HISTORY_LIMIT =
    50;


constexpr double ABSOLUTE_TOLERANCE =
    1e-9;


constexpr double RELATIVE_TOLERANCE =
    1e-9;


// ============================================================================
// Validation statistics
// ============================================================================

struct ValidationStats
{
    // Market data

    std::size_t symbols{0};

    std::size_t one_minute_candles{0};
    std::size_t five_minute_candles{0};
    std::size_t fifteen_minute_candles{0};

    std::size_t incomplete_5m_buckets{0};
    std::size_t incomplete_15m_buckets{0};

    std::size_t invalid_rows{0};
    std::size_t duplicate_rows{0};
    std::size_t skipped_rows{0};


    // Runtime

    std::size_t decision_points{0};
    std::size_t active_session_points{0};
    std::size_t new_sessions{0};


    // Snapshot availability

    std::size_t snapshots_with_1m{0};
    std::size_t snapshots_with_5m{0};
    std::size_t snapshots_with_15m{0};


    // Feature availability

    std::size_t one_minute_avg20_available{0};
    std::size_t one_minute_avg50_available{0};
    std::size_t one_minute_rvol20_available{0};
    std::size_t one_minute_rvol50_available{0};

    std::size_t five_minute_avg20_available{0};
    std::size_t five_minute_avg50_available{0};
    std::size_t five_minute_rvol20_available{0};
    std::size_t five_minute_rvol50_available{0};

    std::size_t fifteen_minute_avg20_available{0};
    std::size_t fifteen_minute_avg50_available{0};
    std::size_t fifteen_minute_rvol20_available{0};
    std::size_t fifteen_minute_rvol50_available{0};

    std::size_t session_volume_available{0};


    // Validation failures

    std::size_t average_volume_mismatches{0};
    std::size_t rvol_mismatches{0};

    std::size_t session_cumulative_mismatches{0};
    std::size_t session_bar_count_mismatches{0};
    std::size_t session_average_mismatches{0};

    std::size_t session_reset_violations{0};
    std::size_t cross_session_history_violations{0};

    std::size_t duplicate_history_violations{0};

    std::size_t lookahead_violations{0};

    std::size_t symbol_violations{0};
    std::size_t decision_time_violations{0};

    std::size_t invalid_numeric_values{0};

    std::size_t feature_exceptions{0};
};


// ============================================================================
// Independent validation state
//
// IMPORTANT:
//
// This state is completely separate from VolumeFeatureEngine.
// It independently calculates what the feature engine SHOULD produce.
// ============================================================================

struct IndependentTimeframeState
{
    std::optional<std::int64_t>
        last_timestamp;


    std::deque<double>
        volume_history;
};


struct IndependentSessionState
{
    std::optional<TradingDate>
        trading_date;


    double cumulative_volume{0.0};


    std::size_t bar_count{0};
};


// ============================================================================
// Numeric comparison
// ============================================================================

bool approximatelyEqual(
    double lhs,
    double rhs)
{
    if (
        std::isnan(lhs) &&
        std::isnan(rhs)
    )
    {
        return true;
    }


    if (
        !std::isfinite(lhs) ||
        !std::isfinite(rhs)
    )
    {
        return lhs == rhs;
    }


    const double difference =
        std::fabs(
            lhs - rhs
        );


    if (
        difference <=
        ABSOLUTE_TOLERANCE
    )
    {
        return true;
    }


    const double scale =
        std::max(
            {
                1.0,
                std::fabs(lhs),
                std::fabs(rhs)
            }
        );


    return
        difference <=
        RELATIVE_TOLERANCE *
        scale;
}


// ============================================================================
// Rolling average
// ============================================================================

std::optional<double> independentAverage(
    const std::deque<double>& history,
    std::size_t period)
{
    if (
        period == 0 ||
        history.size() < period
    )
    {
        return std::nullopt;
    }


    double sum =
        0.0;


    const std::size_t start =
        history.size() -
        period;


    for (
        std::size_t index = start;
        index < history.size();
        ++index
    )
    {
        sum +=
            history[index];
    }


    return
        sum /
        static_cast<double>(
            period
        );
}


// ============================================================================
// Independent candle consumption
//
// Returns true only when a genuinely new candle is consumed.
// ============================================================================

bool independentConsume(
    IndependentTimeframeState& state,
    const std::optional<Candle>& candle,
    ValidationStats& stats)
{
    if (!candle)
    {
        return false;
    }


    if (state.last_timestamp)
    {
        if (
            candle->timestamp ==
            *state.last_timestamp
        )
        {
            return false;
        }


        if (
            candle->timestamp <
            *state.last_timestamp
        )
        {
            ++stats.duplicate_history_violations;

            return false;
        }
    }


    state.volume_history.push_back(
        candle->volume
    );


    while (
        state.volume_history.size() >
        HISTORY_LIMIT
    )
    {
        state.volume_history.pop_front();
    }


    state.last_timestamp =
        candle->timestamp;


    return true;
}


// ============================================================================
// Validate one timeframe
// ============================================================================

void validateTimeframe(
    const TimeframeVolumeFeatures& actual,
    const IndependentTimeframeState& expected_state,
    const std::optional<Candle>& candle,
    ValidationStats& stats)
{
    if (!candle)
    {
        return;
    }


    // ========================================================================
    // Current volume
    // ========================================================================

    if (
        !actual.has_volume ||
        !approximatelyEqual(
            actual.volume,
            candle->volume
        )
    )
    {
        ++stats.average_volume_mismatches;
    }


    // ========================================================================
    // Average Volume 20
    // ========================================================================

    const auto expected_average20 =
        independentAverage(
            expected_state.volume_history,
            AVG20_PERIOD
        );


    if (expected_average20)
    {
        if (
            !actual.has_average_volume_20 ||
            !approximatelyEqual(
                actual.average_volume_20,
                *expected_average20
            )
        )
        {
            ++stats.average_volume_mismatches;
        }


        if (*expected_average20 > 0.0)
        {
            const double expected_rvol20 =
                candle->volume /
                *expected_average20;


            if (
                !actual.has_relative_volume_20 ||
                !approximatelyEqual(
                    actual.relative_volume_20,
                    expected_rvol20
                )
            )
            {
                ++stats.rvol_mismatches;
            }
        }
        else
        {
            if (actual.has_relative_volume_20)
            {
                ++stats.rvol_mismatches;
            }
        }
    }
    else
    {
        if (actual.has_average_volume_20)
        {
            ++stats.average_volume_mismatches;
        }


        if (actual.has_relative_volume_20)
        {
            ++stats.rvol_mismatches;
        }
    }


    // ========================================================================
    // Average Volume 50
    // ========================================================================

    const auto expected_average50 =
        independentAverage(
            expected_state.volume_history,
            AVG50_PERIOD
        );


    if (expected_average50)
    {
        if (
            !actual.has_average_volume_50 ||
            !approximatelyEqual(
                actual.average_volume_50,
                *expected_average50
            )
        )
        {
            ++stats.average_volume_mismatches;
        }


        if (*expected_average50 > 0.0)
        {
            const double expected_rvol50 =
                candle->volume /
                *expected_average50;


            if (
                !actual.has_relative_volume_50 ||
                !approximatelyEqual(
                    actual.relative_volume_50,
                    expected_rvol50
                )
            )
            {
                ++stats.rvol_mismatches;
            }
        }
        else
        {
            if (actual.has_relative_volume_50)
            {
                ++stats.rvol_mismatches;
            }
        }
    }
    else
    {
        if (actual.has_average_volume_50)
        {
            ++stats.average_volume_mismatches;
        }


        if (actual.has_relative_volume_50)
        {
            ++stats.rvol_mismatches;
        }
    }
}


// ============================================================================
// Validate numeric integrity
//
// NaN is valid during warm-up.
//
// Infinity is never valid.
// ============================================================================

void validateNumericIntegrity(
    const VolumeFeatures& features,
    ValidationStats& stats)
{
    const double values[] =
    {
        features.one_minute.volume,
        features.one_minute.average_volume_20,
        features.one_minute.average_volume_50,
        features.one_minute.relative_volume_20,
        features.one_minute.relative_volume_50,

        features.five_minute.volume,
        features.five_minute.average_volume_20,
        features.five_minute.average_volume_50,
        features.five_minute.relative_volume_20,
        features.five_minute.relative_volume_50,

        features.fifteen_minute.volume,
        features.fifteen_minute.average_volume_20,
        features.fifteen_minute.average_volume_50,
        features.fifteen_minute.relative_volume_20,
        features.fifteen_minute.relative_volume_50,

        features.session_cumulative_volume,
        features.session_average_volume_per_bar
    };


    for (
        const double value :
        values
    )
    {
        if (std::isinf(value))
        {
            ++stats.invalid_numeric_values;
        }
    }


    // Values advertised as available must be finite and non-negative.

    const auto validate_available =
        [&stats](
            bool available,
            double value)
        {
            if (
                available &&
                (
                    !std::isfinite(value) ||
                    value < 0.0
                )
            )
            {
                ++stats.invalid_numeric_values;
            }
        };


    validate_available(
        features.one_minute.has_volume,
        features.one_minute.volume
    );

    validate_available(
        features.one_minute.has_average_volume_20,
        features.one_minute.average_volume_20
    );

    validate_available(
        features.one_minute.has_average_volume_50,
        features.one_minute.average_volume_50
    );

    validate_available(
        features.one_minute.has_relative_volume_20,
        features.one_minute.relative_volume_20
    );

    validate_available(
        features.one_minute.has_relative_volume_50,
        features.one_minute.relative_volume_50
    );


    validate_available(
        features.five_minute.has_volume,
        features.five_minute.volume
    );

    validate_available(
        features.five_minute.has_average_volume_20,
        features.five_minute.average_volume_20
    );

    validate_available(
        features.five_minute.has_average_volume_50,
        features.five_minute.average_volume_50
    );

    validate_available(
        features.five_minute.has_relative_volume_20,
        features.five_minute.relative_volume_20
    );

    validate_available(
        features.five_minute.has_relative_volume_50,
        features.five_minute.relative_volume_50
    );


    validate_available(
        features.fifteen_minute.has_volume,
        features.fifteen_minute.volume
    );

    validate_available(
        features.fifteen_minute.has_average_volume_20,
        features.fifteen_minute.average_volume_20
    );

    validate_available(
        features.fifteen_minute.has_average_volume_50,
        features.fifteen_minute.average_volume_50
    );

    validate_available(
        features.fifteen_minute.has_relative_volume_20,
        features.fifteen_minute.relative_volume_20
    );

    validate_available(
        features.fifteen_minute.has_relative_volume_50,
        features.fifteen_minute.relative_volume_50
    );


    if (
        features.has_session_volume &&
        (
            !std::isfinite(
                features.session_cumulative_volume
            ) ||
            features.session_cumulative_volume < 0.0 ||
            !std::isfinite(
                features.session_average_volume_per_bar
            ) ||
            features.session_average_volume_per_bar < 0.0
        )
    )
    {
        ++stats.invalid_numeric_values;
    }
}


// ============================================================================
// Feature availability counters
// ============================================================================

void countAvailability(
    const VolumeFeatures& features,
    ValidationStats& stats)
{
    if (
        features.one_minute.has_average_volume_20
    )
    {
        ++stats.one_minute_avg20_available;
    }


    if (
        features.one_minute.has_average_volume_50
    )
    {
        ++stats.one_minute_avg50_available;
    }


    if (
        features.one_minute.has_relative_volume_20
    )
    {
        ++stats.one_minute_rvol20_available;
    }


    if (
        features.one_minute.has_relative_volume_50
    )
    {
        ++stats.one_minute_rvol50_available;
    }


    if (
        features.five_minute.has_average_volume_20
    )
    {
        ++stats.five_minute_avg20_available;
    }


    if (
        features.five_minute.has_average_volume_50
    )
    {
        ++stats.five_minute_avg50_available;
    }


    if (
        features.five_minute.has_relative_volume_20
    )
    {
        ++stats.five_minute_rvol20_available;
    }


    if (
        features.five_minute.has_relative_volume_50
    )
    {
        ++stats.five_minute_rvol50_available;
    }


    if (
        features.fifteen_minute.has_average_volume_20
    )
    {
        ++stats.fifteen_minute_avg20_available;
    }


    if (
        features.fifteen_minute.has_average_volume_50
    )
    {
        ++stats.fifteen_minute_avg50_available;
    }


    if (
        features.fifteen_minute.has_relative_volume_20
    )
    {
        ++stats.fifteen_minute_rvol20_available;
    }


    if (
        features.fifteen_minute.has_relative_volume_50
    )
    {
        ++stats.fifteen_minute_rvol50_available;
    }


    if (features.has_session_volume)
    {
        ++stats.session_volume_available;
    }
}


// ============================================================================
// Discover symbols
//
// Every file matching:
//
//     *_1min.txt
//
// becomes one validation symbol.
//
// This avoids introducing a second symbol-list dependency into the validator.
// ============================================================================

std::vector<std::string> discoverSymbols(
    const std::filesystem::path& data_folder)
{
    std::vector<std::string> symbols;


    if (
        !std::filesystem::exists(
            data_folder
        )
    )
    {
        throw std::runtime_error(
            "Data folder does not exist: " +
            data_folder.string()
        );
    }


    if (
        !std::filesystem::is_directory(
            data_folder
        )
    )
    {
        throw std::runtime_error(
            "Data path is not a directory: " +
            data_folder.string()
        );
    }


    constexpr const char* suffix =
        "_1min.txt";


    const std::string suffix_string =
        suffix;


    for (
        const auto& entry :
        std::filesystem::directory_iterator(
            data_folder
        )
    )
    {
        if (!entry.is_regular_file())
        {
            continue;
        }


        const std::string filename =
            entry.path().filename().string();


        if (
            filename.size() <=
            suffix_string.size()
        )
        {
            continue;
        }


        if (
            filename.compare(
                filename.size() -
                    suffix_string.size(),
                suffix_string.size(),
                suffix_string
            ) != 0
        )
        {
            continue;
        }


        const std::string symbol =
            filename.substr(
                0,
                filename.size() -
                    suffix_string.size()
            );


        if (!symbol.empty())
        {
            symbols.push_back(
                symbol
            );
        }
    }


    std::sort(
        symbols.begin(),
        symbols.end()
    );


    symbols.erase(
        std::unique(
            symbols.begin(),
            symbols.end()
        ),
        symbols.end()
    );


    return symbols;
}


// ============================================================================
// Validate one symbol
// ============================================================================

void validateSymbol(
    const std::filesystem::path& data_folder,
    const std::string& symbol,
    ValidationStats& total_stats)
{
    std::cout
        << "\n"
        << "------------------------------------------------------------\n"
        << "Validating : "
        << symbol
        << "\n"
        << "------------------------------------------------------------\n";


    const std::filesystem::path filename =
        data_folder /
        (
            symbol +
            "_1min.txt"
        );


    // ========================================================================
    // Load real 1-minute data
    // ========================================================================

    MarketDataLoader loader(
        data_folder
    );


    const auto load_result =
        loader.loadPath(
            filename
        );


    if (
        load_result.candles.empty()
    )
    {
        throw std::runtime_error(
            "No 1-minute candles loaded for " +
            symbol
        );
    }


    for (
        const Candle& candle :
        load_result.candles
    )
    {
        if (
            candle.symbol !=
            symbol
        )
        {
            throw std::runtime_error(
                "Unexpected symbol inside file: " +
                filename.string()
            );
        }
    }


    // ========================================================================
    // Aggregate 1m -> 5m / 15m
    // ========================================================================

    CandleAggregator aggregator;


    const auto five_result =
        aggregator.aggregate(
            load_result.candles,
            Timeframe::FIVE_MINUTES
        );


    const auto fifteen_result =
        aggregator.aggregate(
            load_result.candles,
            Timeframe::FIFTEEN_MINUTES
        );


    if (
        five_result.candles.empty()
    )
    {
        throw std::runtime_error(
            "No 5-minute candles generated for " +
            symbol
        );
    }


    if (
        fifteen_result.candles.empty()
    )
    {
        throw std::runtime_error(
            "No 15-minute candles generated for " +
            symbol
        );
    }


    // ========================================================================
    // Market-data statistics
    // ========================================================================

    ++total_stats.symbols;


    total_stats.one_minute_candles +=
        load_result.candles.size();


    total_stats.five_minute_candles +=
        five_result.candles.size();


    total_stats.fifteen_minute_candles +=
        fifteen_result.candles.size();


    total_stats.incomplete_5m_buckets +=
        five_result.incomplete_buckets;


    total_stats.incomplete_15m_buckets +=
        fifteen_result.incomplete_buckets;


    total_stats.invalid_rows +=
        load_result.invalid_rows;


    total_stats.duplicate_rows +=
        load_result.duplicate_rows;


    total_stats.skipped_rows +=
        load_result.skipped_rows;


    // ========================================================================
    // Synchronizer
    // ========================================================================

    MultiTimeframeSynchronizerConfig config;


    config.mode =
        AvailabilityMode::ZERO_LATENCY;


    MultiTimeframeSynchronizer synchronizer(
        config
    );


    const auto timed_one =
        synchronizer.prepareOneMinute(
            load_result.candles
        );


    const auto timed_five =
        synchronizer.prepareFiveMinute(
            five_result.candles
        );


    const auto timed_fifteen =
        synchronizer.prepareFifteenMinute(
            fifteen_result.candles
        );


    // ========================================================================
    // Phase 2 runtime
    // ========================================================================

    MultiTimeframeCursor cursor(
        timed_one,
        timed_five,
        timed_fifteen
    );


    SessionState session;


    MarketSnapshotBuilder snapshot_builder(
        symbol
    );


    // ========================================================================
    // Phase 3.3 production engine
    // ========================================================================

    VolumeFeatureEngine feature_engine(
        symbol
    );


    // ========================================================================
    // Independent expected state
    //
    // Continuous histories survive session changes.
    // ========================================================================

    IndependentTimeframeState
        expected_one;


    IndependentTimeframeState
        expected_five;


    IndependentTimeframeState
        expected_fifteen;


    IndependentSessionState
        expected_session;


    // Used specifically to verify that rolling history survives
    // a TradingDate transition.

    std::size_t previous_one_history_size =
        0;


    std::size_t previous_five_history_size =
        0;


    std::size_t previous_fifteen_history_size =
        0;


    // ========================================================================
    // Runtime
    //
    // Each real 1-minute candle becomes available at timestamp + 60 seconds.
    // ========================================================================

    for (
        const Candle& source_candle :
        load_result.candles
    )
    {
        const std::int64_t decision_time =
            source_candle.timestamp +
            ONE_MINUTE_SECONDS;


        ++total_stats.decision_points;


        MarketSnapshot snapshot =
            snapshot_builder.build(
                decision_time,
                cursor,
                session
            );


        if (snapshot.sessionActive())
        {
            ++total_stats.active_session_points;
        }


        if (snapshot.new_session)
        {
            ++total_stats.new_sessions;
        }


        // ====================================================================
        // Snapshot identity
        // ====================================================================

        if (
            snapshot.symbol !=
            symbol
        )
        {
            ++total_stats.symbol_violations;
        }


        if (
            snapshot.decision_time !=
            decision_time
        )
        {
            ++total_stats.decision_time_violations;
        }


        // ====================================================================
        // Snapshot availability
        // ====================================================================

        if (snapshot.one_minute)
        {
            ++total_stats.snapshots_with_1m;
        }


        if (snapshot.five_minute)
        {
            ++total_stats.snapshots_with_5m;
        }


        if (snapshot.fifteen_minute)
        {
            ++total_stats.snapshots_with_15m;
        }


        // ====================================================================
        // Look-ahead validation
        //
        // Candle timestamp is the START of the candle.
        //
        // Therefore:
        //
        //     1m available >= timestamp + 1 minute
        //     5m available >= timestamp + 5 minutes
        //     15m available >= timestamp + 15 minutes
        // ====================================================================

        if (
            snapshot.one_minute &&
            snapshot.one_minute->timestamp +
                ONE_MINUTE_SECONDS >
                decision_time
        )
        {
            ++total_stats.lookahead_violations;
        }


        if (
            snapshot.five_minute &&
            snapshot.five_minute->timestamp +
                FIVE_MINUTE_SECONDS >
                decision_time
        )
        {
            ++total_stats.lookahead_violations;
        }


        if (
            snapshot.fifteen_minute &&
            snapshot.fifteen_minute->timestamp +
                FIFTEEN_MINUTE_SECONDS >
                decision_time
        )
        {
            ++total_stats.lookahead_violations;
        }


        // ====================================================================
        // Independent session boundary
        // ====================================================================

        bool trading_date_changed =
            false;


        if (!expected_session.trading_date)
        {
            expected_session.trading_date =
                snapshot.trading_date;


            expected_session.cumulative_volume =
                0.0;


            expected_session.bar_count =
                0;
        }
        else if (
            *expected_session.trading_date !=
            snapshot.trading_date
        )
        {
            trading_date_changed =
                true;


            expected_session.trading_date =
                snapshot.trading_date;


            expected_session.cumulative_volume =
                0.0;


            expected_session.bar_count =
                0;
        }


        // ====================================================================
        // Verify cross-session history was NOT reset
        // ====================================================================

        if (trading_date_changed)
        {
            if (
                expected_one.volume_history.size() <
                previous_one_history_size
            )
            {
                ++total_stats.cross_session_history_violations;
            }


            if (
                expected_five.volume_history.size() <
                previous_five_history_size
            )
            {
                ++total_stats.cross_session_history_violations;
            }


            if (
                expected_fifteen.volume_history.size() <
                previous_fifteen_history_size
            )
            {
                ++total_stats.cross_session_history_violations;
            }
        }


        // ====================================================================
        // Independent candle consumption
        // ====================================================================

        const bool new_one =
            independentConsume(
                expected_one,
                snapshot.one_minute,
                total_stats
            );


        static_cast<void>(
            independentConsume(
                expected_five,
                snapshot.five_minute,
                total_stats
            )
        );


        static_cast<void>(
            independentConsume(
                expected_fifteen,
                snapshot.fifteen_minute,
                total_stats
            )
        );


        // ====================================================================
        // Independent session volume
        // ====================================================================

        if (
            new_one &&
            snapshot.one_minute
        )
        {
            expected_session.cumulative_volume +=
                snapshot.one_minute->volume;


            ++expected_session.bar_count;
        }


        // ====================================================================
        // Run production VolumeFeatureEngine
        // ====================================================================

        VolumeFeatures features;


        try
        {
            features =
                feature_engine.update(
                    snapshot
                );
        }
        catch (
            const std::exception& exception
        )
        {
            ++total_stats.feature_exceptions;


            std::cerr
                << "Feature exception for "
                << symbol
                << " at decision time "
                << decision_time
                << ": "
                << exception.what()
                << "\n";


            continue;
        }


        // ====================================================================
        // Feature identity
        // ====================================================================

        if (
            features.symbol !=
            symbol
        )
        {
            ++total_stats.symbol_violations;
        }


        if (
            features.decision_time !=
            decision_time
        )
        {
            ++total_stats.decision_time_violations;
        }


        // ====================================================================
        // Validate rolling timeframe features
        // ====================================================================

        validateTimeframe(
            features.one_minute,
            expected_one,
            snapshot.one_minute,
            total_stats
        );


        validateTimeframe(
            features.five_minute,
            expected_five,
            snapshot.five_minute,
            total_stats
        );


        validateTimeframe(
            features.fifteen_minute,
            expected_fifteen,
            snapshot.fifteen_minute,
            total_stats
        );


        // ====================================================================
        // Validate session cumulative volume
        // ====================================================================

        if (
            !approximatelyEqual(
                features.session_cumulative_volume,
                expected_session.cumulative_volume
            )
        )
        {
            ++total_stats.session_cumulative_mismatches;
        }


        // ====================================================================
        // Validate session bar count
        // ====================================================================

        if (
            features.session_bar_count !=
            expected_session.bar_count
        )
        {
            ++total_stats.session_bar_count_mismatches;
        }


        // ====================================================================
        // Validate session average
        // ====================================================================

        if (
            expected_session.bar_count > 0
        )
        {
            const double expected_average =
                expected_session.cumulative_volume /
                static_cast<double>(
                    expected_session.bar_count
                );


            if (
                !features.has_session_volume ||
                !approximatelyEqual(
                    features.session_average_volume_per_bar,
                    expected_average
                )
            )
            {
                ++total_stats.session_average_mismatches;
            }
        }
        else
        {
            if (features.has_session_volume)
            {
                ++total_stats.session_average_mismatches;
            }
        }


        // ====================================================================
        // Explicit new-session reset validation
        //
        // On the first available 1m candle of a new TradingDate:
        //
        //     session bar count should be exactly 1
        //     cumulative volume should equal that candle's volume
        //
        // Continuous rolling histories are intentionally NOT reset.
        // ====================================================================

        if (
            trading_date_changed &&
            new_one &&
            snapshot.one_minute
        )
        {
            if (
                features.session_bar_count !=
                1
            )
            {
                ++total_stats.session_reset_violations;
            }


            if (
                !approximatelyEqual(
                    features.session_cumulative_volume,
                    snapshot.one_minute->volume
                )
            )
            {
                ++total_stats.session_reset_violations;
            }
        }


        // ====================================================================
        // Feature availability
        // ====================================================================

        countAvailability(
            features,
            total_stats
        );


        // ====================================================================
        // Numeric integrity
        // ====================================================================

        validateNumericIntegrity(
            features,
            total_stats
        );


        // ====================================================================
        // Save independent history sizes
        // ====================================================================

        previous_one_history_size =
            expected_one.volume_history.size();


        previous_five_history_size =
            expected_five.volume_history.size();


        previous_fifteen_history_size =
            expected_fifteen.volume_history.size();
    }


    std::cout
        << "1m candles : "
        << load_result.candles.size()
        << " | 5m : "
        << five_result.candles.size()
        << " | 15m : "
        << fifteen_result.candles.size()
        << " | incomplete 5m : "
        << five_result.incomplete_buckets
        << " | incomplete 15m : "
        << fifteen_result.incomplete_buckets
        << "\n";
}


// ============================================================================
// Determine final pass/fail
// ============================================================================

bool validationPassed(
    const ValidationStats& stats)
{
    return
        stats.symbols > 0 &&

        stats.decision_points > 0 &&

        stats.average_volume_mismatches == 0 &&
        stats.rvol_mismatches == 0 &&

        stats.session_cumulative_mismatches == 0 &&
        stats.session_bar_count_mismatches == 0 &&
        stats.session_average_mismatches == 0 &&

        stats.session_reset_violations == 0 &&
        stats.cross_session_history_violations == 0 &&

        stats.duplicate_history_violations == 0 &&

        stats.lookahead_violations == 0 &&

        stats.symbol_violations == 0 &&
        stats.decision_time_violations == 0 &&

        stats.invalid_numeric_values == 0 &&

        stats.feature_exceptions == 0;
}


// ============================================================================
// Report
// ============================================================================

void printReport(
    const std::filesystem::path& data_folder,
    const ValidationStats& stats)
{
    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 3.3 - ALL-SYMBOL REAL VOLUME FEATURE VALIDATION\n"
        << "============================================================\n"
        << "Data folder : "
        << data_folder
        << "\n"
        << "Symbols     : "
        << stats.symbols
        << "\n"
        << "\n"

        << "Market data\n"
        << "------------------------------------------------------------\n"
        << "1-minute candles                : "
        << stats.one_minute_candles
        << "\n"
        << "5-minute candles                : "
        << stats.five_minute_candles
        << "\n"
        << "15-minute candles               : "
        << stats.fifteen_minute_candles
        << "\n"
        << "Incomplete 5m buckets           : "
        << stats.incomplete_5m_buckets
        << "\n"
        << "Incomplete 15m buckets          : "
        << stats.incomplete_15m_buckets
        << "\n"
        << "Invalid rows                    : "
        << stats.invalid_rows
        << "\n"
        << "Duplicate rows                  : "
        << stats.duplicate_rows
        << "\n"
        << "Skipped rows                    : "
        << stats.skipped_rows
        << "\n"
        << "\n"

        << "Runtime\n"
        << "------------------------------------------------------------\n"
        << "Decision points                 : "
        << stats.decision_points
        << "\n"
        << "Active-session points           : "
        << stats.active_session_points
        << "\n"
        << "New sessions                    : "
        << stats.new_sessions
        << "\n"
        << "Snapshots with 1m               : "
        << stats.snapshots_with_1m
        << "\n"
        << "Snapshots with 5m               : "
        << stats.snapshots_with_5m
        << "\n"
        << "Snapshots with 15m              : "
        << stats.snapshots_with_15m
        << "\n"
        << "\n"

        << "1-minute feature availability\n"
        << "------------------------------------------------------------\n"
        << "Average Volume 20               : "
        << stats.one_minute_avg20_available
        << "\n"
        << "Average Volume 50               : "
        << stats.one_minute_avg50_available
        << "\n"
        << "RVOL20                          : "
        << stats.one_minute_rvol20_available
        << "\n"
        << "RVOL50                          : "
        << stats.one_minute_rvol50_available
        << "\n"
        << "\n"

        << "5-minute feature availability\n"
        << "------------------------------------------------------------\n"
        << "Average Volume 20               : "
        << stats.five_minute_avg20_available
        << "\n"
        << "Average Volume 50               : "
        << stats.five_minute_avg50_available
        << "\n"
        << "RVOL20                          : "
        << stats.five_minute_rvol20_available
        << "\n"
        << "RVOL50                          : "
        << stats.five_minute_rvol50_available
        << "\n"
        << "\n"

        << "15-minute feature availability\n"
        << "------------------------------------------------------------\n"
        << "Average Volume 20               : "
        << stats.fifteen_minute_avg20_available
        << "\n"
        << "Average Volume 50               : "
        << stats.fifteen_minute_avg50_available
        << "\n"
        << "RVOL20                          : "
        << stats.fifteen_minute_rvol20_available
        << "\n"
        << "RVOL50                          : "
        << stats.fifteen_minute_rvol50_available
        << "\n"
        << "\n"

        << "Session features\n"
        << "------------------------------------------------------------\n"
        << "Session volume available        : "
        << stats.session_volume_available
        << "\n"
        << "\n"

        << "Validation\n"
        << "------------------------------------------------------------\n"
        << "Average-volume mismatches       : "
        << stats.average_volume_mismatches
        << "\n"
        << "RVOL mismatches                 : "
        << stats.rvol_mismatches
        << "\n"
        << "Session cumulative mismatches   : "
        << stats.session_cumulative_mismatches
        << "\n"
        << "Session bar-count mismatches    : "
        << stats.session_bar_count_mismatches
        << "\n"
        << "Session average mismatches      : "
        << stats.session_average_mismatches
        << "\n"
        << "Session reset violations        : "
        << stats.session_reset_violations
        << "\n"
        << "Cross-session history violations: "
        << stats.cross_session_history_violations
        << "\n"
        << "Duplicate-history violations    : "
        << stats.duplicate_history_violations
        << "\n"
        << "Look-ahead violations           : "
        << stats.lookahead_violations
        << "\n"
        << "Symbol violations               : "
        << stats.symbol_violations
        << "\n"
        << "Decision-time violations        : "
        << stats.decision_time_violations
        << "\n"
        << "Invalid numeric values          : "
        << stats.invalid_numeric_values
        << "\n"
        << "Feature exceptions              : "
        << stats.feature_exceptions
        << "\n"
        << "============================================================\n";


    if (validationPassed(stats))
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.3 REAL VOLUME FEATURE VALIDATION PASSED\n"
            << "============================================================\n";
    }
    else
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.3 REAL VOLUME FEATURE VALIDATION FAILED\n"
            << "============================================================\n";
    }
}

} // namespace


// ============================================================================
// Program entry
// ============================================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        std::filesystem::path data_folder =
            DEFAULT_DATA_FOLDER;


        if (argc >= 2)
        {
            data_folder =
                argv[1];
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "CETE PHASE 3.3 - REAL VOLUME FEATURE VALIDATOR\n"
            << "============================================================\n"
            << "Data folder : "
            << data_folder
            << "\n"
            << "Mode        : ZERO_LATENCY historical runtime\n"
            << "Scope       : ALL *_1min.txt symbols\n"
            << "============================================================\n";


        // ====================================================================
        // Discover all available symbols
        // ====================================================================

        const std::vector<std::string> symbols =
            discoverSymbols(
                data_folder
            );


        if (symbols.empty())
        {
            throw std::runtime_error(
                "No *_1min.txt market-data files found in: " +
                data_folder.string()
            );
        }


        std::cout
            << "\nDiscovered symbols : "
            << symbols.size()
            << "\n";


        ValidationStats total_stats;


        // ====================================================================
        // Validate every real symbol independently
        //
        // Each symbol receives:
        //
        //     fresh cursor
        //     fresh SessionState
        //     fresh MarketSnapshotBuilder
        //     fresh VolumeFeatureEngine
        //     fresh independent validation state
        // ====================================================================

        for (
            std::size_t index = 0;
            index < symbols.size();
            ++index
        )
        {
            const std::string& symbol =
                symbols[index];


            std::cout
                << "\n["
                << std::setw(3)
                << index + 1
                << "/"
                << symbols.size()
                << "] "
                << symbol
                << "\n";


            try
            {
                validateSymbol(
                    data_folder,
                    symbol,
                    total_stats
                );
            }
            catch (
                const std::exception& exception
            )
            {
                ++total_stats.feature_exceptions;


                std::cerr
                    << "FAILED symbol "
                    << symbol
                    << ": "
                    << exception.what()
                    << "\n";
            }
        }


        // ====================================================================
        // Final aggregate report
        // ====================================================================

        printReport(
            data_folder,
            total_stats
        );


        return
            validationPassed(
                total_stats
            )
            ? 0
            : 1;
    }
    catch (
        const std::exception& exception
    )
    {
        std::cerr
            << "\nFATAL ERROR: "
            << exception.what()
            << "\n";


        return 1;
    }
}