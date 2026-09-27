#include "devai/features/MultiTimeframeFeatureEngine.hpp"
#include "devai/features/MultiTimeframeFeatures.hpp"

#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/PriceReturnFeatures.hpp"

#include "devai/features/TrendMomentumFeatureEngine.hpp"
#include "devai/features/TrendMomentumFeatures.hpp"

#include "devai/features/VolumeFeatureEngine.hpp"
#include "devai/features/VolumeFeatures.hpp"

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


constexpr double NUMERIC_TOLERANCE =
    1e-9;


// ============================================================================
// Validation statistics
// ============================================================================

struct ValidationStats
{
    // ------------------------------------------------------------------------
    // Market data
    // ------------------------------------------------------------------------

    std::size_t symbols{0};

    std::size_t one_minute_candles{0};

    std::size_t five_minute_candles{0};

    std::size_t fifteen_minute_candles{0};

    std::size_t incomplete_five_minute_buckets{0};

    std::size_t incomplete_fifteen_minute_buckets{0};

    std::size_t invalid_rows{0};

    std::size_t duplicate_rows{0};

    std::size_t skipped_rows{0};


    // ------------------------------------------------------------------------
    // Runtime
    // ------------------------------------------------------------------------

    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t new_sessions{0};

    std::size_t snapshots_with_one_minute{0};

    std::size_t snapshots_with_five_minute{0};

    std::size_t snapshots_with_fifteen_minute{0};


    // ------------------------------------------------------------------------
    // Phase 3.5 availability
    // ------------------------------------------------------------------------

    std::size_t return_alignment_full{0};

    std::size_t ema_alignment_full{0};

    std::size_t ema20_slope_alignment_full{0};

    std::size_t rsi_alignment_full{0};

    std::size_t core_features_ready{0};


    std::size_t rvol_five_to_one_available{0};

    std::size_t rvol_fifteen_to_five_available{0};

    std::size_t rvol_fifteen_to_one_available{0};


    std::size_t atr_five_to_one_available{0};

    std::size_t atr_fifteen_to_five_available{0};

    std::size_t atr_fifteen_to_one_available{0};


    std::size_t realized_five_to_one_available{0};

    std::size_t realized_fifteen_to_five_available{0};

    std::size_t realized_fifteen_to_one_available{0};


    std::size_t rsi_five_minus_one_available{0};

    std::size_t rsi_fifteen_minus_five_available{0};

    std::size_t rsi_fifteen_minus_one_available{0};


    // ------------------------------------------------------------------------
    // Directional observations
    // ------------------------------------------------------------------------

    std::size_t return_all_positive{0};

    std::size_t return_all_negative{0};

    std::size_t return_mixed{0};


    std::size_t ema_all_positive{0};

    std::size_t ema_all_negative{0};

    std::size_t ema_mixed{0};


    std::size_t rsi_all_positive{0};

    std::size_t rsi_all_negative{0};

    std::size_t rsi_mixed{0};


    // ------------------------------------------------------------------------
    // Validation failures
    // ------------------------------------------------------------------------

    std::size_t return_alignment_mismatches{0};

    std::size_t ema_alignment_mismatches{0};

    std::size_t ema20_slope_alignment_mismatches{0};

    std::size_t rsi_alignment_mismatches{0};

    std::size_t rvol_ratio_mismatches{0};

    std::size_t atr_ratio_mismatches{0};

    std::size_t realized_volatility_ratio_mismatches{0};

    std::size_t rsi_relationship_mismatches{0};

    std::size_t core_readiness_mismatches{0};

    std::size_t symbol_violations{0};

    std::size_t decision_time_violations{0};

    std::size_t lookahead_violations{0};

    std::size_t invalid_numeric_values{0};

    std::size_t feature_exceptions{0};
};


// ============================================================================
// Numeric comparison
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
// Independent direction calculation
// ============================================================================

FeatureDirection expectedDirection(
    double value)
{
    if (!std::isfinite(value))
    {
        return FeatureDirection::NEUTRAL;
    }


    if (value > 0.0)
    {
        return FeatureDirection::POSITIVE;
    }


    if (value < 0.0)
    {
        return FeatureDirection::NEGATIVE;
    }


    return FeatureDirection::NEUTRAL;
}


FeatureDirection expectedPairDirection(
    double first,
    double second)
{
    if (
        !std::isfinite(first) ||
        !std::isfinite(second)
    )
    {
        return FeatureDirection::NEUTRAL;
    }


    if (first > second)
    {
        return FeatureDirection::POSITIVE;
    }


    if (first < second)
    {
        return FeatureDirection::NEGATIVE;
    }


    return FeatureDirection::NEUTRAL;
}


// ============================================================================
// Independent alignment structure
// ============================================================================

DirectionAlignment expectedAlignment(
    FeatureDirection one,
    bool has_one,
    FeatureDirection five,
    bool has_five,
    FeatureDirection fifteen,
    bool has_fifteen)
{
    DirectionAlignment output;


    output.one_minute =
        has_one
            ? one
            : FeatureDirection::NEUTRAL;


    output.five_minute =
        has_five
            ? five
            : FeatureDirection::NEUTRAL;


    output.fifteen_minute =
        has_fifteen
            ? fifteen
            : FeatureDirection::NEUTRAL;


    output.fully_available =
        has_one &&
        has_five &&
        has_fifteen;


    const auto count =
        [&output](
            FeatureDirection direction,
            bool available)
        {
            if (!available)
            {
                return;
            }


            if (
                direction ==
                FeatureDirection::POSITIVE
            )
            {
                ++output.positive_count;
            }
            else if (
                direction ==
                FeatureDirection::NEGATIVE
            )
            {
                ++output.negative_count;
            }
            else
            {
                ++output.neutral_count;
            }
        };


    count(
        output.one_minute,
        has_one
    );


    count(
        output.five_minute,
        has_five
    );


    count(
        output.fifteen_minute,
        has_fifteen
    );


    output.all_positive =
        output.fully_available &&
        output.positive_count == 3;


    output.all_negative =
        output.fully_available &&
        output.negative_count == 3;


    output.mixed =
        output.positive_count > 0 &&
        output.negative_count > 0;


    return output;
}


// ============================================================================
// Independent ratios
// ============================================================================

CrossTimeframeRatios expectedRatios(
    double one,
    bool has_one,
    double five,
    bool has_five,
    double fifteen,
    bool has_fifteen)
{
    CrossTimeframeRatios output;


    if (
        has_one &&
        has_five &&
        std::isfinite(one) &&
        std::isfinite(five) &&
        one > 0.0
    )
    {
        output.five_to_one =
            five / one;


        output.has_five_to_one =
            std::isfinite(
                output.five_to_one
            );
    }


    if (
        has_five &&
        has_fifteen &&
        std::isfinite(five) &&
        std::isfinite(fifteen) &&
        five > 0.0
    )
    {
        output.fifteen_to_five =
            fifteen / five;


        output.has_fifteen_to_five =
            std::isfinite(
                output.fifteen_to_five
            );
    }


    if (
        has_one &&
        has_fifteen &&
        std::isfinite(one) &&
        std::isfinite(fifteen) &&
        one > 0.0
    )
    {
        output.fifteen_to_one =
            fifteen / one;


        output.has_fifteen_to_one =
            std::isfinite(
                output.fifteen_to_one
            );
    }


    return output;
}


// ============================================================================
// Independent RSI relationships
// ============================================================================

CrossTimeframeRSI expectedRSIRelationship(
    double one,
    bool has_one,
    double five,
    bool has_five,
    double fifteen,
    bool has_fifteen)
{
    CrossTimeframeRSI output;


    if (
        has_one &&
        std::isfinite(one)
    )
    {
        output.one_minute =
            one;


        output.has_one_minute =
            true;
    }


    if (
        has_five &&
        std::isfinite(five)
    )
    {
        output.five_minute =
            five;


        output.has_five_minute =
            true;
    }


    if (
        has_fifteen &&
        std::isfinite(fifteen)
    )
    {
        output.fifteen_minute =
            fifteen;


        output.has_fifteen_minute =
            true;
    }


    if (
        output.has_one_minute &&
        output.has_five_minute
    )
    {
        output.five_minus_one =
            output.five_minute -
            output.one_minute;


        output.has_five_minus_one =
            true;
    }


    if (
        output.has_five_minute &&
        output.has_fifteen_minute
    )
    {
        output.fifteen_minus_five =
            output.fifteen_minute -
            output.five_minute;


        output.has_fifteen_minus_five =
            true;
    }


    if (
        output.has_one_minute &&
        output.has_fifteen_minute
    )
    {
        output.fifteen_minus_one =
            output.fifteen_minute -
            output.one_minute;


        output.has_fifteen_minus_one =
            true;
    }


    return output;
}


// ============================================================================
// Alignment comparison
// ============================================================================

bool sameAlignment(
    const DirectionAlignment& actual,
    const DirectionAlignment& expected)
{
    return
        actual.one_minute ==
            expected.one_minute &&

        actual.five_minute ==
            expected.five_minute &&

        actual.fifteen_minute ==
            expected.fifteen_minute &&

        actual.positive_count ==
            expected.positive_count &&

        actual.negative_count ==
            expected.negative_count &&

        actual.neutral_count ==
            expected.neutral_count &&

        actual.all_positive ==
            expected.all_positive &&

        actual.all_negative ==
            expected.all_negative &&

        actual.mixed ==
            expected.mixed &&

        actual.fully_available ==
            expected.fully_available;
}


// ============================================================================
// Ratio comparison
// ============================================================================

bool sameRatios(
    const CrossTimeframeRatios& actual,
    const CrossTimeframeRatios& expected)
{
    if (
        actual.has_five_to_one !=
        expected.has_five_to_one
    )
    {
        return false;
    }


    if (
        actual.has_fifteen_to_five !=
        expected.has_fifteen_to_five
    )
    {
        return false;
    }


    if (
        actual.has_fifteen_to_one !=
        expected.has_fifteen_to_one
    )
    {
        return false;
    }


    if (
        expected.has_five_to_one &&
        !approximatelyEqual(
            actual.five_to_one,
            expected.five_to_one
        )
    )
    {
        return false;
    }


    if (
        expected.has_fifteen_to_five &&
        !approximatelyEqual(
            actual.fifteen_to_five,
            expected.fifteen_to_five
        )
    )
    {
        return false;
    }


    if (
        expected.has_fifteen_to_one &&
        !approximatelyEqual(
            actual.fifteen_to_one,
            expected.fifteen_to_one
        )
    )
    {
        return false;
    }


    return true;
}


// ============================================================================
// RSI relationship comparison
// ============================================================================

bool sameRSIRelationship(
    const CrossTimeframeRSI& actual,
    const CrossTimeframeRSI& expected)
{
    if (
        actual.has_one_minute !=
        expected.has_one_minute ||
        actual.has_five_minute !=
        expected.has_five_minute ||
        actual.has_fifteen_minute !=
        expected.has_fifteen_minute ||
        actual.has_five_minus_one !=
        expected.has_five_minus_one ||
        actual.has_fifteen_minus_five !=
        expected.has_fifteen_minus_five ||
        actual.has_fifteen_minus_one !=
        expected.has_fifteen_minus_one
    )
    {
        return false;
    }


    if (
        expected.has_one_minute &&
        !approximatelyEqual(
            actual.one_minute,
            expected.one_minute
        )
    )
    {
        return false;
    }


    if (
        expected.has_five_minute &&
        !approximatelyEqual(
            actual.five_minute,
            expected.five_minute
        )
    )
    {
        return false;
    }


    if (
        expected.has_fifteen_minute &&
        !approximatelyEqual(
            actual.fifteen_minute,
            expected.fifteen_minute
        )
    )
    {
        return false;
    }


    if (
        expected.has_five_minus_one &&
        !approximatelyEqual(
            actual.five_minus_one,
            expected.five_minus_one
        )
    )
    {
        return false;
    }


    if (
        expected.has_fifteen_minus_five &&
        !approximatelyEqual(
            actual.fifteen_minus_five,
            expected.fifteen_minus_five
        )
    )
    {
        return false;
    }


    if (
        expected.has_fifteen_minus_one &&
        !approximatelyEqual(
            actual.fifteen_minus_one,
            expected.fifteen_minus_one
        )
    )
    {
        return false;
    }


    return true;
}


// ============================================================================
// Look-ahead check
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
// Numeric integrity
// ============================================================================

void validateRatiosNumeric(
    const CrossTimeframeRatios& ratios,
    ValidationStats& stats)
{
    const auto check =
        [&stats](
            bool available,
            double value)
        {
            if (
                available &&
                !std::isfinite(value)
            )
            {
                ++stats.invalid_numeric_values;
            }
        };


    check(
        ratios.has_five_to_one,
        ratios.five_to_one
    );


    check(
        ratios.has_fifteen_to_five,
        ratios.fifteen_to_five
    );


    check(
        ratios.has_fifteen_to_one,
        ratios.fifteen_to_one
    );
}


void validateRSINumeric(
    const CrossTimeframeRSI& rsi,
    ValidationStats& stats)
{
    const auto check =
        [&stats](
            bool available,
            double value)
        {
            if (
                available &&
                !std::isfinite(value)
            )
            {
                ++stats.invalid_numeric_values;
            }
        };


    check(
        rsi.has_one_minute,
        rsi.one_minute
    );


    check(
        rsi.has_five_minute,
        rsi.five_minute
    );


    check(
        rsi.has_fifteen_minute,
        rsi.fifteen_minute
    );


    check(
        rsi.has_five_minus_one,
        rsi.five_minus_one
    );


    check(
        rsi.has_fifteen_minus_five,
        rsi.fifteen_minus_five
    );


    check(
        rsi.has_fifteen_minus_one,
        rsi.fifteen_minus_one
    );
}


// ============================================================================
// Discover all *_1min.txt symbols
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


    constexpr const char* suffix =
        "_1min.txt";


    const std::string suffix_string =
        suffix;


    std::vector<std::string>
        symbols;


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
            entry.path()
                .filename()
                .string();


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


        symbols.push_back(
            filename.substr(
                0,
                filename.size() -
                    suffix_string.size()
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
// Count availability
// ============================================================================

void countAvailability(
    const MultiTimeframeFeatures& features,
    ValidationStats& stats)
{
    if (
        features.return_alignment.fully_available
    )
    {
        ++stats.return_alignment_full;
    }


    if (
        features.ema_alignment.fully_available
    )
    {
        ++stats.ema_alignment_full;
    }


    if (
        features.ema20_slope_alignment.fully_available
    )
    {
        ++stats.ema20_slope_alignment_full;
    }


    if (
        features.rsi_alignment.fully_available
    )
    {
        ++stats.rsi_alignment_full;
    }


    if (
        features.core_features_ready
    )
    {
        ++stats.core_features_ready;
    }


    if (
        features.rvol20_ratios.has_five_to_one
    )
    {
        ++stats.rvol_five_to_one_available;
    }


    if (
        features.rvol20_ratios.has_fifteen_to_five
    )
    {
        ++stats.rvol_fifteen_to_five_available;
    }


    if (
        features.rvol20_ratios.has_fifteen_to_one
    )
    {
        ++stats.rvol_fifteen_to_one_available;
    }


    if (
        features.atr14_percent_ratios.has_five_to_one
    )
    {
        ++stats.atr_five_to_one_available;
    }


    if (
        features.atr14_percent_ratios.has_fifteen_to_five
    )
    {
        ++stats.atr_fifteen_to_five_available;
    }


    if (
        features.atr14_percent_ratios.has_fifteen_to_one
    )
    {
        ++stats.atr_fifteen_to_one_available;
    }


    if (
        features.realized_volatility_20_ratios.has_five_to_one
    )
    {
        ++stats.realized_five_to_one_available;
    }


    if (
        features.realized_volatility_20_ratios.has_fifteen_to_five
    )
    {
        ++stats.realized_fifteen_to_five_available;
    }


    if (
        features.realized_volatility_20_ratios.has_fifteen_to_one
    )
    {
        ++stats.realized_fifteen_to_one_available;
    }


    if (
        features.rsi_relationship.has_five_minus_one
    )
    {
        ++stats.rsi_five_minus_one_available;
    }


    if (
        features.rsi_relationship.has_fifteen_minus_five
    )
    {
        ++stats.rsi_fifteen_minus_five_available;
    }


    if (
        features.rsi_relationship.has_fifteen_minus_one
    )
    {
        ++stats.rsi_fifteen_minus_one_available;
    }


    // ------------------------------------------------------------------------
    // Descriptive direction statistics
    // ------------------------------------------------------------------------

    if (
        features.return_alignment.all_positive
    )
    {
        ++stats.return_all_positive;
    }


    if (
        features.return_alignment.all_negative
    )
    {
        ++stats.return_all_negative;
    }


    if (
        features.return_alignment.mixed
    )
    {
        ++stats.return_mixed;
    }


    if (
        features.ema_alignment.all_positive
    )
    {
        ++stats.ema_all_positive;
    }


    if (
        features.ema_alignment.all_negative
    )
    {
        ++stats.ema_all_negative;
    }


    if (
        features.ema_alignment.mixed
    )
    {
        ++stats.ema_mixed;
    }


    if (
        features.rsi_alignment.all_positive
    )
    {
        ++stats.rsi_all_positive;
    }


    if (
        features.rsi_alignment.all_negative
    )
    {
        ++stats.rsi_all_negative;
    }


    if (
        features.rsi_alignment.mixed
    )
    {
        ++stats.rsi_mixed;
    }
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
    // Load market data
    // ========================================================================

    MarketDataLoader loader(
        data_folder
    );


    const std::filesystem::path filename =
        data_folder /
        (
            symbol +
            "_1min.txt"
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


    // ========================================================================
    // Aggregate 5m and 15m
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
    // Prepare synchronized runtime streams
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
    // Phase 3 engines
    //
    // IMPORTANT:
    //
    // These engines remain alive for the complete symbol history.
    // They are NOT reset at trading-session boundaries.
    // ========================================================================

    PriceReturnFeatureEngine price_engine(
        symbol,
        64
    );


    TrendMomentumFeatureEngine trend_engine(
        symbol
    );


    VolumeFeatureEngine volume_engine(
        symbol
    );


    VolatilityFeatureEngine volatility_engine(
        symbol
    );


    MultiTimeframeFeatureEngine multi_engine(
        symbol
    );


    // ========================================================================
    // Full real-data runtime
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


        if (
            snapshot.one_minute
        )
        {
            ++stats.snapshots_with_one_minute;
        }


        if (
            snapshot.five_minute
        )
        {
            ++stats.snapshots_with_five_minute;
        }


        if (
            snapshot.fifteen_minute
        )
        {
            ++stats.snapshots_with_fifteen_minute;
        }


        // ====================================================================
        // Snapshot integrity
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
        // Look-ahead validation
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


        try
        {
            // ================================================================
            // Phase 3.1
            // ================================================================

            const PriceReturnFeatures price =
                price_engine.update(
                    snapshot
                );


            // ================================================================
            // Phase 3.2
            // ================================================================

            const TrendMomentumFeatures trend =
                trend_engine.update(
                    snapshot
                );


            // ================================================================
            // Phase 3.3
            // ================================================================

            const VolumeFeatures volume =
                volume_engine.update(
                    snapshot
                );


            // ================================================================
            // Phase 3.4
            // ================================================================

            const VolatilityFeatures volatility =
                volatility_engine.update(
                    snapshot
                );


            // ================================================================
            // Input identity validation
            // ================================================================

            if (
                price.symbol != symbol ||
                trend.symbol != symbol ||
                volume.symbol != symbol ||
                volatility.symbol != symbol
            )
            {
                ++stats.symbol_violations;
            }


            if (
                price.decision_time != decision_time ||
                trend.decision_time != decision_time ||
                volume.decision_time != decision_time ||
                volatility.decision_time != decision_time
            )
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // Phase 3.5 production engine
            // ================================================================

            const MultiTimeframeFeatures actual =
                multi_engine.update(
                    price,
                    trend,
                    volume,
                    volatility
                );


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


            // ================================================================
            // Independent expected RETURN alignment
            //
            // Important:
            // Candle availability alone is not enough.
            // Close-to-close return must also be finite.
            // ================================================================

            const bool has_return_one =
                price.has_one_minute &&
                std::isfinite(
                    price.one_minute_return
                );


            const bool has_return_five =
                price.has_five_minute &&
                std::isfinite(
                    price.five_minute_return
                );


            const bool has_return_fifteen =
                price.has_fifteen_minute &&
                std::isfinite(
                    price.fifteen_minute_return
                );


            const DirectionAlignment expected_return =
                expectedAlignment(
                    expectedDirection(
                        price.one_minute_return
                    ),
                    has_return_one,

                    expectedDirection(
                        price.five_minute_return
                    ),
                    has_return_five,

                    expectedDirection(
                        price.fifteen_minute_return
                    ),
                    has_return_fifteen
                );


            if (
                !sameAlignment(
                    actual.return_alignment,
                    expected_return
                )
            )
            {
                ++stats.return_alignment_mismatches;
            }


            // ================================================================
            // Independent expected EMA alignment
            // ================================================================

            const bool has_ema_one =
                trend.one_minute.has_ema20 &&
                trend.one_minute.has_ema50;


            const bool has_ema_five =
                trend.five_minute.has_ema20 &&
                trend.five_minute.has_ema50;


            const bool has_ema_fifteen =
                trend.fifteen_minute.has_ema20 &&
                trend.fifteen_minute.has_ema50;


            const DirectionAlignment expected_ema =
                expectedAlignment(
                    expectedPairDirection(
                        trend.one_minute.ema20,
                        trend.one_minute.ema50
                    ),
                    has_ema_one,

                    expectedPairDirection(
                        trend.five_minute.ema20,
                        trend.five_minute.ema50
                    ),
                    has_ema_five,

                    expectedPairDirection(
                        trend.fifteen_minute.ema20,
                        trend.fifteen_minute.ema50
                    ),
                    has_ema_fifteen
                );


            if (
                !sameAlignment(
                    actual.ema_alignment,
                    expected_ema
                )
            )
            {
                ++stats.ema_alignment_mismatches;
            }


            // ================================================================
            // Independent expected EMA20 slope alignment
            // ================================================================

            const DirectionAlignment expected_slope =
                expectedAlignment(
                    expectedDirection(
                        trend.one_minute.ema20_slope
                    ),
                    trend.one_minute.has_ema20_slope,

                    expectedDirection(
                        trend.five_minute.ema20_slope
                    ),
                    trend.five_minute.has_ema20_slope,

                    expectedDirection(
                        trend.fifteen_minute.ema20_slope
                    ),
                    trend.fifteen_minute.has_ema20_slope
                );


            if (
                !sameAlignment(
                    actual.ema20_slope_alignment,
                    expected_slope
                )
            )
            {
                ++stats.ema20_slope_alignment_mismatches;
            }


            // ================================================================
            // Independent expected RSI alignment
            // ================================================================

            const DirectionAlignment expected_rsi =
                expectedAlignment(
                    expectedDirection(
                        trend.one_minute.rsi14 -
                        50.0
                    ),
                    trend.one_minute.has_rsi14,

                    expectedDirection(
                        trend.five_minute.rsi14 -
                        50.0
                    ),
                    trend.five_minute.has_rsi14,

                    expectedDirection(
                        trend.fifteen_minute.rsi14 -
                        50.0
                    ),
                    trend.fifteen_minute.has_rsi14
                );


            if (
                !sameAlignment(
                    actual.rsi_alignment,
                    expected_rsi
                )
            )
            {
                ++stats.rsi_alignment_mismatches;
            }


            // ================================================================
            // Independent expected RVOL20 ratios
            // ================================================================

            const CrossTimeframeRatios expected_rvol =
                expectedRatios(
                    volume.one_minute.relative_volume_20,
                    volume.one_minute.has_relative_volume_20,

                    volume.five_minute.relative_volume_20,
                    volume.five_minute.has_relative_volume_20,

                    volume.fifteen_minute.relative_volume_20,
                    volume.fifteen_minute.has_relative_volume_20
                );


            if (
                !sameRatios(
                    actual.rvol20_ratios,
                    expected_rvol
                )
            )
            {
                ++stats.rvol_ratio_mismatches;
            }


            // ================================================================
            // Independent expected ATR14% ratios
            // ================================================================

            const CrossTimeframeRatios expected_atr =
                expectedRatios(
                    volatility.one_minute.atr14_percent,
                    volatility.one_minute.has_atr14_percent,

                    volatility.five_minute.atr14_percent,
                    volatility.five_minute.has_atr14_percent,

                    volatility.fifteen_minute.atr14_percent,
                    volatility.fifteen_minute.has_atr14_percent
                );


            if (
                !sameRatios(
                    actual.atr14_percent_ratios,
                    expected_atr
                )
            )
            {
                ++stats.atr_ratio_mismatches;
            }


            // ================================================================
            // Independent expected realized-volatility ratios
            // ================================================================

            const CrossTimeframeRatios expected_realized =
                expectedRatios(
                    volatility.one_minute.realized_volatility_20,
                    volatility.one_minute.has_realized_volatility_20,

                    volatility.five_minute.realized_volatility_20,
                    volatility.five_minute.has_realized_volatility_20,

                    volatility.fifteen_minute.realized_volatility_20,
                    volatility.fifteen_minute.has_realized_volatility_20
                );


            if (
                !sameRatios(
                    actual.realized_volatility_20_ratios,
                    expected_realized
                )
            )
            {
                ++stats.realized_volatility_ratio_mismatches;
            }


            // ================================================================
            // Independent expected RSI differences
            // ================================================================

            const CrossTimeframeRSI expected_rsi_relationship =
                expectedRSIRelationship(
                    trend.one_minute.rsi14,
                    trend.one_minute.has_rsi14,

                    trend.five_minute.rsi14,
                    trend.five_minute.has_rsi14,

                    trend.fifteen_minute.rsi14,
                    trend.fifteen_minute.has_rsi14
                );


            if (
                !sameRSIRelationship(
                    actual.rsi_relationship,
                    expected_rsi_relationship
                )
            )
            {
                ++stats.rsi_relationship_mismatches;
            }


            // ================================================================
            // Core readiness
            // ================================================================

            const bool expected_core_ready =
                expected_return.fully_available &&
                expected_ema.fully_available &&
                expected_rsi.fully_available;


            if (
                actual.core_features_ready !=
                expected_core_ready
            )
            {
                ++stats.core_readiness_mismatches;
            }


            // ================================================================
            // Numeric integrity
            // ================================================================

            validateRatiosNumeric(
                actual.rvol20_ratios,
                stats
            );


            validateRatiosNumeric(
                actual.atr14_percent_ratios,
                stats
            );


            validateRatiosNumeric(
                actual.realized_volatility_20_ratios,
                stats
            );


            validateRSINumeric(
                actual.rsi_relationship,
                stats
            );


            // ================================================================
            // Availability statistics
            // ================================================================

            countAvailability(
                actual,
                stats
            );
        }
        catch (
            const std::exception& error
        )
        {
            ++stats.feature_exceptions;


            std::cerr
                << "Feature exception for "
                << symbol
                << " at decision time "
                << decision_time
                << ": "
                << error.what()
                << "\n";
        }
    }
}


// ============================================================================
// Main
// ============================================================================

} // namespace


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
            << "CETE PHASE 3.5 - REAL MULTI-TIMEFRAME FEATURE VALIDATOR\n"
            << "============================================================\n"
            << "Data folder : "
            << data_folder
            << "\n"
            << "Mode        : ZERO_LATENCY historical runtime\n"
            << "Scope       : ALL *_1min.txt symbols\n"
            << "============================================================\n";


        const std::vector<std::string> symbols =
            discoverSymbols(
                data_folder
            );


        if (
            symbols.empty()
        )
        {
            throw std::runtime_error(
                "No *_1min.txt files discovered."
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
                    << "ERROR validating "
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
            stats.return_alignment_mismatches != 0 ||
            stats.ema_alignment_mismatches != 0 ||
            stats.ema20_slope_alignment_mismatches != 0 ||
            stats.rsi_alignment_mismatches != 0 ||
            stats.rvol_ratio_mismatches != 0 ||
            stats.atr_ratio_mismatches != 0 ||
            stats.realized_volatility_ratio_mismatches != 0 ||
            stats.rsi_relationship_mismatches != 0 ||
            stats.core_readiness_mismatches != 0 ||
            stats.symbol_violations != 0 ||
            stats.decision_time_violations != 0 ||
            stats.lookahead_violations != 0 ||
            stats.invalid_numeric_values != 0 ||
            stats.feature_exceptions != 0;


        // ====================================================================
        // Report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.5 - ALL-SYMBOL REAL MULTI-TIMEFRAME VALIDATION\n"
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
            << "\n"

            << "\n"
            << "Core feature availability\n"
            << "------------------------------------------------------------\n"
            << "Return alignment fully available: "
            << stats.return_alignment_full
            << "\n"
            << "EMA alignment fully available   : "
            << stats.ema_alignment_full
            << "\n"
            << "EMA20 slope fully available     : "
            << stats.ema20_slope_alignment_full
            << "\n"
            << "RSI alignment fully available   : "
            << stats.rsi_alignment_full
            << "\n"
            << "Core features ready             : "
            << stats.core_features_ready
            << "\n"

            << "\n"
            << "RVOL20 ratio availability\n"
            << "------------------------------------------------------------\n"
            << "5m / 1m                         : "
            << stats.rvol_five_to_one_available
            << "\n"
            << "15m / 5m                        : "
            << stats.rvol_fifteen_to_five_available
            << "\n"
            << "15m / 1m                        : "
            << stats.rvol_fifteen_to_one_available
            << "\n"

            << "\n"
            << "ATR14% ratio availability\n"
            << "------------------------------------------------------------\n"
            << "5m / 1m                         : "
            << stats.atr_five_to_one_available
            << "\n"
            << "15m / 5m                        : "
            << stats.atr_fifteen_to_five_available
            << "\n"
            << "15m / 1m                        : "
            << stats.atr_fifteen_to_one_available
            << "\n"

            << "\n"
            << "Realized Volatility 20 ratio availability\n"
            << "------------------------------------------------------------\n"
            << "5m / 1m                         : "
            << stats.realized_five_to_one_available
            << "\n"
            << "15m / 5m                        : "
            << stats.realized_fifteen_to_five_available
            << "\n"
            << "15m / 1m                        : "
            << stats.realized_fifteen_to_one_available
            << "\n"

            << "\n"
            << "RSI relationship availability\n"
            << "------------------------------------------------------------\n"
            << "5m - 1m                         : "
            << stats.rsi_five_minus_one_available
            << "\n"
            << "15m - 5m                        : "
            << stats.rsi_fifteen_minus_five_available
            << "\n"
            << "15m - 1m                        : "
            << stats.rsi_fifteen_minus_one_available
            << "\n"

            << "\n"
            << "Descriptive alignment observations\n"
            << "------------------------------------------------------------\n"
            << "Return all positive             : "
            << stats.return_all_positive
            << "\n"
            << "Return all negative             : "
            << stats.return_all_negative
            << "\n"
            << "Return mixed                    : "
            << stats.return_mixed
            << "\n"
            << "EMA all positive                : "
            << stats.ema_all_positive
            << "\n"
            << "EMA all negative                : "
            << stats.ema_all_negative
            << "\n"
            << "EMA mixed                       : "
            << stats.ema_mixed
            << "\n"
            << "RSI all positive                : "
            << stats.rsi_all_positive
            << "\n"
            << "RSI all negative                : "
            << stats.rsi_all_negative
            << "\n"
            << "RSI mixed                       : "
            << stats.rsi_mixed
            << "\n"

            << "\n"
            << "Validation\n"
            << "------------------------------------------------------------\n"
            << "Return alignment mismatches     : "
            << stats.return_alignment_mismatches
            << "\n"
            << "EMA alignment mismatches        : "
            << stats.ema_alignment_mismatches
            << "\n"
            << "EMA20 slope mismatches          : "
            << stats.ema20_slope_alignment_mismatches
            << "\n"
            << "RSI alignment mismatches        : "
            << stats.rsi_alignment_mismatches
            << "\n"
            << "RVOL20 ratio mismatches         : "
            << stats.rvol_ratio_mismatches
            << "\n"
            << "ATR14% ratio mismatches         : "
            << stats.atr_ratio_mismatches
            << "\n"
            << "Realized Vol20 ratio mismatches : "
            << stats.realized_volatility_ratio_mismatches
            << "\n"
            << "RSI relationship mismatches     : "
            << stats.rsi_relationship_mismatches
            << "\n"
            << "Core readiness mismatches       : "
            << stats.core_readiness_mismatches
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
                << "PHASE 3.5 REAL MULTI-TIMEFRAME FEATURE VALIDATION FAILED\n"
                << "============================================================\n"
                << "Phase 3.5 must NOT be considered complete.\n"
                << "============================================================\n";


            return 1;
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.5 REAL MULTI-TIMEFRAME FEATURE VALIDATION PASSED\n"
            << "============================================================\n"
            << "Return alignment                : PASSED\n"
            << "EMA alignment                   : PASSED\n"
            << "EMA20 slope alignment           : PASSED\n"
            << "RSI alignment                   : PASSED\n"
            << "RVOL20 relationships            : PASSED\n"
            << "ATR14% relationships            : PASSED\n"
            << "Realized volatility relationships: PASSED\n"
            << "RSI timeframe relationships     : PASSED\n"
            << "Warm-up availability            : PASSED\n"
            << "Look-ahead protection           : PASSED\n"
            << "Symbol / decision-time integrity: PASSED\n"
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
            << "PHASE 3.5 REAL VALIDATION ERROR\n"
            << error.what()
            << "\n";


        return 1;
    }
}