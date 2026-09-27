#include "devai/features/VolatilityFeatureEngine.hpp"
#include "devai/features/VolatilityFeatures.hpp"

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
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


using namespace devai::features;
using namespace devai::market;


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


constexpr std::size_t ATR_PERIOD =
    14;


constexpr std::size_t WINDOW_20 =
    20;


constexpr std::size_t WINDOW_50 =
    50;


constexpr double NUMERIC_TOLERANCE =
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

    std::size_t incomplete_five_minute_buckets{0};

    std::size_t incomplete_fifteen_minute_buckets{0};

    std::size_t invalid_rows{0};

    std::size_t duplicate_rows{0};

    std::size_t skipped_rows{0};


    // Runtime

    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t new_sessions{0};

    std::size_t snapshots_with_one_minute{0};

    std::size_t snapshots_with_five_minute{0};

    std::size_t snapshots_with_fifteen_minute{0};


    // Feature availability

    std::size_t one_true_range_available{0};

    std::size_t one_atr14_available{0};

    std::size_t one_stddev20_available{0};

    std::size_t one_stddev50_available{0};

    std::size_t one_realized20_available{0};

    std::size_t one_realized50_available{0};


    std::size_t five_true_range_available{0};

    std::size_t five_atr14_available{0};

    std::size_t five_stddev20_available{0};

    std::size_t five_stddev50_available{0};

    std::size_t five_realized20_available{0};

    std::size_t five_realized50_available{0};


    std::size_t fifteen_true_range_available{0};

    std::size_t fifteen_atr14_available{0};

    std::size_t fifteen_stddev20_available{0};

    std::size_t fifteen_stddev50_available{0};

    std::size_t fifteen_realized20_available{0};

    std::size_t fifteen_realized50_available{0};


    // Validation failures

    std::size_t true_range_mismatches{0};

    std::size_t true_range_percent_mismatches{0};

    std::size_t atr14_mismatches{0};

    std::size_t atr14_percent_mismatches{0};

    std::size_t stddev20_mismatches{0};

    std::size_t stddev50_mismatches{0};

    std::size_t realized20_mismatches{0};

    std::size_t realized50_mismatches{0};

    std::size_t availability_mismatches{0};

    std::size_t duplicate_state_violations{0};

    std::size_t cross_session_history_violations{0};

    std::size_t overnight_gap_violations{0};

    std::size_t lookahead_violations{0};

    std::size_t symbol_violations{0};

    std::size_t decision_time_violations{0};

    std::size_t invalid_numeric_values{0};

    std::size_t feature_exceptions{0};
};


// ============================================================================
// Independent timeframe state
//
// IMPORTANT:
//
// This state is independent of VolatilityFeatureEngine.
//
// It exists only to calculate expected results.
//
// It deliberately persists across trading sessions.
// ============================================================================

struct ExpectedTimeframeState
{
    std::optional<std::int64_t>
        last_timestamp;


    std::optional<double>
        previous_close;


    std::optional<double>
        last_true_range;


    std::optional<double>
        last_true_range_percent;


    std::deque<double>
        initial_true_ranges;


    std::optional<double>
        atr14;


    std::deque<double>
        simple_returns;


    std::deque<double>
        log_returns;
};


// ============================================================================
// Expected feature result
// ============================================================================

struct ExpectedTimeframeFeatures
{
    double true_range{
        std::numeric_limits<double>::quiet_NaN()
    };


    double true_range_percent{
        std::numeric_limits<double>::quiet_NaN()
    };


    double atr14{
        std::numeric_limits<double>::quiet_NaN()
    };


    double atr14_percent{
        std::numeric_limits<double>::quiet_NaN()
    };


    double stddev20{
        std::numeric_limits<double>::quiet_NaN()
    };


    double stddev50{
        std::numeric_limits<double>::quiet_NaN()
    };


    double realized20{
        std::numeric_limits<double>::quiet_NaN()
    };


    double realized50{
        std::numeric_limits<double>::quiet_NaN()
    };


    bool has_true_range{false};

    bool has_true_range_percent{false};

    bool has_atr14{false};

    bool has_atr14_percent{false};

    bool has_stddev20{false};

    bool has_stddev50{false};

    bool has_realized20{false};

    bool has_realized50{false};


    bool new_candle{false};
};


// ============================================================================
// Numeric helpers
// ============================================================================

bool approximatelyEqual(
    double left,
    double right,
    double tolerance = NUMERIC_TOLERANCE)
{
    if (
        !std::isfinite(left) ||
        !std::isfinite(right)
    )
    {
        return false;
    }


    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            }
        );


    return
        std::fabs(
            left - right
        ) <=
        tolerance * scale;
}


// ============================================================================
// Bounded history
// ============================================================================

void appendBounded(
    std::deque<double>& values,
    double value,
    std::size_t maximum_size)
{
    values.push_back(
        value
    );


    while (
        values.size() >
        maximum_size
    )
    {
        values.pop_front();
    }
}


// ============================================================================
// Independent standard deviation
//
// Same mathematical definition required by CETE Phase 3.4:
//
// population standard deviation
//
//     sqrt(sum((x - mean)^2) / N)
// ============================================================================

double expectedStandardDeviation(
    const std::deque<double>& values,
    std::size_t period)
{
    if (
        period == 0 ||
        values.size() < period
    )
    {
        return std::numeric_limits<double>::quiet_NaN();
    }


    const std::size_t start =
        values.size() -
        period;


    double sum =
        0.0;


    for (
        std::size_t index = start;
        index < values.size();
        ++index
    )
    {
        sum +=
            values[index];
    }


    const double mean =
        sum /
        static_cast<double>(
            period
        );


    double squared_sum =
        0.0;


    for (
        std::size_t index = start;
        index < values.size();
        ++index
    )
    {
        const double difference =
            values[index] -
            mean;


        squared_sum +=
            difference *
            difference;
    }


    double variance =
        squared_sum /
        static_cast<double>(
            period
        );


    if (
        variance < 0.0 &&
        variance > -1e-15
    )
    {
        variance =
            0.0;
    }


    if (
        variance < 0.0 ||
        !std::isfinite(variance)
    )
    {
        return std::numeric_limits<double>::quiet_NaN();
    }


    return std::sqrt(
        variance
    );
}


// ============================================================================
// Independent True Range
// ============================================================================

double expectedTrueRange(
    const Candle& candle,
    const std::optional<double>& previous_close)
{
    const double high_low =
        candle.high -
        candle.low;


    if (!previous_close)
    {
        return high_low;
    }


    const double high_previous =
        std::fabs(
            candle.high -
            *previous_close
        );


    const double low_previous =
        std::fabs(
            candle.low -
            *previous_close
        );


    return std::max(
        {
            high_low,
            high_previous,
            low_previous
        }
    );
}


// ============================================================================
// Independent timeframe update
// ============================================================================

ExpectedTimeframeFeatures updateExpected(
    ExpectedTimeframeState& state,
    const std::optional<Candle>& candle)
{
    ExpectedTimeframeFeatures output;


    if (!candle)
    {
        return output;
    }


    // ------------------------------------------------------------------------
    // Duplicate candle
    //
    // No rolling state changes.
    // ------------------------------------------------------------------------

    if (state.last_timestamp)
    {
        if (
            candle->timestamp ==
            *state.last_timestamp
        )
        {
            if (state.last_true_range)
            {
                output.true_range =
                    *state.last_true_range;

                output.has_true_range =
                    true;
            }


            if (state.last_true_range_percent)
            {
                output.true_range_percent =
                    *state.last_true_range_percent;

                output.has_true_range_percent =
                    true;
            }


            if (state.atr14)
            {
                output.atr14 =
                    *state.atr14;

                output.has_atr14 =
                    true;


                output.atr14_percent =
                    *state.atr14 /
                    candle->close;

                output.has_atr14_percent =
                    std::isfinite(
                        output.atr14_percent
                    );
            }


            output.stddev20 =
                expectedStandardDeviation(
                    state.simple_returns,
                    WINDOW_20
                );


            output.stddev50 =
                expectedStandardDeviation(
                    state.simple_returns,
                    WINDOW_50
                );


            output.realized20 =
                expectedStandardDeviation(
                    state.log_returns,
                    WINDOW_20
                );


            output.realized50 =
                expectedStandardDeviation(
                    state.log_returns,
                    WINDOW_50
                );


            output.has_stddev20 =
                std::isfinite(
                    output.stddev20
                );


            output.has_stddev50 =
                std::isfinite(
                    output.stddev50
                );


            output.has_realized20 =
                std::isfinite(
                    output.realized20
                );


            output.has_realized50 =
                std::isfinite(
                    output.realized50
                );


            return output;
        }


        if (
            candle->timestamp <
            *state.last_timestamp
        )
        {
            throw std::runtime_error(
                "Independent volatility state moved backwards."
            );
        }
    }


    output.new_candle =
        true;


    // ------------------------------------------------------------------------
    // True Range
    // ------------------------------------------------------------------------

    const double true_range =
        expectedTrueRange(
            *candle,
            state.previous_close
        );


    const double true_range_percent =
        true_range /
        candle->close;


    state.last_true_range =
        true_range;


    state.last_true_range_percent =
        true_range_percent;


    output.true_range =
        true_range;


    output.true_range_percent =
        true_range_percent;


    output.has_true_range =
        true;


    output.has_true_range_percent =
        true;


    // ------------------------------------------------------------------------
    // ATR14
    // ------------------------------------------------------------------------

    if (!state.atr14)
    {
        state.initial_true_ranges.push_back(
            true_range
        );


        if (
            state.initial_true_ranges.size() ==
            ATR_PERIOD
        )
        {
            double sum =
                0.0;


            for (
                const double value :
                state.initial_true_ranges
            )
            {
                sum +=
                    value;
            }


            state.atr14 =
                sum /
                static_cast<double>(
                    ATR_PERIOD
                );


            state.initial_true_ranges.clear();
        }
    }
    else
    {
        state.atr14 =
            (
                (
                    *state.atr14 *
                    static_cast<double>(
                        ATR_PERIOD - 1
                    )
                ) +
                true_range
            ) /
            static_cast<double>(
                ATR_PERIOD
            );
    }


    // ------------------------------------------------------------------------
    // Returns
    // ------------------------------------------------------------------------

    if (state.previous_close)
    {
        const double simple_return =
            (
                candle->close /
                *state.previous_close
            ) -
            1.0;


        const double log_return =
            std::log(
                candle->close /
                *state.previous_close
            );


        appendBounded(
            state.simple_returns,
            simple_return,
            WINDOW_50
        );


        appendBounded(
            state.log_returns,
            log_return,
            WINDOW_50
        );
    }


    // ------------------------------------------------------------------------
    // Commit current close/timestamp
    // ------------------------------------------------------------------------

    state.previous_close =
        candle->close;


    state.last_timestamp =
        candle->timestamp;


    // ------------------------------------------------------------------------
    // Produce expected ATR
    // ------------------------------------------------------------------------

    if (state.atr14)
    {
        output.atr14 =
            *state.atr14;


        output.has_atr14 =
            true;


        output.atr14_percent =
            *state.atr14 /
            candle->close;


        output.has_atr14_percent =
            std::isfinite(
                output.atr14_percent
            );
    }


    // ------------------------------------------------------------------------
    // Rolling volatility
    // ------------------------------------------------------------------------

    output.stddev20 =
        expectedStandardDeviation(
            state.simple_returns,
            WINDOW_20
        );


    output.stddev50 =
        expectedStandardDeviation(
            state.simple_returns,
            WINDOW_50
        );


    output.realized20 =
        expectedStandardDeviation(
            state.log_returns,
            WINDOW_20
        );


    output.realized50 =
        expectedStandardDeviation(
            state.log_returns,
            WINDOW_50
        );


    output.has_stddev20 =
        std::isfinite(
            output.stddev20
        );


    output.has_stddev50 =
        std::isfinite(
            output.stddev50
        );


    output.has_realized20 =
        std::isfinite(
            output.realized20
        );


    output.has_realized50 =
        std::isfinite(
            output.realized50
        );


    return output;
}


// ============================================================================
// Look-ahead validation
// ============================================================================

bool hasLookahead(
    const std::optional<Candle>& candle,
    std::int64_t timeframe_seconds,
    std::int64_t decision_time)
{
    if (!candle)
    {
        return false;
    }


    return
        candle->timestamp +
            timeframe_seconds >
        decision_time;
}


// ============================================================================
// Validate one timeframe
// ============================================================================

void validateTimeframe(
    const TimeframeVolatilityFeatures& actual,
    const ExpectedTimeframeFeatures& expected,
    ValidationStats& stats)
{
    // ------------------------------------------------------------------------
    // Availability
    // ------------------------------------------------------------------------

    if (
        actual.has_true_range !=
        expected.has_true_range
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_true_range_percent !=
        expected.has_true_range_percent
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_atr14 !=
        expected.has_atr14
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_atr14_percent !=
        expected.has_atr14_percent
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_return_stddev_20 !=
        expected.has_stddev20
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_return_stddev_50 !=
        expected.has_stddev50
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_realized_volatility_20 !=
        expected.has_realized20
    )
    {
        ++stats.availability_mismatches;
    }


    if (
        actual.has_realized_volatility_50 !=
        expected.has_realized50
    )
    {
        ++stats.availability_mismatches;
    }


    // ------------------------------------------------------------------------
    // Values
    // ------------------------------------------------------------------------

    if (
        actual.has_true_range &&
        expected.has_true_range &&
        !approximatelyEqual(
            actual.true_range,
            expected.true_range
        )
    )
    {
        ++stats.true_range_mismatches;
    }


    if (
        actual.has_true_range_percent &&
        expected.has_true_range_percent &&
        !approximatelyEqual(
            actual.true_range_percent,
            expected.true_range_percent
        )
    )
    {
        ++stats.true_range_percent_mismatches;
    }


    if (
        actual.has_atr14 &&
        expected.has_atr14 &&
        !approximatelyEqual(
            actual.atr14,
            expected.atr14
        )
    )
    {
        ++stats.atr14_mismatches;
    }


    if (
        actual.has_atr14_percent &&
        expected.has_atr14_percent &&
        !approximatelyEqual(
            actual.atr14_percent,
            expected.atr14_percent
        )
    )
    {
        ++stats.atr14_percent_mismatches;
    }


    if (
        actual.has_return_stddev_20 &&
        expected.has_stddev20 &&
        !approximatelyEqual(
            actual.return_stddev_20,
            expected.stddev20
        )
    )
    {
        ++stats.stddev20_mismatches;
    }


    if (
        actual.has_return_stddev_50 &&
        expected.has_stddev50 &&
        !approximatelyEqual(
            actual.return_stddev_50,
            expected.stddev50
        )
    )
    {
        ++stats.stddev50_mismatches;
    }


    if (
        actual.has_realized_volatility_20 &&
        expected.has_realized20 &&
        !approximatelyEqual(
            actual.realized_volatility_20,
            expected.realized20
        )
    )
    {
        ++stats.realized20_mismatches;
    }


    if (
        actual.has_realized_volatility_50 &&
        expected.has_realized50 &&
        !approximatelyEqual(
            actual.realized_volatility_50,
            expected.realized50
        )
    )
    {
        ++stats.realized50_mismatches;
    }
}


// ============================================================================
// Numeric integrity
//
// NaN is legal when the associated feature is unavailable.
// A feature marked available must be finite and non-negative.
// ============================================================================

void validateNumericIntegrity(
    const TimeframeVolatilityFeatures& features,
    ValidationStats& stats)
{
    const auto validate =
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


    validate(
        features.has_true_range,
        features.true_range
    );


    validate(
        features.has_true_range_percent,
        features.true_range_percent
    );


    validate(
        features.has_atr14,
        features.atr14
    );


    validate(
        features.has_atr14_percent,
        features.atr14_percent
    );


    validate(
        features.has_return_stddev_20,
        features.return_stddev_20
    );


    validate(
        features.has_return_stddev_50,
        features.return_stddev_50
    );


    validate(
        features.has_realized_volatility_20,
        features.realized_volatility_20
    );


    validate(
        features.has_realized_volatility_50,
        features.realized_volatility_50
    );
}


// ============================================================================
// Feature availability counters
// ============================================================================

void countOneMinuteAvailability(
    const TimeframeVolatilityFeatures& features,
    ValidationStats& stats)
{
    if (features.has_true_range)
    {
        ++stats.one_true_range_available;
    }


    if (features.has_atr14)
    {
        ++stats.one_atr14_available;
    }


    if (features.has_return_stddev_20)
    {
        ++stats.one_stddev20_available;
    }


    if (features.has_return_stddev_50)
    {
        ++stats.one_stddev50_available;
    }


    if (features.has_realized_volatility_20)
    {
        ++stats.one_realized20_available;
    }


    if (features.has_realized_volatility_50)
    {
        ++stats.one_realized50_available;
    }
}


void countFiveMinuteAvailability(
    const TimeframeVolatilityFeatures& features,
    ValidationStats& stats)
{
    if (features.has_true_range)
    {
        ++stats.five_true_range_available;
    }


    if (features.has_atr14)
    {
        ++stats.five_atr14_available;
    }


    if (features.has_return_stddev_20)
    {
        ++stats.five_stddev20_available;
    }


    if (features.has_return_stddev_50)
    {
        ++stats.five_stddev50_available;
    }


    if (features.has_realized_volatility_20)
    {
        ++stats.five_realized20_available;
    }


    if (features.has_realized_volatility_50)
    {
        ++stats.five_realized50_available;
    }
}


void countFifteenMinuteAvailability(
    const TimeframeVolatilityFeatures& features,
    ValidationStats& stats)
{
    if (features.has_true_range)
    {
        ++stats.fifteen_true_range_available;
    }


    if (features.has_atr14)
    {
        ++stats.fifteen_atr14_available;
    }


    if (features.has_return_stddev_20)
    {
        ++stats.fifteen_stddev20_available;
    }


    if (features.has_return_stddev_50)
    {
        ++stats.fifteen_stddev50_available;
    }


    if (features.has_realized_volatility_20)
    {
        ++stats.fifteen_realized20_available;
    }


    if (features.has_realized_volatility_50)
    {
        ++stats.fifteen_realized50_available;
    }
}


// ============================================================================
// Discover all real symbols
//
// Files:
//
//     SYMBOL_1min.txt
// ============================================================================

std::vector<std::string> discoverSymbols(
    const std::filesystem::path& data_folder)
{
    if (
        !std::filesystem::exists(
            data_folder
        )
    )
    {
        throw std::runtime_error(
            "Market-data folder does not exist: " +
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
            "Market-data path is not a directory: " +
            data_folder.string()
        );
    }


    const std::string suffix =
        "_1min.txt";


    std::vector<std::string>
        symbols;


    for (
        const auto& entry :
        std::filesystem::directory_iterator(
            data_folder
        )
    )
    {
        if (
            !entry.is_regular_file()
        )
        {
            continue;
        }


        const std::string filename =
            entry.path()
                .filename()
                .string();


        if (
            filename.size() <=
            suffix.size()
        )
        {
            continue;
        }


        if (
            filename.compare(
                filename.size() -
                    suffix.size(),
                suffix.size(),
                suffix
            ) != 0
        )
        {
            continue;
        }


        symbols.push_back(
            filename.substr(
                0,
                filename.size() -
                    suffix.size()
            )
        );
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
    ValidationStats& stats)
{
    std::cout
        << "\n"
        << "------------------------------------------------------------\n"
        << "Validating : "
        << symbol
        << "\n"
        << "------------------------------------------------------------\n";


    // ========================================================================
    // Load 1-minute data
    // ========================================================================

    const std::filesystem::path filename =
        data_folder /
        (
            symbol +
            "_1min.txt"
        );


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
            "No candles loaded for " +
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
                "Unexpected symbol inside " +
                filename.string()
            );
        }
    }


    // ========================================================================
    // Aggregate 5m / 15m
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


    // ========================================================================
    // Accumulate market-data statistics
    // ========================================================================

    stats.one_minute_candles +=
        load_result.candles.size();


    stats.five_minute_candles +=
        five_result.candles.size();


    stats.fifteen_minute_candles +=
        fifteen_result.candles.size();


    stats.incomplete_five_minute_buckets +=
        five_result.incomplete_buckets;


    stats.incomplete_fifteen_minute_buckets +=
        fifteen_result.incomplete_buckets;


    stats.invalid_rows +=
        load_result.invalid_rows;


    stats.duplicate_rows +=
        load_result.duplicate_rows;


    stats.skipped_rows +=
        load_result.skipped_rows;


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
    // Runtime
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


    VolatilityFeatureEngine feature_engine(
        symbol
    );


    // ========================================================================
    // Independent expected states
    //
    // IMPORTANT:
    //
    // Never reset these at a new trading session.
    // ========================================================================

    ExpectedTimeframeState expected_one;

    ExpectedTimeframeState expected_five;

    ExpectedTimeframeState expected_fifteen;


    // ========================================================================
    // Cross-session validation state
    // ========================================================================

    std::optional<TradingDate>
        previous_trading_date;


    std::size_t previous_one_simple_size =
        0;


    std::size_t previous_five_simple_size =
        0;


    std::size_t previous_fifteen_simple_size =
        0;


    // ========================================================================
    // Runtime loop
    //
    // Every real 1-minute candle completion becomes a decision point.
    // ========================================================================

    for (
        const Candle& source_candle :
        load_result.candles
    )
    {
        const std::int64_t decision_time =
            source_candle.timestamp +
            ONE_MINUTE_SECONDS;


        ++stats.decision_points;


        // ====================================================================
        // Build legal point-in-time snapshot
        // ====================================================================

        MarketSnapshot snapshot =
            snapshot_builder.build(
                decision_time,
                cursor,
                session
            );


        if (
            snapshot.sessionActive()
        )
        {
            ++stats.active_session_points;
        }


        if (
            snapshot.new_session
        )
        {
            ++stats.new_sessions;
        }


        if (snapshot.one_minute)
        {
            ++stats.snapshots_with_one_minute;
        }


        if (snapshot.five_minute)
        {
            ++stats.snapshots_with_five_minute;
        }


        if (snapshot.fifteen_minute)
        {
            ++stats.snapshots_with_fifteen_minute;
        }


        // ====================================================================
        // Snapshot identity
        // ====================================================================

        if (
            snapshot.symbol !=
            symbol
        )
        {
            ++stats.symbol_violations;
        }


        if (
            snapshot.decision_time !=
            decision_time
        )
        {
            ++stats.decision_time_violations;
        }


        // ====================================================================
        // Look-ahead
        // ====================================================================

        if (
            hasLookahead(
                snapshot.one_minute,
                ONE_MINUTE_SECONDS,
                decision_time
            )
        )
        {
            ++stats.lookahead_violations;
        }


        if (
            hasLookahead(
                snapshot.five_minute,
                FIVE_MINUTE_SECONDS,
                decision_time
            )
        )
        {
            ++stats.lookahead_violations;
        }


        if (
            hasLookahead(
                snapshot.fifteen_minute,
                FIFTEEN_MINUTE_SECONDS,
                decision_time
            )
        )
        {
            ++stats.lookahead_violations;
        }


        // ====================================================================
        // Capture state BEFORE independent update
        //
        // Used for cross-session continuity checks.
        // ====================================================================

        previous_one_simple_size =
            expected_one.simple_returns.size();


        previous_five_simple_size =
            expected_five.simple_returns.size();


        previous_fifteen_simple_size =
            expected_fifteen.simple_returns.size();


        const bool trading_date_changed =
            previous_trading_date.has_value() &&
            *previous_trading_date !=
                snapshot.trading_date;


        // ====================================================================
        // Independent expected calculations
        // ====================================================================

        ExpectedTimeframeFeatures expected_one_features;

        ExpectedTimeframeFeatures expected_five_features;

        ExpectedTimeframeFeatures expected_fifteen_features;


        try
        {
            expected_one_features =
                updateExpected(
                    expected_one,
                    snapshot.one_minute
                );


            expected_five_features =
                updateExpected(
                    expected_five,
                    snapshot.five_minute
                );


            expected_fifteen_features =
                updateExpected(
                    expected_fifteen,
                    snapshot.fifteen_minute
                );
        }
        catch (
            const std::exception& error
        )
        {
            ++stats.feature_exceptions;


            std::cerr
                << "Independent validation exception for "
                << symbol
                << ": "
                << error.what()
                << "\n";


            continue;
        }


        // ====================================================================
        // Cross-session continuity
        //
        // A date change must NOT clear continuous rolling histories.
        //
        // The new candle may add one observation, so history may stay the same
        // or increase by one. It must never shrink because of a session reset.
        // ====================================================================

        if (trading_date_changed)
        {
            if (
                expected_one.simple_returns.size() <
                previous_one_simple_size
            )
            {
                ++stats.cross_session_history_violations;
            }


            if (
                expected_five.simple_returns.size() <
                previous_five_simple_size
            )
            {
                ++stats.cross_session_history_violations;
            }


            if (
                expected_fifteen.simple_returns.size() <
                previous_fifteen_simple_size
            )
            {
                ++stats.cross_session_history_violations;
            }
        }


        previous_trading_date =
            snapshot.trading_date;


        // ====================================================================
        // Production Phase 3.4
        // ====================================================================

        VolatilityFeatures actual;


        try
        {
            actual =
                feature_engine.update(
                    snapshot
                );
        }
        catch (
            const std::exception& error
        )
        {
            ++stats.feature_exceptions;


            std::cerr
                << "VolatilityFeatureEngine exception for "
                << symbol
                << " at decision time "
                << decision_time
                << ": "
                << error.what()
                << "\n";


            continue;
        }


        // ====================================================================
        // Production output identity
        // ====================================================================

        if (
            actual.symbol !=
            symbol
        )
        {
            ++stats.symbol_violations;
        }


        if (
            actual.decision_time !=
            decision_time
        )
        {
            ++stats.decision_time_violations;
        }


        // ====================================================================
        // Feature availability counts
        // ====================================================================

        countOneMinuteAvailability(
            actual.one_minute,
            stats
        );


        countFiveMinuteAvailability(
            actual.five_minute,
            stats
        );


        countFifteenMinuteAvailability(
            actual.fifteen_minute,
            stats
        );


        // ====================================================================
        // Numeric integrity
        // ====================================================================

        validateNumericIntegrity(
            actual.one_minute,
            stats
        );


        validateNumericIntegrity(
            actual.five_minute,
            stats
        );


        validateNumericIntegrity(
            actual.fifteen_minute,
            stats
        );


        // ====================================================================
        // Compare against independent calculations
        // ====================================================================

        validateTimeframe(
            actual.one_minute,
            expected_one_features,
            stats
        );


        validateTimeframe(
            actual.five_minute,
            expected_five_features,
            stats
        );


        validateTimeframe(
            actual.fifteen_minute,
            expected_fifteen_features,
            stats
        );


        // ====================================================================
        // Duplicate-state protection
        //
        // If 5m/15m candle is repeated, the engine must continue returning
        // the previously calculated TR rather than NaN or recalculating state.
        // ====================================================================

        if (
            snapshot.five_minute &&
            !expected_five_features.new_candle &&
            expected_five.last_true_range
        )
        {
            if (
                !actual.five_minute.has_true_range ||
                !approximatelyEqual(
                    actual.five_minute.true_range,
                    *expected_five.last_true_range
                )
            )
            {
                ++stats.duplicate_state_violations;
            }
        }


        if (
            snapshot.fifteen_minute &&
            !expected_fifteen_features.new_candle &&
            expected_fifteen.last_true_range
        )
        {
            if (
                !actual.fifteen_minute.has_true_range ||
                !approximatelyEqual(
                    actual.fifteen_minute.true_range,
                    *expected_fifteen.last_true_range
                )
            )
            {
                ++stats.duplicate_state_violations;
            }
        }


        // ====================================================================
        // Overnight-gap validation
        //
        // On a trading-date transition, if the new 1m candle is available and
        // there was already a previous close, the independent TR includes that
        // previous close. Comparing actual vs expected verifies that the
        // production engine did not reset previous_close at the session change.
        // ====================================================================

        if (
            trading_date_changed &&
            snapshot.one_minute &&
            expected_one_features.new_candle &&
            expected_one_features.has_true_range
        )
        {
            if (
                !actual.one_minute.has_true_range ||
                !approximatelyEqual(
                    actual.one_minute.true_range,
                    expected_one_features.true_range
                )
            )
            {
                ++stats.overnight_gap_violations;
            }
        }
    }
}


// ============================================================================
// Print availability
// ============================================================================

void printAvailability(
    const std::string& label,
    std::size_t true_range,
    std::size_t atr14,
    std::size_t stddev20,
    std::size_t stddev50,
    std::size_t realized20,
    std::size_t realized50)
{
    std::cout
        << "\n"
        << label
        << "\n"
        << "------------------------------------------------------------\n"
        << "True Range                      : "
        << true_range
        << "\n"
        << "ATR14                           : "
        << atr14
        << "\n"
        << "Return StdDev 20                : "
        << stddev20
        << "\n"
        << "Return StdDev 50                : "
        << stddev50
        << "\n"
        << "Realized Volatility 20          : "
        << realized20
        << "\n"
        << "Realized Volatility 50          : "
        << realized50
        << "\n";
}

} // namespace


// ============================================================================
// Main
// ============================================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        std::filesystem::path data_folder =
            DEFAULT_DATA_FOLDER;


        if (
            argc >= 2
        )
        {
            data_folder =
                argv[1];
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "CETE PHASE 3.4 - REAL VOLATILITY FEATURE VALIDATOR\n"
            << "============================================================\n"
            << "Data folder : "
            << data_folder
            << "\n"
            << "Mode        : ZERO_LATENCY historical runtime\n"
            << "Scope       : ALL *_1min.txt symbols\n"
            << "============================================================\n";


        // ====================================================================
        // Discover real symbols
        // ====================================================================

        const std::vector<std::string> symbols =
            discoverSymbols(
                data_folder
            );


        if (
            symbols.empty()
        )
        {
            throw std::runtime_error(
                "No *_1min.txt market-data files found."
            );
        }


        std::cout
            << "\n"
            << "Discovered symbols : "
            << symbols.size()
            << "\n";


        ValidationStats stats;


        stats.symbols =
            symbols.size();


        // ====================================================================
        // Validate every symbol
        // ====================================================================

        for (
            std::size_t index = 0;
            index < symbols.size();
            ++index
        )
        {
            std::cout
                << "\n"
                << "["
                << std::setw(3)
                << index + 1
                << "/"
                << symbols.size()
                << "] "
                << symbols[index]
                << "\n";


            try
            {
                validateSymbol(
                    data_folder,
                    symbols[index],
                    stats
                );
            }
            catch (
                const std::exception& error
            )
            {
                ++stats.feature_exceptions;


                std::cerr
                    << "\nERROR validating "
                    << symbols[index]
                    << ": "
                    << error.what()
                    << "\n";
            }
        }


        // ====================================================================
        // Final failure decision
        // ====================================================================

        const bool failed =
            stats.true_range_mismatches != 0 ||
            stats.true_range_percent_mismatches != 0 ||
            stats.atr14_mismatches != 0 ||
            stats.atr14_percent_mismatches != 0 ||
            stats.stddev20_mismatches != 0 ||
            stats.stddev50_mismatches != 0 ||
            stats.realized20_mismatches != 0 ||
            stats.realized50_mismatches != 0 ||
            stats.availability_mismatches != 0 ||
            stats.duplicate_state_violations != 0 ||
            stats.cross_session_history_violations != 0 ||
            stats.overnight_gap_violations != 0 ||
            stats.lookahead_violations != 0 ||
            stats.symbol_violations != 0 ||
            stats.decision_time_violations != 0 ||
            stats.invalid_numeric_values != 0 ||
            stats.feature_exceptions != 0;


        // ====================================================================
        // Report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.4 - ALL-SYMBOL REAL VOLATILITY FEATURE VALIDATION\n"
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
            << stats.incomplete_five_minute_buckets
            << "\n"
            << "Incomplete 15m buckets          : "
            << stats.incomplete_fifteen_minute_buckets
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
            << stats.snapshots_with_one_minute
            << "\n"
            << "Snapshots with 5m               : "
            << stats.snapshots_with_five_minute
            << "\n"
            << "Snapshots with 15m              : "
            << stats.snapshots_with_fifteen_minute
            << "\n";


        printAvailability(
            "1-minute feature availability",
            stats.one_true_range_available,
            stats.one_atr14_available,
            stats.one_stddev20_available,
            stats.one_stddev50_available,
            stats.one_realized20_available,
            stats.one_realized50_available
        );


        printAvailability(
            "5-minute feature availability",
            stats.five_true_range_available,
            stats.five_atr14_available,
            stats.five_stddev20_available,
            stats.five_stddev50_available,
            stats.five_realized20_available,
            stats.five_realized50_available
        );


        printAvailability(
            "15-minute feature availability",
            stats.fifteen_true_range_available,
            stats.fifteen_atr14_available,
            stats.fifteen_stddev20_available,
            stats.fifteen_stddev50_available,
            stats.fifteen_realized20_available,
            stats.fifteen_realized50_available
        );


        std::cout
            << "\n"
            << "Validation\n"
            << "------------------------------------------------------------\n"
            << "True Range mismatches           : "
            << stats.true_range_mismatches
            << "\n"
            << "True Range % mismatches         : "
            << stats.true_range_percent_mismatches
            << "\n"
            << "ATR14 mismatches                : "
            << stats.atr14_mismatches
            << "\n"
            << "ATR14 % mismatches              : "
            << stats.atr14_percent_mismatches
            << "\n"
            << "StdDev20 mismatches             : "
            << stats.stddev20_mismatches
            << "\n"
            << "StdDev50 mismatches             : "
            << stats.stddev50_mismatches
            << "\n"
            << "Realized Vol20 mismatches       : "
            << stats.realized20_mismatches
            << "\n"
            << "Realized Vol50 mismatches       : "
            << stats.realized50_mismatches
            << "\n"
            << "Availability mismatches         : "
            << stats.availability_mismatches
            << "\n"
            << "Duplicate-state violations      : "
            << stats.duplicate_state_violations
            << "\n"
            << "Cross-session history violations: "
            << stats.cross_session_history_violations
            << "\n"
            << "Overnight-gap violations        : "
            << stats.overnight_gap_violations
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


        if (failed)
        {
            std::cerr
                << "\n"
                << "============================================================\n"
                << "PHASE 3.4 REAL VOLATILITY FEATURE VALIDATION FAILED\n"
                << "============================================================\n"
                << "Volatility feature correctness violation detected.\n"
                << "Phase 3.4 must NOT be considered complete.\n"
                << "============================================================\n";


            return 1;
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.4 REAL VOLATILITY FEATURE VALIDATION PASSED\n"
            << "============================================================\n"
            << "True Range / True Range %       : PASSED\n"
            << "ATR14 / ATR14 %                 : PASSED\n"
            << "Return StdDev 20 / 50           : PASSED\n"
            << "Realized Volatility 20 / 50     : PASSED\n"
            << "Duplicate-state protection      : PASSED\n"
            << "Cross-session continuity        : PASSED\n"
            << "Overnight-gap handling          : PASSED\n"
            << "Look-ahead protection           : PASSED\n"
            << "Numeric integrity               : PASSED\n"
            << "============================================================\n";


        return 0;
    }
    catch (
        const std::exception& error
    )
    {
        std::cerr
            << "\n"
            << "PHASE 3.4 REAL VOLATILITY VALIDATION ERROR\n"
            << error.what()
            << "\n";


        return 1;
    }
}