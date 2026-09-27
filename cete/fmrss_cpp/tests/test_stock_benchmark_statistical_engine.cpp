#include "devai/statistics/StockBenchmarkStatisticalEngine.hpp"

#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using devai::features::PriceReturnFeatures;
using devai::features::StockBenchmarkFeatures;

using devai::market::Candle;
using devai::market::MarketSnapshot;

using devai::statistics::RelativeStatisticalContext;
using devai::statistics::StockBenchmarkStatisticalEngine;
using devai::statistics::StockBenchmarkStatisticalFeatures;


// ============================================================================
// Configuration
// ============================================================================

constexpr const char* STOCK_SYMBOL =
    "RELIANCE";

constexpr const char* BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::size_t REGRESSION_WINDOW =
    5;

constexpr std::size_t RESIDUAL_WINDOW =
    5;

constexpr double EXTREME_Z_THRESHOLD =
    2.0;

constexpr double EPSILON =
    1.0e-10;


// ============================================================================
// Test result
// ============================================================================

struct TestResult
{
    std::string name;

    bool passed{false};

    std::string error;
};


// ============================================================================
// Numeric helper
// ============================================================================

bool approximatelyEqual(
    double left,
    double right,
    double tolerance = EPSILON)
{
    if (std::isnan(left) &&
        std::isnan(right))
    {
        return true;
    }


    if (!std::isfinite(left) ||
        !std::isfinite(right))
    {
        return left == right;
    }


    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            });


    return
        std::fabs(
            left -
            right) <=
        tolerance *
        scale;
}


// ============================================================================
// Assertion helpers
// ============================================================================

void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(
            message);
    }
}


void requireApproximatelyEqual(
    double actual,
    double expected,
    const std::string& message,
    double tolerance = EPSILON)
{
    if (!approximatelyEqual(
            actual,
            expected,
            tolerance))
    {
        throw std::runtime_error(
            message +
            " | actual=" +
            std::to_string(actual) +
            " expected=" +
            std::to_string(expected));
    }
}


// ============================================================================
// Candle helper
// ============================================================================

Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double close = 100.0,
    double volume = 1000.0)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        close;

    candle.high =
        close;

    candle.low =
        close;

    candle.close =
        close;

    candle.volume =
        volume;

    return candle;
}


// ============================================================================
// Snapshot helper
//
// For artificial validation we only need:
//     symbol
//     decision_time
//     1m / 5m / 15m source timestamps
//
// The same timestamp can deliberately be carried forward to test repeated
// candle protection.
// ============================================================================

MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    std::optional<std::int64_t> one_minute_timestamp,
    std::optional<std::int64_t> five_minute_timestamp,
    std::optional<std::int64_t> fifteen_minute_timestamp)
{
    MarketSnapshot snapshot;

    snapshot.symbol =
        symbol;

    snapshot.decision_time =
        decision_time;


    if (one_minute_timestamp)
    {
        snapshot.one_minute =
            makeCandle(
                symbol,
                *one_minute_timestamp);
    }


    if (five_minute_timestamp)
    {
        snapshot.five_minute =
            makeCandle(
                symbol,
                *five_minute_timestamp);
    }


    if (fifteen_minute_timestamp)
    {
        snapshot.fifteen_minute =
            makeCandle(
                symbol,
                *fifteen_minute_timestamp);
    }


    return snapshot;
}


// ============================================================================
// PriceReturnFeatures helper
// ============================================================================

PriceReturnFeatures makePriceReturns(
    const std::string& symbol,
    std::int64_t decision_time,
    double one_minute_return,
    double five_minute_return,
    double fifteen_minute_return,
    bool has_one_minute = true,
    bool has_five_minute = true,
    bool has_fifteen_minute = true)
{
    PriceReturnFeatures features;

    features.symbol =
        symbol;

    features.decision_time =
        decision_time;


    features.one_minute_return =
        one_minute_return;

    features.five_minute_return =
        five_minute_return;

    features.fifteen_minute_return =
        fifteen_minute_return;


    features.has_one_minute =
        has_one_minute;

    features.has_five_minute =
        has_five_minute;

    features.has_fifteen_minute =
        has_fifteen_minute;


    return features;
}


// ============================================================================
// Phase 3.6 relative feature helper
// ============================================================================

StockBenchmarkFeatures makeRelativeFeatures(
    std::int64_t decision_time,
    double stock_one,
    double benchmark_one,
    double stock_five,
    double benchmark_five,
    double stock_fifteen,
    double benchmark_fifteen,
    bool has_one = true,
    bool has_five = true,
    bool has_fifteen = true)
{
    StockBenchmarkFeatures features;

    features.stock_symbol =
        STOCK_SYMBOL;

    features.benchmark_symbol =
        BENCHMARK_SYMBOL;

    features.decision_time =
        decision_time;


    if (has_one)
    {
        features.relative_return.one_minute =
            stock_one -
            benchmark_one;

        features.relative_return.has_one_minute =
            true;
    }


    if (has_five)
    {
        features.relative_return.five_minute =
            stock_five -
            benchmark_five;

        features.relative_return.has_five_minute =
            true;
    }


    if (has_fifteen)
    {
        features.relative_return.fifteen_minute =
            stock_fifteen -
            benchmark_fifteen;

        features.relative_return.has_fifteen_minute =
            true;
    }


    return features;
}


// ============================================================================
// Complete artificial update helper
// ============================================================================

StockBenchmarkStatisticalFeatures updateEngine(
    StockBenchmarkStatisticalEngine& engine,
    std::int64_t decision_time,
    std::int64_t source_timestamp,
    double stock_return,
    double benchmark_return)
{
    const auto stock_features =
        makePriceReturns(
            STOCK_SYMBOL,
            decision_time,
            stock_return,
            stock_return,
            stock_return);


    const auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            decision_time,
            benchmark_return,
            benchmark_return,
            benchmark_return);


    const auto relative_features =
        makeRelativeFeatures(
            decision_time,
            stock_return,
            benchmark_return,
            stock_return,
            benchmark_return,
            stock_return,
            benchmark_return);


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            decision_time,
            source_timestamp,
            source_timestamp,
            source_timestamp);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            decision_time,
            source_timestamp,
            source_timestamp,
            source_timestamp);


    return
        engine.update(
            stock_features,
            benchmark_features,
            relative_features,
            stock_snapshot,
            benchmark_snapshot);
}


// ============================================================================
// Independent regression reference
// ============================================================================

struct ReferenceRegression
{
    double beta{
        std::numeric_limits<double>::
            quiet_NaN()
    };

    double alpha{
        std::numeric_limits<double>::
            quiet_NaN()
    };

    double correlation{
        std::numeric_limits<double>::
            quiet_NaN()
    };
};


ReferenceRegression calculateReferenceRegression(
    const std::vector<double>& stock_returns,
    const std::vector<double>& benchmark_returns)
{
    require(
        stock_returns.size() ==
            benchmark_returns.size(),
        "Reference regression size mismatch.");


    require(
        !stock_returns.empty(),
        "Reference regression cannot use empty data.");


    const std::size_t count =
        stock_returns.size();


    long double stock_sum =
        0.0L;

    long double benchmark_sum =
        0.0L;


    for (std::size_t index = 0;
         index < count;
         ++index)
    {
        stock_sum +=
            static_cast<long double>(
                stock_returns[index]);

        benchmark_sum +=
            static_cast<long double>(
                benchmark_returns[index]);
    }


    const long double stock_mean =
        stock_sum /
        static_cast<long double>(
            count);


    const long double benchmark_mean =
        benchmark_sum /
        static_cast<long double>(
            count);


    long double stock_variance_sum =
        0.0L;

    long double benchmark_variance_sum =
        0.0L;

    long double covariance_sum =
        0.0L;


    for (std::size_t index = 0;
         index < count;
         ++index)
    {
        const long double stock_difference =
            static_cast<long double>(
                stock_returns[index]) -
            stock_mean;


        const long double benchmark_difference =
            static_cast<long double>(
                benchmark_returns[index]) -
            benchmark_mean;


        stock_variance_sum +=
            stock_difference *
            stock_difference;


        benchmark_variance_sum +=
            benchmark_difference *
            benchmark_difference;


        covariance_sum +=
            stock_difference *
            benchmark_difference;
    }


    const long double denominator =
        static_cast<long double>(
            count);


    const long double stock_variance =
        stock_variance_sum /
        denominator;


    const long double benchmark_variance =
        benchmark_variance_sum /
        denominator;


    const long double covariance =
        covariance_sum /
        denominator;


    ReferenceRegression result;


    result.beta =
        static_cast<double>(
            covariance /
            benchmark_variance);


    result.alpha =
        static_cast<double>(
            stock_mean -
            static_cast<long double>(
                result.beta) *
            benchmark_mean);


    result.correlation =
        static_cast<double>(
            covariance /
            std::sqrt(
                stock_variance *
                benchmark_variance));


    return result;
}


// ============================================================================
// Test 1 — Rolling Beta
//
// Artificial model:
//
//     stock = 0.001 + 2 * NIFTY
//
// Expected beta = 2
// ============================================================================

void testRollingBeta()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.010,
        -0.005,
         0.002,
         0.006,
         0.012
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        const double stock =
            0.001 +
            2.0 *
            benchmark[index];


        result =
            updateEngine(
                engine,
                1000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark[index]);
    }


    require(
        result.one_minute.regression_ready,
        "Regression should be ready after five observations.");


    requireApproximatelyEqual(
        result.one_minute.rolling_beta,
        2.0,
        "Rolling beta is incorrect.");
}


// ============================================================================
// Test 2 — Alpha / intercept
// ============================================================================

void testAlpha()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.010,
        -0.004,
         0.001,
         0.007,
         0.013
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        const double stock =
            0.002 +
            1.5 *
            benchmark[index];


        result =
            updateEngine(
                engine,
                2000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                1940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark[index]);
    }


    requireApproximatelyEqual(
        result.one_minute.rolling_beta,
        1.5,
        "Beta should equal 1.5.");


    requireApproximatelyEqual(
        result.one_minute.rolling_alpha,
        0.002,
        "Alpha/intercept is incorrect.");
}


// ============================================================================
// Test 3 — Rolling correlation
//
// Perfect positive linear relationship -> correlation +1.
// ============================================================================

void testRollingCorrelation()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.020,
        -0.010,
         0.001,
         0.011,
         0.020
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        const double stock =
            0.003 +
            1.25 *
            benchmark[index];


        result =
            updateEngine(
                engine,
                3000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                2940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark[index]);
    }


    requireApproximatelyEqual(
        result.one_minute.
            rolling_correlation,
        1.0,
        "Perfect positive relationship should have correlation +1.");
}


// ============================================================================
// Test 4 — Residual return
//
// Uses a non-perfect relationship and independently calculates the expected
// regression and current residual.
// ============================================================================

void testResidualReturn()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.010,
        -0.004,
         0.002,
         0.007,
         0.013
    };


    const std::vector<double> stock =
    {
        -0.013,
        -0.004,
         0.005,
         0.010,
         0.025
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        result =
            updateEngine(
                engine,
                4000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                3940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock[index],
                benchmark[index]);
    }


    const ReferenceRegression reference =
        calculateReferenceRegression(
            stock,
            benchmark);


    const double expected_residual =
        stock.back() -
        (
            reference.alpha +
            reference.beta *
            benchmark.back()
        );


    requireApproximatelyEqual(
        result.one_minute.rolling_beta,
        reference.beta,
        "Residual test beta mismatch.");


    requireApproximatelyEqual(
        result.one_minute.rolling_alpha,
        reference.alpha,
        "Residual test alpha mismatch.");


    requireApproximatelyEqual(
        result.one_minute.residual_return,
        expected_residual,
        "Residual return is incorrect.");
}


// ============================================================================
// Test 5 — Residual Z-score
//
// Regression window = 5.
// Residual window   = 5.
//
// Feed enough observations so the residual statistical window becomes ready.
// ============================================================================

void testResidualZScore()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.010,
        -0.006,
        -0.002,
         0.003,
         0.007,
         0.011,
         0.015,
         0.019,
         0.023
    };


    const std::vector<double> noise =
    {
         0.0010,
        -0.0005,
         0.0008,
        -0.0012,
         0.0003,
         0.0015,
        -0.0007,
         0.0004,
         0.0030
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        const double stock =
            0.001 +
            1.4 *
            benchmark[index] +
            noise[index];


        result =
            updateEngine(
                engine,
                5000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                4940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark[index]);
    }


    require(
        result.one_minute.
            regression_ready,
        "Regression must be ready.");


    require(
        result.one_minute.
            residual_statistics_ready,
        "Residual statistics should be ready after five residuals.");


    require(
        result.one_minute.
            residual_observation_count ==
            RESIDUAL_WINDOW,
        "Residual rolling observation count should equal residual window.");


    require(
        std::isfinite(
            result.one_minute.
                residual_z_score),
        "Residual Z-score must be finite.");


    require(
        std::isfinite(
            result.one_minute.
                residual_robust_z_score),
        "Robust residual Z-score must be finite.");
}


// ============================================================================
// Test 6 — Window readiness
//
// With regression window 5:
//
// observations 1..4 -> not regression ready
// observation 5     -> regression ready
//
// Residual statistics require another five residual observations.
// Therefore complete readiness occurs at observation 9.
// ============================================================================

void testWindowReadiness()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < 4;
         ++index)
    {
        const double benchmark =
            0.001 *
            static_cast<double>(
                index + 1);


        const double stock =
            0.0005 +
            1.2 *
            benchmark;


        result =
            updateEngine(
                engine,
                6000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                5940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark);


        require(
            !result.one_minute.
                regression_ready,
            "Regression became ready before the full regression window.");
    }


    result =
        updateEngine(
            engine,
            6240,
            6180,
            0.0005 +
                1.2 *
                0.005,
            0.005);


    require(
        result.one_minute.
            regression_ready,
        "Regression should become ready on observation five.");


    require(
        !result.one_minute.
            residual_statistics_ready,
        "Residual statistics should not yet be ready.");


    for (std::size_t index = 5;
         index < 9;
         ++index)
    {
        const double benchmark =
            0.001 *
            static_cast<double>(
                index + 1);


        const double noise =
            (index % 2 == 0)
                ? 0.0001
                : -0.0001;


        const double stock =
            0.0005 +
            1.2 *
            benchmark +
            noise;


        result =
            updateEngine(
                engine,
                6000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                5940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark);
    }


    require(
        result.one_minute.
            residual_statistics_ready,
        "Residual statistics should be ready at observation nine.");


    require(
        result.one_minute.
            fully_ready,
        "1-minute Phase 4.4 state should be fully ready.");


    require(
        result.fully_ready,
        "All horizons should be fully ready because artificial helper "
        "feeds identical 1m/5m/15m streams.");
}


// ============================================================================
// Test 7 — Independent 1m / 5m / 15m horizons
//
// 1m source changes every update.
// 5m source remains carried forward.
// 15m source remains carried forward.
//
// Only 1m should advance on repeated higher-timeframe candles.
// ============================================================================

void testIndependentHorizons()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    // First observation.

    {
        const std::int64_t decision_time =
            7000;


        const auto stock_features =
            makePriceReturns(
                STOCK_SYMBOL,
                decision_time,
                0.002,
                0.003,
                0.004);


        const auto benchmark_features =
            makePriceReturns(
                BENCHMARK_SYMBOL,
                decision_time,
                0.001,
                0.0015,
                0.002);


        const auto relative =
            makeRelativeFeatures(
                decision_time,
                0.002,
                0.001,
                0.003,
                0.0015,
                0.004,
                0.002);


        const auto stock_snapshot =
            makeSnapshot(
                STOCK_SYMBOL,
                decision_time,
                6940,
                6900,
                6900);


        const auto benchmark_snapshot =
            makeSnapshot(
                BENCHMARK_SYMBOL,
                decision_time,
                6940,
                6900,
                6900);


        engine.update(
            stock_features,
            benchmark_features,
            relative,
            stock_snapshot,
            benchmark_snapshot);
    }


    // Second decision:
    // 1m advances.
    // 5m and 15m are deliberately repeated.

    const std::int64_t decision_time =
        7060;


    const auto stock_features =
        makePriceReturns(
            STOCK_SYMBOL,
            decision_time,
            0.004,
            0.003,
            0.004);


    const auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            decision_time,
            0.002,
            0.0015,
            0.002);


    const auto relative =
        makeRelativeFeatures(
            decision_time,
            0.004,
            0.002,
            0.003,
            0.0015,
            0.004,
            0.002);


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            decision_time,
            7000,
            6900,
            6900);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            decision_time,
            7000,
            6900,
            6900);


    const auto result =
        engine.update(
            stock_features,
            benchmark_features,
            relative,
            stock_snapshot,
            benchmark_snapshot);


    require(
        result.one_minute.
            paired_observation_count ==
            2,
        "1m horizon did not advance independently.");


    require(
        result.five_minute.
            paired_observation_count ==
            1,
        "Repeated 5m candle was consumed twice.");


    require(
        result.fifteen_minute.
            paired_observation_count ==
            1,
        "Repeated 15m candle was consumed twice.");
}


// ============================================================================
// Test 8 — Repeated candle protection
// ============================================================================

void testRepeatedCandleProtection()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    // Decision 1.

    updateEngine(
        engine,
        8000,
        7940,
        0.003,
        0.001);


    // Decision 2 uses a new decision time but the same source timestamp.
    // All horizons should ignore the repeated source candle.

    const auto stock_features =
        makePriceReturns(
            STOCK_SYMBOL,
            8060,
            0.003,
            0.003,
            0.003);


    const auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            8060,
            0.001,
            0.001,
            0.001);


    const auto relative =
        makeRelativeFeatures(
            8060,
            0.003,
            0.001,
            0.003,
            0.001,
            0.003,
            0.001);


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            8060,
            7940,
            7940,
            7940);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            8060,
            7940,
            7940,
            7940);


    const auto result =
        engine.update(
            stock_features,
            benchmark_features,
            relative,
            stock_snapshot,
            benchmark_snapshot);


    require(
        result.one_minute.
            paired_observation_count ==
            1,
        "Repeated 1m source was consumed.");


    require(
        result.five_minute.
            paired_observation_count ==
            1,
        "Repeated 5m source was consumed.");


    require(
        result.fifteen_minute.
            paired_observation_count ==
            1,
        "Repeated 15m source was consumed.");
}


// ============================================================================
// Test 9 — Relative statistical context
// ============================================================================

void testRelativeStatisticalContext()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.010,
        -0.005,
         0.001,
         0.006,
         0.011,
         0.016,
         0.021,
         0.026,
         0.031
    };


    const std::vector<double> noise =
    {
         0.0005,
        -0.0004,
         0.0003,
        -0.0006,
         0.0002,
         0.0007,
        -0.0003,
         0.0004,
         0.0040
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        const double stock =
            0.001 +
            1.3 *
            benchmark[index] +
            noise[index];


        result =
            updateEngine(
                engine,
                9000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                8940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark[index]);
    }


    require(
        result.one_minute.
            relative_context !=
            RelativeStatisticalContext::
                UNAVAILABLE,
        "Relative context should be available once residual statistics exist.");


    require(
        result.one_minute.
            relative_context !=
            RelativeStatisticalContext::
                NEUTRAL,
        "Artificial final residual should not be neutral.");
}


// ============================================================================
// Test 10 — Cross-session continuity
//
// Phase 4.4 has no session reset. Artificial snapshots can move across a large
// time gap and the rolling mathematical state must remain intact.
// ============================================================================

void testCrossSessionContinuity()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    for (std::size_t index = 0;
         index < 4;
         ++index)
    {
        const double benchmark =
            0.001 *
            static_cast<double>(
                index + 1);


        updateEngine(
            engine,
            10000 +
                static_cast<std::int64_t>(
                    index) *
                60,
            9940 +
                static_cast<std::int64_t>(
                    index) *
                60,
            0.0005 +
                1.2 *
                benchmark,
            benchmark);
    }


    // Artificial "next session" jump.
    // No reset() is called.

    const auto result =
        updateEngine(
            engine,
            100000,
            99940,
            0.0005 +
                1.2 *
                0.005,
            0.005);


    require(
        result.one_minute.
            paired_observation_count ==
            REGRESSION_WINDOW,
        "Rolling state was lost across session/time gap.");


    require(
        result.one_minute.
            regression_ready,
        "Regression should become ready using cross-session history.");
}


// ============================================================================
// Test 11 — Explicit reset
// ============================================================================

void testExplicitReset()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    for (std::size_t index = 0;
         index < 5;
         ++index)
    {
        const double benchmark =
            0.001 *
            static_cast<double>(
                index + 1);


        updateEngine(
            engine,
            11000 +
                static_cast<std::int64_t>(
                    index) *
                60,
            10940 +
                static_cast<std::int64_t>(
                    index) *
                60,
            0.001 +
                1.4 *
                benchmark,
            benchmark);
    }


    engine.reset();


    const auto result =
        updateEngine(
            engine,
            20000,
            19940,
            0.003,
            0.001);


    require(
        result.one_minute.
            paired_observation_count ==
            1,
        "Explicit reset did not clear regression history.");


    require(
        !result.one_minute.
            regression_ready,
        "Regression remained ready after explicit reset.");


    require(
        result.one_minute.
            residual_observation_count ==
            0,
        "Explicit reset did not clear residual statistics.");
}


// ============================================================================
// Test 12 — Backward decision-time protection
// ============================================================================

void testBackwardTimeProtection()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    updateEngine(
        engine,
        12000,
        11940,
        0.002,
        0.001);


    bool threw =
        false;


    try
    {
        updateEngine(
            engine,
            11900,
            11840,
            0.003,
            0.0015);
    }
    catch (const std::runtime_error&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Backward decision time was not rejected.");
}


// ============================================================================
// Test 13 — Duplicate decision-time protection
// ============================================================================

void testDuplicateTimeProtection()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    updateEngine(
        engine,
        13000,
        12940,
        0.002,
        0.001);


    bool threw =
        false;


    try
    {
        updateEngine(
            engine,
            13000,
            13000,
            0.003,
            0.0015);
    }
    catch (const std::runtime_error&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Duplicate decision time was not rejected.");
}


// ============================================================================
// Test 14 — Symbol protection
// ============================================================================

void testSymbolProtection()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::int64_t decision_time =
        14000;


    auto stock_features =
        makePriceReturns(
            "WRONG_SYMBOL",
            decision_time,
            0.002,
            0.002,
            0.002);


    const auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            decision_time,
            0.001,
            0.001,
            0.001);


    const auto relative =
        makeRelativeFeatures(
            decision_time,
            0.002,
            0.001,
            0.002,
            0.001,
            0.002,
            0.001);


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            decision_time,
            13940,
            13940,
            13940);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            decision_time,
            13940,
            13940,
            13940);


    bool threw =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                stock_features,
                benchmark_features,
                relative,
                stock_snapshot,
                benchmark_snapshot));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Stock symbol mismatch was not rejected.");
}


// ============================================================================
// Test 15 — Decision-time integrity
// ============================================================================

void testDecisionTimeIntegrity()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::int64_t decision_time =
        15000;


    const auto stock_features =
        makePriceReturns(
            STOCK_SYMBOL,
            decision_time,
            0.002,
            0.002,
            0.002);


    auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            decision_time + 1,
            0.001,
            0.001,
            0.001);


    const auto relative =
        makeRelativeFeatures(
            decision_time,
            0.002,
            0.001,
            0.002,
            0.001,
            0.002,
            0.001);


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            decision_time,
            14940,
            14940,
            14940);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            decision_time,
            14940,
            14940,
            14940);


    bool threw =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                stock_features,
                benchmark_features,
                relative,
                stock_snapshot,
                benchmark_snapshot));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Decision-time mismatch was not rejected.");
}


// ============================================================================
// Test 16 — Phase 3.6 relative-return integrity
// ============================================================================

void testRelativeReturnIntegrity()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::int64_t decision_time =
        16000;


    const auto stock_features =
        makePriceReturns(
            STOCK_SYMBOL,
            decision_time,
            0.010,
            0.010,
            0.010);


    const auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            decision_time,
            0.004,
            0.004,
            0.004);


    auto relative =
        makeRelativeFeatures(
            decision_time,
            0.010,
            0.004,
            0.010,
            0.004,
            0.010,
            0.004);


    // Correct relative return would be 0.006.
    // Deliberately corrupt 1m value.

    relative.relative_return.one_minute =
        0.999;


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            decision_time,
            15940,
            15940,
            15940);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            decision_time,
            15940,
            15940,
            15940);


    bool threw =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                stock_features,
                benchmark_features,
                relative,
                stock_snapshot,
                benchmark_snapshot));
    }
    catch (const std::runtime_error&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Inconsistent Phase 3.6 relative return was not rejected.");
}


// ============================================================================
// Test 17 — Numeric integrity
// ============================================================================

void testNumericIntegrity()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const double invalid =
        std::numeric_limits<double>::
            infinity();


    const std::int64_t decision_time =
        17000;


    auto stock_features =
        makePriceReturns(
            STOCK_SYMBOL,
            decision_time,
            invalid,
            invalid,
            invalid);


    const auto benchmark_features =
        makePriceReturns(
            BENCHMARK_SYMBOL,
            decision_time,
            0.001,
            0.001,
            0.001);


    const auto relative =
        makeRelativeFeatures(
            decision_time,
            0.002,
            0.001,
            0.002,
            0.001,
            0.002,
            0.001);


    const auto stock_snapshot =
        makeSnapshot(
            STOCK_SYMBOL,
            decision_time,
            16940,
            16940,
            16940);


    const auto benchmark_snapshot =
        makeSnapshot(
            BENCHMARK_SYMBOL,
            decision_time,
            16940,
            16940,
            16940);


    // The Phase 4.4 update API treats a return as available only when
    // has_* is true AND the return is finite.
    //
    // Therefore an infinite PriceReturnFeatures value must never enter the
    // regression. The result must remain unavailable rather than producing
    // infinity.

    const auto result =
        engine.update(
            stock_features,
            benchmark_features,
            relative,
            stock_snapshot,
            benchmark_snapshot);


    require(
        !result.one_minute.
            has_observation,
        "Non-finite stock return entered the 1m regression.");


    require(
        !result.five_minute.
            has_observation,
        "Non-finite stock return entered the 5m regression.");


    require(
        !result.fifteen_minute.
            has_observation,
        "Non-finite stock return entered the 15m regression.");
}


// ============================================================================
// Test 18 — Negative correlation
// ============================================================================

void testNegativeCorrelation()
{
    StockBenchmarkStatisticalEngine engine(
        STOCK_SYMBOL,
        BENCHMARK_SYMBOL,
        REGRESSION_WINDOW,
        RESIDUAL_WINDOW,
        EXTREME_Z_THRESHOLD);


    const std::vector<double> benchmark =
    {
        -0.010,
        -0.005,
         0.001,
         0.007,
         0.012
    };


    StockBenchmarkStatisticalFeatures result;


    for (std::size_t index = 0;
         index < benchmark.size();
         ++index)
    {
        const double stock =
            0.002 -
            1.5 *
            benchmark[index];


        result =
            updateEngine(
                engine,
                18000 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                17940 +
                    static_cast<std::int64_t>(
                        index) *
                    60,
                stock,
                benchmark[index]);
    }


    requireApproximatelyEqual(
        result.one_minute.
            rolling_beta,
        -1.5,
        "Negative beta is incorrect.");


    requireApproximatelyEqual(
        result.one_minute.
            rolling_correlation,
        -1.0,
        "Perfect negative relationship should have correlation -1.");
}


// ============================================================================
// Test runner
// ============================================================================

TestResult runTest(
    const std::string& name,
    const std::function<void()>& test)
{
    TestResult result;

    result.name =
        name;


    try
    {
        test();

        result.passed =
            true;
    }
    catch (const std::exception& exception)
    {
        result.passed =
            false;

        result.error =
            exception.what();
    }
    catch (...)
    {
        result.passed =
            false;

        result.error =
            "Unknown exception.";
    }


    return result;
}

} // namespace


// ============================================================================
// MAIN
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.4 — STOCK / NIFTY STATISTICAL ENGINE TEST\n"
        << "============================================================\n";


    const std::vector<TestResult> results =
    {
        runTest(
            "Rolling Beta",
            testRollingBeta),

        runTest(
            "Alpha / Intercept",
            testAlpha),

        runTest(
            "Rolling Correlation",
            testRollingCorrelation),

        runTest(
            "Residual Return",
            testResidualReturn),

        runTest(
            "Residual Z-Score",
            testResidualZScore),

        runTest(
            "Window Readiness",
            testWindowReadiness),

        runTest(
            "Independent Timeframes",
            testIndependentHorizons),

        runTest(
            "Repeated Candle Protection",
            testRepeatedCandleProtection),

        runTest(
            "Relative Statistical Context",
            testRelativeStatisticalContext),

        runTest(
            "Cross-Session Continuity",
            testCrossSessionContinuity),

        runTest(
            "Explicit Reset",
            testExplicitReset),

        runTest(
            "Backward-Time Protection",
            testBackwardTimeProtection),

        runTest(
            "Duplicate-Time Protection",
            testDuplicateTimeProtection),

        runTest(
            "Symbol Protection",
            testSymbolProtection),

        runTest(
            "Decision-Time Integrity",
            testDecisionTimeIntegrity),

        runTest(
            "Phase 3.6 Relative Integrity",
            testRelativeReturnIntegrity),

        runTest(
            "Numeric Integrity",
            testNumericIntegrity),

        runTest(
            "Negative Correlation",
            testNegativeCorrelation)
    };


    bool all_passed =
        true;


    for (const auto& result :
         results)
    {
        std::cout
            << std::left
            << std::setw(34)
            << (
                result.name +
                " :"
            )
            << (
                result.passed
                    ? "PASSED"
                    : "FAILED"
            )
            << "\n";


        if (!result.passed)
        {
            all_passed =
                false;


            std::cout
                << "    "
                << result.error
                << "\n";
        }
    }


    std::cout
        << "============================================================\n";


    if (!all_passed)
    {
        std::cout
            << "PHASE 4.4 STOCK / NIFTY STATISTICAL ENGINE FAILED\n"
            << "============================================================\n";

        return EXIT_FAILURE;
    }


    std::cout
        << "PHASE 4.4 STOCK / NIFTY STATISTICAL ENGINE PASSED\n"
        << "============================================================\n";


    return EXIT_SUCCESS;
}