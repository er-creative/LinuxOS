#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/PriceReturnFeatures.hpp"

#include "devai/features/TrendMomentumFeatureEngine.hpp"
#include "devai/features/TrendMomentumFeatures.hpp"

#include "devai/features/VolatilityFeatureEngine.hpp"
#include "devai/features/VolatilityFeatures.hpp"

#include "devai/features/StockBenchmarkFeatureEngine.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/StockBenchmarkSnapshot.hpp"
#include "devai/market/StockBenchmarkSynchronizer.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
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


const std::string BENCHMARK_SYMBOL =
    "NIFTY%2050";


constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;


constexpr double NUMERIC_TOLERANCE =
    1e-9;


// ============================================================================
// Prepared runtime
// ============================================================================

struct PreparedRuntime
{
    std::vector<Candle> one_minute;
    std::vector<Candle> five_minute;
    std::vector<Candle> fifteen_minute;

    std::vector<TimedCandle> timed_one;
    std::vector<TimedCandle> timed_five;
    std::vector<TimedCandle> timed_fifteen;

    std::size_t incomplete_five_minute_buckets{0};
    std::size_t incomplete_fifteen_minute_buckets{0};

    std::size_t invalid_rows{0};
    std::size_t duplicate_rows{0};
    std::size_t skipped_rows{0};
};


// ============================================================================
// Validation statistics
// ============================================================================

struct ValidationStats
{
    // Universe

    std::size_t stock_symbols{0};


    // Market data

    std::size_t stock_one_minute_candles{0};
    std::size_t stock_five_minute_candles{0};
    std::size_t stock_fifteen_minute_candles{0};

    std::size_t stock_incomplete_five_minute_buckets{0};
    std::size_t stock_incomplete_fifteen_minute_buckets{0};

    std::size_t stock_invalid_rows{0};
    std::size_t stock_duplicate_rows{0};
    std::size_t stock_skipped_rows{0};


    // Runtime

    std::size_t decision_points{0};

    std::size_t synchronized_pairs{0};
    std::size_t incomplete_pairs{0};

    std::size_t alignment_rejections{0};
    std::size_t unexpected_runtime_exceptions{0};

    std::size_t new_stock_sessions{0};
    std::size_t new_benchmark_sessions{0};


    // Feature availability

    std::size_t relative_return_one_available{0};
    std::size_t relative_return_five_available{0};
    std::size_t relative_return_fifteen_available{0};

    std::size_t relative_rsi_one_available{0};
    std::size_t relative_rsi_five_available{0};
    std::size_t relative_rsi_fifteen_available{0};

    std::size_t relative_ema_one_available{0};
    std::size_t relative_ema_five_available{0};
    std::size_t relative_ema_fifteen_available{0};

    std::size_t relative_ema20_slope_one_available{0};
    std::size_t relative_ema20_slope_five_available{0};
    std::size_t relative_ema20_slope_fifteen_available{0};

    std::size_t relative_ema50_slope_one_available{0};
    std::size_t relative_ema50_slope_five_available{0};
    std::size_t relative_ema50_slope_fifteen_available{0};

    std::size_t relative_atr_one_available{0};
    std::size_t relative_atr_five_available{0};
    std::size_t relative_atr_fifteen_available{0};

    std::size_t relative_realized_one_available{0};
    std::size_t relative_realized_five_available{0};
    std::size_t relative_realized_fifteen_available{0};

    std::size_t core_features_ready{0};


    // Descriptive observations

    std::size_t positive_relative_return_one{0};
    std::size_t negative_relative_return_one{0};

    std::size_t positive_relative_return_five{0};
    std::size_t negative_relative_return_five{0};

    std::size_t positive_relative_return_fifteen{0};
    std::size_t negative_relative_return_fifteen{0};


    // Validation failures

    std::size_t relative_return_mismatches{0};
    std::size_t relative_rsi_mismatches{0};
    std::size_t relative_ema_mismatches{0};

    std::size_t relative_ema20_slope_mismatches{0};
    std::size_t relative_ema50_slope_mismatches{0};

    std::size_t relative_atr_mismatches{0};
    std::size_t relative_realized_mismatches{0};

    std::size_t core_readiness_mismatches{0};

    std::size_t symbol_violations{0};
    std::size_t decision_time_violations{0};

    std::size_t invalid_numeric_values{0};
    std::size_t feature_exceptions{0};
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
        std::fabs(left - right) <=
        tolerance * scale;
}


double expectedDifference(
    double stock,
    double benchmark)
{
    if (
        !std::isfinite(stock) ||
        !std::isfinite(benchmark)
    )
    {
        return
            std::numeric_limits<double>::quiet_NaN();
    }


    return stock - benchmark;
}


double expectedRatio(
    double stock,
    double benchmark)
{
    if (
        !std::isfinite(stock) ||
        !std::isfinite(benchmark) ||
        benchmark <= 0.0
    )
    {
        return
            std::numeric_limits<double>::quiet_NaN();
    }


    const double value =
        stock / benchmark;


    if (!std::isfinite(value))
    {
        return
            std::numeric_limits<double>::quiet_NaN();
    }


    return value;
}


double expectedNormalizedEMASpread(
    double ema20,
    double ema50)
{
    if (
        !std::isfinite(ema20) ||
        !std::isfinite(ema50) ||
        ema50 == 0.0
    )
    {
        return
            std::numeric_limits<double>::quiet_NaN();
    }


    return
        (ema20 - ema50) /
        ema50;
}


// ============================================================================
// Difference validation
// ============================================================================

bool validateDifferenceValue(
    double actual,
    bool actual_available,

    double stock,
    bool stock_available,

    double benchmark,
    bool benchmark_available)
{
    const bool expected_available =
        stock_available &&
        benchmark_available &&
        std::isfinite(stock) &&
        std::isfinite(benchmark);


    if (
        actual_available !=
        expected_available
    )
    {
        return false;
    }


    if (!expected_available)
    {
        return true;
    }


    return
        approximatelyEqual(
            actual,
            expectedDifference(
                stock,
                benchmark
            )
        );
}


// ============================================================================
// Ratio validation
// ============================================================================

bool validateRatioValue(
    double actual,
    bool actual_available,

    double stock,
    bool stock_available,

    double benchmark,
    bool benchmark_available)
{
    const bool expected_available =
        stock_available &&
        benchmark_available &&
        std::isfinite(stock) &&
        std::isfinite(benchmark) &&
        benchmark > 0.0;


    if (
        actual_available !=
        expected_available
    )
    {
        return false;
    }


    if (!expected_available)
    {
        return true;
    }


    return
        approximatelyEqual(
            actual,
            expectedRatio(
                stock,
                benchmark
            )
        );
}


// ============================================================================
// EMA relationship validation
// ============================================================================

bool validateEMARelationshipValue(
    double actual,
    bool actual_available,

    double stock_ema20,
    bool stock_has_ema20,

    double stock_ema50,
    bool stock_has_ema50,

    double benchmark_ema20,
    bool benchmark_has_ema20,

    double benchmark_ema50,
    bool benchmark_has_ema50)
{
    const bool inputs_available =
        stock_has_ema20 &&
        stock_has_ema50 &&
        benchmark_has_ema20 &&
        benchmark_has_ema50;


    double expected =
        std::numeric_limits<double>::quiet_NaN();


    if (inputs_available)
    {
        const double stock_spread =
            expectedNormalizedEMASpread(
                stock_ema20,
                stock_ema50
            );


        const double benchmark_spread =
            expectedNormalizedEMASpread(
                benchmark_ema20,
                benchmark_ema50
            );


        expected =
            expectedDifference(
                stock_spread,
                benchmark_spread
            );
    }


    const bool expected_available =
        std::isfinite(expected);


    if (
        actual_available !=
        expected_available
    )
    {
        return false;
    }


    if (!expected_available)
    {
        return true;
    }


    return
        approximatelyEqual(
            actual,
            expected
        );
}


// ============================================================================
// Numeric integrity
// ============================================================================

template <typename RelativeFeature>
void validateRelativeNumeric(
    const RelativeFeature& feature,
    ValidationStats& stats)
{
    if (
        feature.has_one_minute &&
        !std::isfinite(feature.one_minute)
    )
    {
        ++stats.invalid_numeric_values;
    }


    if (
        feature.has_five_minute &&
        !std::isfinite(feature.five_minute)
    )
    {
        ++stats.invalid_numeric_values;
    }


    if (
        feature.has_fifteen_minute &&
        !std::isfinite(feature.fifteen_minute)
    )
    {
        ++stats.invalid_numeric_values;
    }
}


// ============================================================================
// Discover stock symbols
// ============================================================================

std::vector<std::string> discoverStockSymbols(
    const std::filesystem::path& data_folder)
{
    if (!std::filesystem::exists(data_folder))
    {
        throw std::runtime_error(
            "Market-data folder does not exist: " +
            data_folder.string()
        );
    }


    const std::string suffix =
        "_1min.txt";


    std::vector<std::string> symbols;


    for (
        const auto& entry :
        std::filesystem::directory_iterator(data_folder)
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


        if (filename.size() <= suffix.size())
        {
            continue;
        }


        if (
            filename.compare(
                filename.size() - suffix.size(),
                suffix.size(),
                suffix
            ) != 0
        )
        {
            continue;
        }


        const std::string symbol =
            filename.substr(
                0,
                filename.size() - suffix.size()
            );


        if (symbol == BENCHMARK_SYMBOL)
        {
            continue;
        }


        symbols.push_back(symbol);
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
// Prepare runtime
// ============================================================================

PreparedRuntime prepareRuntime(
    const std::filesystem::path& data_folder,
    const std::string& symbol)
{
    MarketDataLoader loader(
        data_folder
    );


    const auto load_result =
        loader.loadPath(
            data_folder /
            (symbol + "_1min.txt")
        );


    if (load_result.candles.empty())
    {
        throw std::runtime_error(
            "No candles loaded for " +
            symbol
        );
    }


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


    MultiTimeframeSynchronizerConfig config;

    config.mode =
        AvailabilityMode::ZERO_LATENCY;


    MultiTimeframeSynchronizer synchronizer(
        config
    );


    PreparedRuntime output;


    output.one_minute =
        load_result.candles;

    output.five_minute =
        five_result.candles;

    output.fifteen_minute =
        fifteen_result.candles;


    output.timed_one =
        synchronizer.prepareOneMinute(
            output.one_minute
        );


    output.timed_five =
        synchronizer.prepareFiveMinute(
            output.five_minute
        );


    output.timed_fifteen =
        synchronizer.prepareFifteenMinute(
            output.fifteen_minute
        );


    output.incomplete_five_minute_buckets =
        five_result.incomplete_buckets;


    output.incomplete_fifteen_minute_buckets =
        fifteen_result.incomplete_buckets;


    output.invalid_rows =
        load_result.invalid_rows;


    output.duplicate_rows =
        load_result.duplicate_rows;


    output.skipped_rows =
        load_result.skipped_rows;


    return output;
}


// ============================================================================
// Availability counts
// ============================================================================

void countAvailability(
    const StockBenchmarkFeatures& feature,
    ValidationStats& stats)
{
    // Returns

    if (feature.relative_return.has_one_minute)
    {
        ++stats.relative_return_one_available;

        if (feature.relative_return.one_minute > 0.0)
        {
            ++stats.positive_relative_return_one;
        }
        else if (feature.relative_return.one_minute < 0.0)
        {
            ++stats.negative_relative_return_one;
        }
    }


    if (feature.relative_return.has_five_minute)
    {
        ++stats.relative_return_five_available;

        if (feature.relative_return.five_minute > 0.0)
        {
            ++stats.positive_relative_return_five;
        }
        else if (feature.relative_return.five_minute < 0.0)
        {
            ++stats.negative_relative_return_five;
        }
    }


    if (feature.relative_return.has_fifteen_minute)
    {
        ++stats.relative_return_fifteen_available;

        if (feature.relative_return.fifteen_minute > 0.0)
        {
            ++stats.positive_relative_return_fifteen;
        }
        else if (feature.relative_return.fifteen_minute < 0.0)
        {
            ++stats.negative_relative_return_fifteen;
        }
    }


    // RSI

    if (feature.relative_rsi14.has_one_minute)
    {
        ++stats.relative_rsi_one_available;
    }

    if (feature.relative_rsi14.has_five_minute)
    {
        ++stats.relative_rsi_five_available;
    }

    if (feature.relative_rsi14.has_fifteen_minute)
    {
        ++stats.relative_rsi_fifteen_available;
    }


    // EMA relationship

    if (feature.relative_ema_relationship.has_one_minute)
    {
        ++stats.relative_ema_one_available;
    }

    if (feature.relative_ema_relationship.has_five_minute)
    {
        ++stats.relative_ema_five_available;
    }

    if (feature.relative_ema_relationship.has_fifteen_minute)
    {
        ++stats.relative_ema_fifteen_available;
    }


    // EMA20 slope

    if (feature.relative_ema20_slope.has_one_minute)
    {
        ++stats.relative_ema20_slope_one_available;
    }

    if (feature.relative_ema20_slope.has_five_minute)
    {
        ++stats.relative_ema20_slope_five_available;
    }

    if (feature.relative_ema20_slope.has_fifteen_minute)
    {
        ++stats.relative_ema20_slope_fifteen_available;
    }


    // EMA50 slope

    if (feature.relative_ema50_slope.has_one_minute)
    {
        ++stats.relative_ema50_slope_one_available;
    }

    if (feature.relative_ema50_slope.has_five_minute)
    {
        ++stats.relative_ema50_slope_five_available;
    }

    if (feature.relative_ema50_slope.has_fifteen_minute)
    {
        ++stats.relative_ema50_slope_fifteen_available;
    }


    // ATR14%

    if (feature.relative_atr14_percent.has_one_minute)
    {
        ++stats.relative_atr_one_available;
    }

    if (feature.relative_atr14_percent.has_five_minute)
    {
        ++stats.relative_atr_five_available;
    }

    if (feature.relative_atr14_percent.has_fifteen_minute)
    {
        ++stats.relative_atr_fifteen_available;
    }


    // Realized volatility

    if (feature.relative_realized_volatility_20.has_one_minute)
    {
        ++stats.relative_realized_one_available;
    }

    if (feature.relative_realized_volatility_20.has_five_minute)
    {
        ++stats.relative_realized_five_available;
    }

    if (feature.relative_realized_volatility_20.has_fifteen_minute)
    {
        ++stats.relative_realized_fifteen_available;
    }


    if (feature.core_features_ready)
    {
        ++stats.core_features_ready;
    }
}


// ============================================================================
// Independently validate Phase 3.6
// ============================================================================

void validateOutput(
    const StockBenchmarkFeatures& actual,

    const PriceReturnFeatures& stock_price,
    const TrendMomentumFeatures& stock_trend,
    const VolatilityFeatures& stock_volatility,

    const PriceReturnFeatures& benchmark_price,
    const TrendMomentumFeatures& benchmark_trend,
    const VolatilityFeatures& benchmark_volatility,

    ValidationStats& stats)
{
    // ========================================================================
    // A. Relative returns
    // ========================================================================

    bool mismatch = false;


    mismatch |=
        !validateDifferenceValue(
            actual.relative_return.one_minute,
            actual.relative_return.has_one_minute,

            stock_price.one_minute_return,
            stock_price.has_one_minute &&
                std::isfinite(stock_price.one_minute_return),

            benchmark_price.one_minute_return,
            benchmark_price.has_one_minute &&
                std::isfinite(benchmark_price.one_minute_return)
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_return.five_minute,
            actual.relative_return.has_five_minute,

            stock_price.five_minute_return,
            stock_price.has_five_minute &&
                std::isfinite(stock_price.five_minute_return),

            benchmark_price.five_minute_return,
            benchmark_price.has_five_minute &&
                std::isfinite(benchmark_price.five_minute_return)
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_return.fifteen_minute,
            actual.relative_return.has_fifteen_minute,

            stock_price.fifteen_minute_return,
            stock_price.has_fifteen_minute &&
                std::isfinite(stock_price.fifteen_minute_return),

            benchmark_price.fifteen_minute_return,
            benchmark_price.has_fifteen_minute &&
                std::isfinite(benchmark_price.fifteen_minute_return)
        );


    if (mismatch)
    {
        ++stats.relative_return_mismatches;
    }


    // ========================================================================
    // B1. Relative RSI14
    // ========================================================================

    mismatch = false;


    mismatch |=
        !validateDifferenceValue(
            actual.relative_rsi14.one_minute,
            actual.relative_rsi14.has_one_minute,

            stock_trend.one_minute.rsi14,
            stock_trend.one_minute.has_rsi14,

            benchmark_trend.one_minute.rsi14,
            benchmark_trend.one_minute.has_rsi14
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_rsi14.five_minute,
            actual.relative_rsi14.has_five_minute,

            stock_trend.five_minute.rsi14,
            stock_trend.five_minute.has_rsi14,

            benchmark_trend.five_minute.rsi14,
            benchmark_trend.five_minute.has_rsi14
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_rsi14.fifteen_minute,
            actual.relative_rsi14.has_fifteen_minute,

            stock_trend.fifteen_minute.rsi14,
            stock_trend.fifteen_minute.has_rsi14,

            benchmark_trend.fifteen_minute.rsi14,
            benchmark_trend.fifteen_minute.has_rsi14
        );


    if (mismatch)
    {
        ++stats.relative_rsi_mismatches;
    }


    // ========================================================================
    // B2. Relative EMA relationship
    // ========================================================================

    mismatch = false;


    mismatch |=
        !validateEMARelationshipValue(
            actual.relative_ema_relationship.one_minute,
            actual.relative_ema_relationship.has_one_minute,

            stock_trend.one_minute.ema20,
            stock_trend.one_minute.has_ema20,

            stock_trend.one_minute.ema50,
            stock_trend.one_minute.has_ema50,

            benchmark_trend.one_minute.ema20,
            benchmark_trend.one_minute.has_ema20,

            benchmark_trend.one_minute.ema50,
            benchmark_trend.one_minute.has_ema50
        );


    mismatch |=
        !validateEMARelationshipValue(
            actual.relative_ema_relationship.five_minute,
            actual.relative_ema_relationship.has_five_minute,

            stock_trend.five_minute.ema20,
            stock_trend.five_minute.has_ema20,

            stock_trend.five_minute.ema50,
            stock_trend.five_minute.has_ema50,

            benchmark_trend.five_minute.ema20,
            benchmark_trend.five_minute.has_ema20,

            benchmark_trend.five_minute.ema50,
            benchmark_trend.five_minute.has_ema50
        );


    mismatch |=
        !validateEMARelationshipValue(
            actual.relative_ema_relationship.fifteen_minute,
            actual.relative_ema_relationship.has_fifteen_minute,

            stock_trend.fifteen_minute.ema20,
            stock_trend.fifteen_minute.has_ema20,

            stock_trend.fifteen_minute.ema50,
            stock_trend.fifteen_minute.has_ema50,

            benchmark_trend.fifteen_minute.ema20,
            benchmark_trend.fifteen_minute.has_ema20,

            benchmark_trend.fifteen_minute.ema50,
            benchmark_trend.fifteen_minute.has_ema50
        );


    if (mismatch)
    {
        ++stats.relative_ema_mismatches;
    }


    // ========================================================================
    // B3. Relative EMA20 slope
    // ========================================================================

    mismatch = false;


    mismatch |=
        !validateDifferenceValue(
            actual.relative_ema20_slope.one_minute,
            actual.relative_ema20_slope.has_one_minute,

            stock_trend.one_minute.ema20_slope,
            stock_trend.one_minute.has_ema20_slope,

            benchmark_trend.one_minute.ema20_slope,
            benchmark_trend.one_minute.has_ema20_slope
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_ema20_slope.five_minute,
            actual.relative_ema20_slope.has_five_minute,

            stock_trend.five_minute.ema20_slope,
            stock_trend.five_minute.has_ema20_slope,

            benchmark_trend.five_minute.ema20_slope,
            benchmark_trend.five_minute.has_ema20_slope
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_ema20_slope.fifteen_minute,
            actual.relative_ema20_slope.has_fifteen_minute,

            stock_trend.fifteen_minute.ema20_slope,
            stock_trend.fifteen_minute.has_ema20_slope,

            benchmark_trend.fifteen_minute.ema20_slope,
            benchmark_trend.fifteen_minute.has_ema20_slope
        );


    if (mismatch)
    {
        ++stats.relative_ema20_slope_mismatches;
    }


    // ========================================================================
    // B4. Relative EMA50 slope
    // ========================================================================

    mismatch = false;


    mismatch |=
        !validateDifferenceValue(
            actual.relative_ema50_slope.one_minute,
            actual.relative_ema50_slope.has_one_minute,

            stock_trend.one_minute.ema50_slope,
            stock_trend.one_minute.has_ema50_slope,

            benchmark_trend.one_minute.ema50_slope,
            benchmark_trend.one_minute.has_ema50_slope
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_ema50_slope.five_minute,
            actual.relative_ema50_slope.has_five_minute,

            stock_trend.five_minute.ema50_slope,
            stock_trend.five_minute.has_ema50_slope,

            benchmark_trend.five_minute.ema50_slope,
            benchmark_trend.five_minute.has_ema50_slope
        );


    mismatch |=
        !validateDifferenceValue(
            actual.relative_ema50_slope.fifteen_minute,
            actual.relative_ema50_slope.has_fifteen_minute,

            stock_trend.fifteen_minute.ema50_slope,
            stock_trend.fifteen_minute.has_ema50_slope,

            benchmark_trend.fifteen_minute.ema50_slope,
            benchmark_trend.fifteen_minute.has_ema50_slope
        );


    if (mismatch)
    {
        ++stats.relative_ema50_slope_mismatches;
    }


    // ========================================================================
    // D1. Relative ATR14%
    // ========================================================================

    mismatch = false;


    mismatch |=
        !validateRatioValue(
            actual.relative_atr14_percent.one_minute,
            actual.relative_atr14_percent.has_one_minute,

            stock_volatility.one_minute.atr14_percent,
            stock_volatility.one_minute.has_atr14_percent,

            benchmark_volatility.one_minute.atr14_percent,
            benchmark_volatility.one_minute.has_atr14_percent
        );


    mismatch |=
        !validateRatioValue(
            actual.relative_atr14_percent.five_minute,
            actual.relative_atr14_percent.has_five_minute,

            stock_volatility.five_minute.atr14_percent,
            stock_volatility.five_minute.has_atr14_percent,

            benchmark_volatility.five_minute.atr14_percent,
            benchmark_volatility.five_minute.has_atr14_percent
        );


    mismatch |=
        !validateRatioValue(
            actual.relative_atr14_percent.fifteen_minute,
            actual.relative_atr14_percent.has_fifteen_minute,

            stock_volatility.fifteen_minute.atr14_percent,
            stock_volatility.fifteen_minute.has_atr14_percent,

            benchmark_volatility.fifteen_minute.atr14_percent,
            benchmark_volatility.fifteen_minute.has_atr14_percent
        );


    if (mismatch)
    {
        ++stats.relative_atr_mismatches;
    }


    // ========================================================================
    // D2. Relative realized volatility 20
    // ========================================================================

    mismatch = false;


    mismatch |=
        !validateRatioValue(
            actual.relative_realized_volatility_20.one_minute,
            actual.relative_realized_volatility_20.has_one_minute,

            stock_volatility.one_minute.realized_volatility_20,
            stock_volatility.one_minute.has_realized_volatility_20,

            benchmark_volatility.one_minute.realized_volatility_20,
            benchmark_volatility.one_minute.has_realized_volatility_20
        );


    mismatch |=
        !validateRatioValue(
            actual.relative_realized_volatility_20.five_minute,
            actual.relative_realized_volatility_20.has_five_minute,

            stock_volatility.five_minute.realized_volatility_20,
            stock_volatility.five_minute.has_realized_volatility_20,

            benchmark_volatility.five_minute.realized_volatility_20,
            benchmark_volatility.five_minute.has_realized_volatility_20
        );


    mismatch |=
        !validateRatioValue(
            actual.relative_realized_volatility_20.fifteen_minute,
            actual.relative_realized_volatility_20.has_fifteen_minute,

            stock_volatility.fifteen_minute.realized_volatility_20,
            stock_volatility.fifteen_minute.has_realized_volatility_20,

            benchmark_volatility.fifteen_minute.realized_volatility_20,
            benchmark_volatility.fifteen_minute.has_realized_volatility_20
        );


    if (mismatch)
    {
        ++stats.relative_realized_mismatches;
    }


    // ========================================================================
    // Core readiness
    // ========================================================================

    const bool expected_core_ready =
        actual.relative_return.has_one_minute &&
        actual.relative_return.has_five_minute &&
        actual.relative_return.has_fifteen_minute &&

        actual.relative_rsi14.has_one_minute &&
        actual.relative_rsi14.has_five_minute &&
        actual.relative_rsi14.has_fifteen_minute &&

        actual.relative_ema_relationship.has_one_minute &&
        actual.relative_ema_relationship.has_five_minute &&
        actual.relative_ema_relationship.has_fifteen_minute;


    if (
        actual.core_features_ready !=
        expected_core_ready
    )
    {
        ++stats.core_readiness_mismatches;
    }


    // ========================================================================
    // Numeric integrity
    // ========================================================================

    validateRelativeNumeric(
        actual.relative_return,
        stats
    );


    validateRelativeNumeric(
        actual.relative_rsi14,
        stats
    );


    validateRelativeNumeric(
        actual.relative_ema_relationship,
        stats
    );


    validateRelativeNumeric(
        actual.relative_ema20_slope,
        stats
    );


    validateRelativeNumeric(
        actual.relative_ema50_slope,
        stats
    );


    validateRelativeNumeric(
        actual.relative_atr14_percent,
        stats
    );


    validateRelativeNumeric(
        actual.relative_realized_volatility_20,
        stats
    );
}


// ============================================================================
// Validate one stock against NIFTY
// ============================================================================

void validateStock(
    const std::filesystem::path& data_folder,
    const std::string& stock_symbol,
    const PreparedRuntime& benchmark_runtime,
    ValidationStats& stats)
{
    std::cout
        << "\n"
        << "------------------------------------------------------------\n"
        << "Stock     : "
        << stock_symbol
        << "\n"
        << "Benchmark : "
        << BENCHMARK_SYMBOL
        << "\n"
        << "------------------------------------------------------------\n";


    const PreparedRuntime stock_runtime =
        prepareRuntime(
            data_folder,
            stock_symbol
        );


    stats.stock_one_minute_candles +=
        stock_runtime.one_minute.size();

    stats.stock_five_minute_candles +=
        stock_runtime.five_minute.size();

    stats.stock_fifteen_minute_candles +=
        stock_runtime.fifteen_minute.size();


    stats.stock_incomplete_five_minute_buckets +=
        stock_runtime.incomplete_five_minute_buckets;

    stats.stock_incomplete_fifteen_minute_buckets +=
        stock_runtime.incomplete_fifteen_minute_buckets;


    stats.stock_invalid_rows +=
        stock_runtime.invalid_rows;

    stats.stock_duplicate_rows +=
        stock_runtime.duplicate_rows;

    stats.stock_skipped_rows +=
        stock_runtime.skipped_rows;


    std::cout
        << "Stock 1m : "
        << stock_runtime.one_minute.size()
        << " | 5m : "
        << stock_runtime.five_minute.size()
        << " | 15m : "
        << stock_runtime.fifteen_minute.size()
        << "\n";


    // ========================================================================
    // Phase 2 runtime
    // ========================================================================

    MultiTimeframeCursor stock_cursor(
        stock_runtime.timed_one,
        stock_runtime.timed_five,
        stock_runtime.timed_fifteen
    );


    MultiTimeframeCursor benchmark_cursor(
        benchmark_runtime.timed_one,
        benchmark_runtime.timed_five,
        benchmark_runtime.timed_fifteen
    );


    SessionState stock_session;
    SessionState benchmark_session;


    MarketSnapshotBuilder stock_snapshot_builder(
        stock_symbol
    );


    MarketSnapshotBuilder benchmark_snapshot_builder(
        BENCHMARK_SYMBOL
    );


    StockBenchmarkSynchronizer
        stock_benchmark_synchronizer;


    // ========================================================================
    // Stock Phase 3.1 - 3.4 dependencies required by Phase 3.6
    // ========================================================================

    PriceReturnFeatureEngine stock_price_engine(
        stock_symbol,
        64
    );


    TrendMomentumFeatureEngine stock_trend_engine(
        stock_symbol
    );


    VolatilityFeatureEngine stock_volatility_engine(
        stock_symbol
    );


    // ========================================================================
    // Benchmark engines
    //
    // These are intentionally independent for each stock run so that stock
    // and benchmark histories advance on the exact same decision sequence.
    // ========================================================================

    PriceReturnFeatureEngine benchmark_price_engine(
        BENCHMARK_SYMBOL,
        64
    );


    TrendMomentumFeatureEngine benchmark_trend_engine(
        BENCHMARK_SYMBOL
    );


    VolatilityFeatureEngine benchmark_volatility_engine(
        BENCHMARK_SYMBOL
    );


    // ========================================================================
    // Phase 3.6
    // ========================================================================

    StockBenchmarkFeatureEngine relative_engine(
        stock_symbol,
        BENCHMARK_SYMBOL
    );


    // ========================================================================
    // Runtime
    // ========================================================================

    for (
        const Candle& source_candle :
        stock_runtime.one_minute
    )
    {
        const std::int64_t decision_time =
            source_candle.timestamp +
            ONE_MINUTE_SECONDS;


        ++stats.decision_points;


        try
        {
            MarketSnapshot stock_snapshot =
                stock_snapshot_builder.build(
                    decision_time,
                    stock_cursor,
                    stock_session
                );


            MarketSnapshot benchmark_snapshot =
                benchmark_snapshot_builder.build(
                    decision_time,
                    benchmark_cursor,
                    benchmark_session
                );


            if (stock_snapshot.new_session)
            {
                ++stats.new_stock_sessions;
            }


            if (benchmark_snapshot.new_session)
            {
                ++stats.new_benchmark_sessions;
            }


            // ================================================================
            // Phase 2.4 synchronization
            // ================================================================

            const StockBenchmarkSnapshot pair =
                stock_benchmark_synchronizer.synchronize(
                    stock_snapshot,
                    benchmark_snapshot
                );


            if (!pair.fullySynchronized())
            {
                ++stats.incomplete_pairs;

                continue;
            }


            ++stats.synchronized_pairs;


            // ================================================================
            // Pair identity
            // ================================================================

            if (
                pair.stock.symbol != stock_symbol ||
                pair.benchmark.symbol != BENCHMARK_SYMBOL
            )
            {
                ++stats.symbol_violations;
            }


            if (
                pair.decision_time != decision_time ||
                pair.stock.decision_time != decision_time ||
                pair.benchmark.decision_time != decision_time
            )
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // Stock features
            // ================================================================

            const PriceReturnFeatures stock_price =
                stock_price_engine.update(
                    pair.stock
                );


            const TrendMomentumFeatures stock_trend =
                stock_trend_engine.update(
                    pair.stock
                );


            const VolatilityFeatures stock_volatility =
                stock_volatility_engine.update(
                    pair.stock
                );


            // ================================================================
            // Benchmark features
            // ================================================================

            const PriceReturnFeatures benchmark_price =
                benchmark_price_engine.update(
                    pair.benchmark
                );


            const TrendMomentumFeatures benchmark_trend =
                benchmark_trend_engine.update(
                    pair.benchmark
                );


            const VolatilityFeatures benchmark_volatility =
                benchmark_volatility_engine.update(
                    pair.benchmark
                );


            // ================================================================
            // Phase 3.6
            // ================================================================

            const StockBenchmarkFeatures actual =
                relative_engine.update(
                    stock_price,
                    stock_trend,
                    stock_volatility,

                    benchmark_price,
                    benchmark_trend,
                    benchmark_volatility
                );


            // ================================================================
            // Output identity
            // ================================================================

            if (
                actual.stock_symbol != stock_symbol ||
                actual.benchmark_symbol != BENCHMARK_SYMBOL
            )
            {
                ++stats.symbol_violations;
            }


            if (
                actual.decision_time != decision_time
            )
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // Independent validation
            // ================================================================

            validateOutput(
                actual,

                stock_price,
                stock_trend,
                stock_volatility,

                benchmark_price,
                benchmark_trend,
                benchmark_volatility,

                stats
            );


            countAvailability(
                actual,
                stats
            );
        }
        catch (const std::exception& error)
        {
            const std::string message =
                error.what();


            // ================================================================
            // Known data-alignment rejection
            //
            // Phase 2.4 correctly rejects a pair if both stock and benchmark
            // have a timeframe candle but those candles belong to different
            // market timestamps.
            //
            // This is expected with the already-known incomplete stock data.
            // It is recorded, not hidden, but it is not a Phase 3.6 failure.
            // ================================================================

            if (
                message.find(
                    "candle timestamps are not aligned"
                ) != std::string::npos
            )
            {
                ++stats.alignment_rejections;

                continue;
            }


            // ================================================================
            // Anything else is unexpected and MUST fail validation.
            // ================================================================

            ++stats.unexpected_runtime_exceptions;


            std::cerr
                << "Unexpected runtime exception for "
                << stock_symbol
                << " at "
                << decision_time
                << ": "
                << error.what()
                << "\n";
        }
    }
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


        if (argc >= 2)
        {
            data_folder =
                argv[1];
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "CETE PHASE 3.6 - REAL STOCK / NIFTY FEATURE VALIDATOR\n"
            << "============================================================\n"
            << "Data folder : "
            << data_folder
            << "\n"
            << "Benchmark   : "
            << BENCHMARK_SYMBOL
            << "\n"
            << "Mode        : ZERO_LATENCY historical runtime\n"
            << "============================================================\n";


        // ====================================================================
        // Benchmark
        // ====================================================================

        const std::filesystem::path benchmark_file =
            data_folder /
            (BENCHMARK_SYMBOL + "_1min.txt");


        if (!std::filesystem::exists(benchmark_file))
        {
            throw std::runtime_error(
                "Benchmark file not found: " +
                benchmark_file.string()
            );
        }


        // ====================================================================
        // Discover stocks
        // ====================================================================

        const std::vector<std::string> stocks =
            discoverStockSymbols(
                data_folder
            );


        if (stocks.empty())
        {
            throw std::runtime_error(
                "No stock *_1min.txt files discovered."
            );
        }


        std::cout
            << "\n"
            << "Discovered stock symbols : "
            << stocks.size()
            << "\n";


        for (
            std::size_t index = 0;
            index < stocks.size();
            ++index
        )
        {
            std::cout
                << "  "
                << std::setw(2)
                << index + 1
                << ". "
                << stocks[index]
                << "\n";
        }


        // ====================================================================
        // Prepare NIFTY once
        // ====================================================================

        std::cout
            << "\n"
            << "Preparing benchmark "
            << BENCHMARK_SYMBOL
            << "...\n";


        const PreparedRuntime benchmark_runtime =
            prepareRuntime(
                data_folder,
                BENCHMARK_SYMBOL
            );


        std::cout
            << "Benchmark 1m : "
            << benchmark_runtime.one_minute.size()
            << " | 5m : "
            << benchmark_runtime.five_minute.size()
            << " | 15m : "
            << benchmark_runtime.fifteen_minute.size()
            << "\n";


        // ====================================================================
        // All-stock validation
        // ====================================================================

        ValidationStats stats;

        stats.stock_symbols =
            stocks.size();


        for (
            std::size_t index = 0;
            index < stocks.size();
            ++index
        )
        {
            std::cout
                << "\n"
                << "["
                << std::setw(2)
                << index + 1
                << "/"
                << stocks.size()
                << "] "
                << stocks[index]
                << "\n";


            try
            {
                validateStock(
                    data_folder,
                    stocks[index],
                    benchmark_runtime,
                    stats
                );
            }
            catch (const std::exception& error)
            {
                ++stats.feature_exceptions;


                std::cerr
                    << "ERROR validating "
                    << stocks[index]
                    << ": "
                    << error.what()
                    << "\n";
            }
        }


        // ====================================================================
        // Failure decision
        //
        // Known alignment rejections are deliberately NOT included.
        //
        // They represent incomplete/misaligned source-data observations that
        // Phase 2.4 correctly refuses to synchronize.
        // ====================================================================

        const bool failed =
            stats.relative_return_mismatches != 0 ||
            stats.relative_rsi_mismatches != 0 ||
            stats.relative_ema_mismatches != 0 ||
            stats.relative_ema20_slope_mismatches != 0 ||
            stats.relative_ema50_slope_mismatches != 0 ||
            stats.relative_atr_mismatches != 0 ||
            stats.relative_realized_mismatches != 0 ||
            stats.core_readiness_mismatches != 0 ||
            stats.symbol_violations != 0 ||
            stats.decision_time_violations != 0 ||
            stats.invalid_numeric_values != 0 ||
            stats.unexpected_runtime_exceptions != 0 ||
            stats.feature_exceptions != 0;


        // ====================================================================
        // Report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.6 - ALL-STOCK REAL STOCK / NIFTY VALIDATION\n"
            << "============================================================\n"
            << "Benchmark                        : "
            << BENCHMARK_SYMBOL
            << "\n"
            << "Stock symbols                    : "
            << stats.stock_symbols
            << "\n"

            << "\n"
            << "Stock market data\n"
            << "------------------------------------------------------------\n"
            << "1-minute candles                : "
            << stats.stock_one_minute_candles
            << "\n"
            << "5-minute candles                : "
            << stats.stock_five_minute_candles
            << "\n"
            << "15-minute candles               : "
            << stats.stock_fifteen_minute_candles
            << "\n"
            << "Incomplete 5m buckets           : "
            << stats.stock_incomplete_five_minute_buckets
            << "\n"
            << "Incomplete 15m buckets          : "
            << stats.stock_incomplete_fifteen_minute_buckets
            << "\n"
            << "Invalid rows                    : "
            << stats.stock_invalid_rows
            << "\n"
            << "Duplicate rows                  : "
            << stats.stock_duplicate_rows
            << "\n"
            << "Skipped rows                    : "
            << stats.stock_skipped_rows
            << "\n"

            << "\n"
            << "Runtime synchronization\n"
            << "------------------------------------------------------------\n"
            << "Decision points                 : "
            << stats.decision_points
            << "\n"
            << "Fully synchronized pairs        : "
            << stats.synchronized_pairs
            << "\n"
            << "Incomplete pairs                : "
            << stats.incomplete_pairs
            << "\n"
            << "Known alignment rejections      : "
            << stats.alignment_rejections
            << "\n"
            << "Unexpected runtime exceptions   : "
            << stats.unexpected_runtime_exceptions
            << "\n"
            << "Stock new sessions              : "
            << stats.new_stock_sessions
            << "\n"
            << "Benchmark new sessions          : "
            << stats.new_benchmark_sessions
            << "\n"

            << "\n"
            << "Relative return availability\n"
            << "------------------------------------------------------------\n"
            << "1m                              : "
            << stats.relative_return_one_available
            << "\n"
            << "5m                              : "
            << stats.relative_return_five_available
            << "\n"
            << "15m                             : "
            << stats.relative_return_fifteen_available
            << "\n"

            << "\n"
            << "Relative RSI14 availability\n"
            << "------------------------------------------------------------\n"
            << "1m                              : "
            << stats.relative_rsi_one_available
            << "\n"
            << "5m                              : "
            << stats.relative_rsi_five_available
            << "\n"
            << "15m                             : "
            << stats.relative_rsi_fifteen_available
            << "\n"

            << "\n"
            << "Relative EMA relationship availability\n"
            << "------------------------------------------------------------\n"
            << "1m                              : "
            << stats.relative_ema_one_available
            << "\n"
            << "5m                              : "
            << stats.relative_ema_five_available
            << "\n"
            << "15m                             : "
            << stats.relative_ema_fifteen_available
            << "\n"

            << "\n"
            << "Relative EMA slope availability\n"
            << "------------------------------------------------------------\n"
            << "EMA20 1m                        : "
            << stats.relative_ema20_slope_one_available
            << "\n"
            << "EMA20 5m                        : "
            << stats.relative_ema20_slope_five_available
            << "\n"
            << "EMA20 15m                       : "
            << stats.relative_ema20_slope_fifteen_available
            << "\n"
            << "EMA50 1m                        : "
            << stats.relative_ema50_slope_one_available
            << "\n"
            << "EMA50 5m                        : "
            << stats.relative_ema50_slope_five_available
            << "\n"
            << "EMA50 15m                       : "
            << stats.relative_ema50_slope_fifteen_available
            << "\n"

            << "\n"
            << "Relative volume\n"
            << "------------------------------------------------------------\n"
            << "Stock RVOL20                    : PHASE 3.3 FEATURE\n"
            << "Stock/NIFTY RVOL20 ratio        : NOT APPLICABLE\n"

            << "\n"
            << "Relative ATR14% availability\n"
            << "------------------------------------------------------------\n"
            << "1m                              : "
            << stats.relative_atr_one_available
            << "\n"
            << "5m                              : "
            << stats.relative_atr_five_available
            << "\n"
            << "15m                             : "
            << stats.relative_atr_fifteen_available
            << "\n"

            << "\n"
            << "Relative Realized Volatility20 availability\n"
            << "------------------------------------------------------------\n"
            << "1m                              : "
            << stats.relative_realized_one_available
            << "\n"
            << "5m                              : "
            << stats.relative_realized_five_available
            << "\n"
            << "15m                             : "
            << stats.relative_realized_fifteen_available
            << "\n"

            << "\n"
            << "Core feature readiness\n"
            << "------------------------------------------------------------\n"
            << "Core features ready             : "
            << stats.core_features_ready
            << "\n"

            << "\n"
            << "Descriptive relative-return observations\n"
            << "------------------------------------------------------------\n"
            << "Positive relative return 1m     : "
            << stats.positive_relative_return_one
            << "\n"
            << "Negative relative return 1m     : "
            << stats.negative_relative_return_one
            << "\n"
            << "Positive relative return 5m     : "
            << stats.positive_relative_return_five
            << "\n"
            << "Negative relative return 5m     : "
            << stats.negative_relative_return_five
            << "\n"
            << "Positive relative return 15m    : "
            << stats.positive_relative_return_fifteen
            << "\n"
            << "Negative relative return 15m    : "
            << stats.negative_relative_return_fifteen
            << "\n"

            << "\n"
            << "Validation\n"
            << "------------------------------------------------------------\n"
            << "Relative return mismatches      : "
            << stats.relative_return_mismatches
            << "\n"
            << "Relative RSI14 mismatches       : "
            << stats.relative_rsi_mismatches
            << "\n"
            << "Relative EMA mismatches         : "
            << stats.relative_ema_mismatches
            << "\n"
            << "Relative EMA20 slope mismatches : "
            << stats.relative_ema20_slope_mismatches
            << "\n"
            << "Relative EMA50 slope mismatches : "
            << stats.relative_ema50_slope_mismatches
            << "\n"
            << "Relative ATR14% mismatches      : "
            << stats.relative_atr_mismatches
            << "\n"
            << "Relative RealVol20 mismatches   : "
            << stats.relative_realized_mismatches
            << "\n"
            << "Core readiness mismatches       : "
            << stats.core_readiness_mismatches
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
            << "Unexpected runtime exceptions   : "
            << stats.unexpected_runtime_exceptions
            << "\n"
            << "Feature exceptions              : "
            << stats.feature_exceptions
            << "\n"
            << "============================================================\n";


        // ====================================================================
        // Result
        // ====================================================================

        if (failed)
        {
            std::cerr
                << "\n"
                << "============================================================\n"
                << "PHASE 3.6 REAL STOCK / NIFTY FEATURE VALIDATION FAILED\n"
                << "============================================================\n"
                << "A Phase 3.6 calculation or integrity violation was detected.\n"
                << "============================================================\n";


            return 1;
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.6 REAL STOCK / NIFTY FEATURE VALIDATION PASSED\n"
            << "============================================================\n"
            << "Relative returns                : PASSED\n"
            << "Relative RSI14                  : PASSED\n"
            << "Relative EMA relationship       : PASSED\n"
            << "Relative EMA20 slope            : PASSED\n"
            << "Relative EMA50 slope            : PASSED\n"
            << "Stock RVOL20                    : PHASE 3.3 FEATURE\n"
            << "Stock/NIFTY RVOL20 ratio        : NOT APPLICABLE\n"
            << "Relative ATR14%                 : PASSED\n"
            << "Relative Realized Volatility20  : PASSED\n"
            << "Core readiness                  : PASSED\n"
            << "Known alignment rejection safety: PASSED\n"
            << "Symbol integrity                : PASSED\n"
            << "Decision-time integrity         : PASSED\n"
            << "Numeric integrity               : PASSED\n"
            << "============================================================\n";


        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "\n"
            << "PHASE 3.6 REAL VALIDATION ERROR\n"
            << error.what()
            << "\n";


        return 1;
    }
}