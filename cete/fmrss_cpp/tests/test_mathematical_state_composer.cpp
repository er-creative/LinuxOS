#include "devai/statistics/MathematicalStateComposer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using namespace devai::statistics;


// ============================================================================
// Constants
// ============================================================================

constexpr double EPSILON = 1.0e-9;

constexpr std::int64_t DECISION_TIME =
    1'800'000'000;


// ============================================================================
// Numeric helper
// ============================================================================

bool approximatelyEqual(
    double lhs,
    double rhs,
    double tolerance = EPSILON)
{
    if (std::isnan(lhs) &&
        std::isnan(rhs))
    {
        return true;
    }

    if (!std::isfinite(lhs) ||
        !std::isfinite(rhs))
    {
        return lhs == rhs;
    }

    const double scale =
        std::max(
            1.0,
            std::max(
                std::fabs(lhs),
                std::fabs(rhs)));

    return
        std::fabs(lhs - rhs) <=
        tolerance * scale;
}


// ============================================================================
// Test report
// ============================================================================

struct TestReport
{
    bool constructor_validation{true};

    bool metadata_composition{true};
    bool lossless_return_composition{true};
    bool lossless_volume_composition{true};
    bool lossless_stock_benchmark_composition{true};
    bool lossless_regime_composition{true};

    bool full_readiness{true};
    bool partial_readiness{true};
    bool timeframe_independence{true};

    bool warmup_nan_allowed{true};
    bool available_nan_rejected{true};
    bool invalid_stddev_rejected{true};
    bool invalid_correlation_rejected{true};
    bool invalid_trend_score_rejected{true};

    bool return_readiness_integrity{true};
    bool volume_readiness_integrity{true};
    bool stock_benchmark_readiness_integrity{true};
    bool regime_readiness_integrity{true};

    bool return_symbol_protection{true};
    bool volume_symbol_protection{true};
    bool stock_symbol_protection{true};
    bool benchmark_symbol_protection{true};
    bool regime_stock_symbol_protection{true};
    bool regime_benchmark_symbol_protection{true};

    bool volume_decision_time_protection{true};
    bool stock_benchmark_decision_time_protection{true};
    bool regime_decision_time_protection{true};

    bool deterministic_stateless_composition{true};


    [[nodiscard]]
    bool passed() const noexcept
    {
        return
            constructor_validation &&
            metadata_composition &&
            lossless_return_composition &&
            lossless_volume_composition &&
            lossless_stock_benchmark_composition &&
            lossless_regime_composition &&
            full_readiness &&
            partial_readiness &&
            timeframe_independence &&
            warmup_nan_allowed &&
            available_nan_rejected &&
            invalid_stddev_rejected &&
            invalid_correlation_rejected &&
            invalid_trend_score_rejected &&
            return_readiness_integrity &&
            volume_readiness_integrity &&
            stock_benchmark_readiness_integrity &&
            regime_readiness_integrity &&
            return_symbol_protection &&
            volume_symbol_protection &&
            stock_symbol_protection &&
            benchmark_symbol_protection &&
            regime_stock_symbol_protection &&
            regime_benchmark_symbol_protection &&
            volume_decision_time_protection &&
            stock_benchmark_decision_time_protection &&
            regime_decision_time_protection &&
            deterministic_stateless_composition;
    }
};


// ============================================================================
// Phase 4.2 artificial data
// ============================================================================

ReturnHorizonStatistics makeReturnHorizon(
    double value)
{
    ReturnHorizonStatistics output;

    output.return_value =
        value;

    output.absolute_return =
        std::fabs(value);

    output.rolling_mean =
        value * 0.50;

    output.rolling_standard_deviation =
        0.010;

    output.z_score =
        0.75;

    output.rolling_median =
        value * 0.40;

    output.rolling_mad =
        0.008;

    output.robust_z_score =
        0.60;


    output.absolute_return_mean =
        0.012;

    output.absolute_return_standard_deviation =
        0.006;

    output.absolute_return_z_score =
        0.80;

    output.absolute_return_median =
        0.010;

    output.absolute_return_mad =
        0.005;

    output.absolute_return_robust_z_score =
        0.70;


    output.extreme_state =
        StatisticalExtremeState::NORMAL;

    output.observation_count =
        60;

    output.has_observation =
        true;

    output.ready =
        true;

    return output;
}


ReturnStatisticalFeatures makeReturnFeatures()
{
    ReturnStatisticalFeatures output;

    output.symbol =
        "RELIANCE";

    output.decision_time =
        DECISION_TIME;

    output.one_minute =
        makeReturnHorizon(0.010);

    output.five_minute =
        makeReturnHorizon(0.020);

    output.fifteen_minute =
        makeReturnHorizon(0.030);

    output.one_minute_ready =
        true;

    output.five_minute_ready =
        true;

    output.fifteen_minute_ready =
        true;

    output.fully_ready =
        true;

    return output;
}


// ============================================================================
// Phase 4.3 artificial data
// ============================================================================

VolumeMetricStatistics makeVolumeMetric(
    double value,
    double z_score)
{
    VolumeMetricStatistics output;

    output.value =
        value;

    output.rolling_mean =
        value * 0.90;

    output.rolling_standard_deviation =
        value * 0.10;

    output.z_score =
        z_score;

    output.rolling_median =
        value * 0.88;

    output.rolling_mad =
        value * 0.08;

    output.robust_z_score =
        z_score * 0.90;

    output.observation_count =
        60;

    output.has_observation =
        true;

    output.ready =
        true;

    return output;
}


VolumeHorizonStatistics makeVolumeHorizon(
    double volume)
{
    VolumeHorizonStatistics output;

    output.volume =
        makeVolumeMetric(
            volume,
            0.50);

    output.rvol20 =
        makeVolumeMetric(
            1.20,
            0.40);

    output.rvol50 =
        makeVolumeMetric(
            1.10,
            0.30);

    output.has_volume =
        true;

    output.has_rvol20 =
        true;

    output.has_rvol50 =
        true;

    output.volume_ready =
        true;

    output.rvol20_ready =
        true;

    output.rvol50_ready =
        true;

    output.fully_ready =
        true;

    output.abnormal_volume_state =
        AbnormalVolumeState::NORMAL;

    return output;
}


VolumeStatisticalFeatures makeVolumeFeatures()
{
    VolumeStatisticalFeatures output;

    output.symbol =
        "RELIANCE";

    output.decision_time =
        DECISION_TIME;

    output.one_minute =
        makeVolumeHorizon(100000.0);

    output.five_minute =
        makeVolumeHorizon(500000.0);

    output.fifteen_minute =
        makeVolumeHorizon(1500000.0);

    output.one_minute_ready =
        true;

    output.five_minute_ready =
        true;

    output.fifteen_minute_ready =
        true;

    output.fully_ready =
        true;

    return output;
}


// ============================================================================
// Phase 4.4 artificial data
// ============================================================================

StockBenchmarkHorizonStatistics
makeStockBenchmarkHorizon(
    double stock_return,
    double benchmark_return)
{
    StockBenchmarkHorizonStatistics output;

    output.stock_return =
        stock_return;

    output.benchmark_return =
        benchmark_return;

    output.relative_return =
        stock_return -
        benchmark_return;


    output.rolling_beta =
        1.10;

    output.rolling_alpha =
        0.001;

    output.rolling_correlation =
        0.80;


    output.residual_return =
        0.002;

    output.residual_rolling_mean =
        0.001;

    output.residual_rolling_standard_deviation =
        0.004;

    output.residual_z_score =
        0.50;

    output.residual_rolling_median =
        0.001;

    output.residual_rolling_mad =
        0.003;

    output.residual_robust_z_score =
        0.45;


    output.relative_context =
        RelativeStatisticalContext::POSITIVE;


    output.paired_observation_count =
        60;

    output.residual_observation_count =
        60;


    output.has_observation =
        true;

    output.regression_ready =
        true;

    output.residual_statistics_ready =
        true;

    output.fully_ready =
        true;

    return output;
}


StockBenchmarkStatisticalFeatures
makeStockBenchmarkFeatures()
{
    StockBenchmarkStatisticalFeatures output;

    output.stock_symbol =
        "RELIANCE";

    output.benchmark_symbol =
        "NIFTY%2050";

    output.decision_time =
        DECISION_TIME;


    output.one_minute =
        makeStockBenchmarkHorizon(
            0.010,
            0.007);

    output.five_minute =
        makeStockBenchmarkHorizon(
            0.020,
            0.015);

    output.fifteen_minute =
        makeStockBenchmarkHorizon(
            0.030,
            0.022);


    output.one_minute_ready =
        true;

    output.five_minute_ready =
        true;

    output.fifteen_minute_ready =
        true;

    output.fully_ready =
        true;

    return output;
}


// ============================================================================
// Phase 4.5 artificial data
// ============================================================================

MarketRegimeHorizon makeRegimeHorizon(
    TrendState trend,
    MarketRegime regime)
{
    MarketRegimeHorizon output;

    output.trend_state =
        trend;

    output.volatility_state =
        VolatilityState::NORMAL;

    output.statistical_stability =
        StatisticalStability::STABLE;

    output.regime =
        regime;


    output.trend_score =
        trend == TrendState::STRONG_UPTREND
            ? 4
            : trend == TrendState::UPTREND
                ? 2
                : trend == TrendState::STRONG_DOWNTREND
                    ? -4
                    : trend == TrendState::DOWNTREND
                        ? -2
                        : 0;


    output.atr14_percent =
        0.012;

    output.realized_volatility_20 =
        0.009;

    output.return_z_score =
        0.75;

    output.absolute_return_z_score =
        0.80;

    output.residual_z_score =
        0.50;

    output.rolling_correlation =
        0.80;


    output.trend_ready =
        true;

    output.volatility_ready =
        true;

    output.statistical_stability_ready =
        true;

    output.regime_ready =
        true;

    return output;
}


MarketRegimeFeatures makeRegimeFeatures()
{
    MarketRegimeFeatures output;

    output.stock_symbol =
        "RELIANCE";

    output.benchmark_symbol =
        "NIFTY%2050";

    output.decision_time =
        DECISION_TIME;


    output.one_minute =
        makeRegimeHorizon(
            TrendState::STRONG_UPTREND,
            MarketRegime::TRENDING_UP);

    output.five_minute =
        makeRegimeHorizon(
            TrendState::UPTREND,
            MarketRegime::TRENDING_UP);

    output.fifteen_minute =
        makeRegimeHorizon(
            TrendState::NEUTRAL,
            MarketRegime::RANGE_BOUND);


    output.one_minute_ready =
        true;

    output.five_minute_ready =
        true;

    output.fifteen_minute_ready =
        true;

    output.fully_ready =
        true;

    return output;
}


// ============================================================================
// Base input bundle
// ============================================================================

struct Inputs
{
    ReturnStatisticalFeatures
        returns;

    VolumeStatisticalFeatures
        volume;

    StockBenchmarkStatisticalFeatures
        stock_benchmark;

    MarketRegimeFeatures
        regime;
};


Inputs makeInputs()
{
    Inputs input;

    input.returns =
        makeReturnFeatures();

    input.volume =
        makeVolumeFeatures();

    input.stock_benchmark =
        makeStockBenchmarkFeatures();

    input.regime =
        makeRegimeFeatures();

    return input;
}


// ============================================================================
// Compose helper
// ============================================================================

MathematicalStateFeatures compose(
    const MathematicalStateComposer& composer,
    const Inputs& input)
{
    return
        composer.compose(
            input.returns,
            input.volume,
            input.stock_benchmark,
            input.regime);
}


// ============================================================================
// Exception helper
// ============================================================================

template<typename Function>
bool throwsException(Function&& function)
{
    try
    {
        function();
    }
    catch (const std::exception&)
    {
        return true;
    }

    return false;
}


// ============================================================================
// TEST 1 — Constructor validation
// ============================================================================

void testConstructorValidation(
    TestReport& report)
{
    if (!throwsException(
            []()
            {
                MathematicalStateComposer composer(
                    "",
                    "NIFTY%2050");
            }))
    {
        report.constructor_validation =
            false;
    }


    if (!throwsException(
            []()
            {
                MathematicalStateComposer composer(
                    "RELIANCE",
                    "");
            }))
    {
        report.constructor_validation =
            false;
    }


    if (!throwsException(
            []()
            {
                MathematicalStateComposer composer(
                    "RELIANCE",
                    "RELIANCE");
            }))
    {
        report.constructor_validation =
            false;
    }


    try
    {
        MathematicalStateComposer composer(
            "RELIANCE",
            "NIFTY%2050");

        if (composer.stockSymbol() !=
                "RELIANCE" ||

            composer.benchmarkSymbol() !=
                "NIFTY%2050")
        {
            report.constructor_validation =
                false;
        }
    }
    catch (...)
    {
        report.constructor_validation =
            false;
    }
}


// ============================================================================
// TEST 2 — Fully-ready composition and metadata
// ============================================================================

void testFullComposition(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    const Inputs input =
        makeInputs();

    const auto result =
        compose(
            composer,
            input);


    if (result.stock_symbol !=
            "RELIANCE" ||

        result.benchmark_symbol !=
            "NIFTY%2050" ||

        result.decision_time !=
            DECISION_TIME)
    {
        report.metadata_composition =
            false;
    }


    if (!result.one_minute_ready ||
        !result.five_minute_ready ||
        !result.fifteen_minute_ready ||
        !result.numerical_integrity ||
        !result.fully_ready)
    {
        report.full_readiness =
            false;
    }


    // ------------------------------------------------------------------------
    // Return data must be copied without recalculation.
    // ------------------------------------------------------------------------

    if (!approximatelyEqual(
            result.one_minute.
                return_statistics.return_value,
            input.returns.
                one_minute.return_value) ||

        !approximatelyEqual(
            result.five_minute.
                return_statistics.z_score,
            input.returns.
                five_minute.z_score) ||

        !approximatelyEqual(
            result.fifteen_minute.
                return_statistics.robust_z_score,
            input.returns.
                fifteen_minute.robust_z_score))
    {
        report.lossless_return_composition =
            false;
    }


    // ------------------------------------------------------------------------
    // Volume data
    // ------------------------------------------------------------------------

    if (!approximatelyEqual(
            result.one_minute.
                volume_statistics.volume.z_score,
            input.volume.
                one_minute.volume.z_score) ||

        !approximatelyEqual(
            result.five_minute.
                volume_statistics.rvol20.z_score,
            input.volume.
                five_minute.rvol20.z_score) ||

        !approximatelyEqual(
            result.fifteen_minute.
                volume_statistics.rvol50.robust_z_score,
            input.volume.
                fifteen_minute.rvol50.robust_z_score))
    {
        report.lossless_volume_composition =
            false;
    }


    // ------------------------------------------------------------------------
    // Stock / benchmark data
    // ------------------------------------------------------------------------

    if (!approximatelyEqual(
            result.one_minute.
                stock_benchmark_statistics.rolling_beta,
            input.stock_benchmark.
                one_minute.rolling_beta) ||

        !approximatelyEqual(
            result.five_minute.
                stock_benchmark_statistics.residual_z_score,
            input.stock_benchmark.
                five_minute.residual_z_score) ||

        !approximatelyEqual(
            result.fifteen_minute.
                stock_benchmark_statistics.rolling_correlation,
            input.stock_benchmark.
                fifteen_minute.rolling_correlation))
    {
        report.lossless_stock_benchmark_composition =
            false;
    }


    // ------------------------------------------------------------------------
    // Market regime
    // ------------------------------------------------------------------------

    if (result.one_minute.
            market_regime.regime !=
            input.regime.
                one_minute.regime ||

        result.five_minute.
            market_regime.trend_state !=
            input.regime.
                five_minute.trend_state ||

        result.fifteen_minute.
            market_regime.regime !=
            input.regime.
                fifteen_minute.regime)
    {
        report.lossless_regime_composition =
            false;
    }
}


// ============================================================================
// TEST 3 — Partial readiness
//
// Make 5-minute volume statistics legitimately not ready.
//
// The 1m and 15m states must remain ready.
// ============================================================================

void testPartialReadiness(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.volume.five_minute.rvol50.ready =
        false;

    input.volume.five_minute.rvol50_ready =
        false;

    input.volume.five_minute.fully_ready =
        false;

    input.volume.five_minute_ready =
        false;

    input.volume.fully_ready =
        false;


    const auto result =
        compose(
            composer,
            input);


    if (!result.one_minute_ready ||
        result.five_minute_ready ||
        !result.fifteen_minute_ready ||
        result.fully_ready)
    {
        report.partial_readiness =
            false;
    }


    if (!result.one_minute.
            readiness.fully_ready ||

        result.five_minute.
            readiness.fully_ready ||

        !result.fifteen_minute.
            readiness.fully_ready)
    {
        report.timeframe_independence =
            false;
    }


    if (!result.numerical_integrity)
    {
        report.partial_readiness =
            false;
    }
}


// ============================================================================
// TEST 4 — Warm-up NaN is allowed
//
// An unavailable metric may legitimately contain NaN.
//
// This is different from an AVAILABLE metric containing NaN.
// ============================================================================

void testWarmupNaN(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    const double nan =
        std::numeric_limits<double>::quiet_NaN();


    input.volume.five_minute.rvol50 =
        VolumeMetricStatistics{};

    input.volume.five_minute.rvol50.value =
        nan;

    input.volume.five_minute.rvol50.has_observation =
        false;

    input.volume.five_minute.rvol50.ready =
        false;

    input.volume.five_minute.has_rvol50 =
        false;

    input.volume.five_minute.rvol50_ready =
        false;

    input.volume.five_minute.fully_ready =
        false;

    input.volume.five_minute_ready =
        false;

    input.volume.fully_ready =
        false;


    const auto result =
        compose(
            composer,
            input);


    if (!result.numerical_integrity ||
        result.five_minute_ready ||
        result.fully_ready)
    {
        report.warmup_nan_allowed =
            false;
    }
}


// ============================================================================
// TEST 5 — Available NaN must fail numerical integrity
// ============================================================================

void testAvailableNaN(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.returns.one_minute.z_score =
        std::numeric_limits<double>::quiet_NaN();


    const auto result =
        compose(
            composer,
            input);


    if (result.one_minute.
            readiness.numerical_integrity ||

        result.one_minute_ready ||

        result.numerical_integrity ||

        result.fully_ready)
    {
        report.available_nan_rejected =
            false;
    }
}


// ============================================================================
// TEST 6 — Negative standard deviation must fail
// ============================================================================

void testInvalidStandardDeviation(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.volume.one_minute.
        volume.rolling_standard_deviation =
            -1.0;


    const auto result =
        compose(
            composer,
            input);


    if (result.one_minute.
            readiness.numerical_integrity ||

        result.numerical_integrity ||
        result.fully_ready)
    {
        report.invalid_stddev_rejected =
            false;
    }
}


// ============================================================================
// TEST 7 — Correlation outside [-1, 1] must fail
// ============================================================================

void testInvalidCorrelation(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.stock_benchmark.fifteen_minute.
        rolling_correlation =
            1.50;


    const auto result =
        compose(
            composer,
            input);


    if (result.fifteen_minute.
            readiness.numerical_integrity ||

        result.numerical_integrity ||
        result.fully_ready)
    {
        report.invalid_correlation_rejected =
            false;
    }
}


// ============================================================================
// TEST 8 — Invalid trend score
// ============================================================================

void testInvalidTrendScore(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.regime.five_minute.trend_score =
        5;


    const auto result =
        compose(
            composer,
            input);


    if (result.five_minute.
            readiness.numerical_integrity ||

        result.numerical_integrity ||
        result.fully_ready)
    {
        report.invalid_trend_score_rejected =
            false;
    }
}


// ============================================================================
// TEST 9 — Return readiness
// ============================================================================

void testReturnReadiness(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.returns.one_minute.ready =
        false;

    input.returns.one_minute_ready =
        false;

    input.returns.fully_ready =
        false;


    const auto result =
        compose(
            composer,
            input);


    if (result.one_minute.
            readiness.return_statistics_ready ||

        result.one_minute_ready ||
        result.fully_ready)
    {
        report.return_readiness_integrity =
            false;
    }
}


// ============================================================================
// TEST 10 — Volume readiness structural integrity
//
// Deliberately create an impossible Phase 4.3 state:
// rvol50 metric is not ready but horizon claims it is ready.
// Numerical integrity must reject it.
// ============================================================================

void testVolumeReadinessIntegrity(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.volume.one_minute.rvol50.ready =
        false;

    // Deliberately leave:
    //     rvol50_ready = true
    //     fully_ready  = true


    const auto result =
        compose(
            composer,
            input);


    if (result.one_minute.
            readiness.numerical_integrity ||

        result.numerical_integrity ||
        result.fully_ready)
    {
        report.volume_readiness_integrity =
            false;
    }
}


// ============================================================================
// TEST 11 — Stock/NIFTY readiness structural integrity
// ============================================================================

void testStockBenchmarkReadinessIntegrity(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.stock_benchmark.one_minute.
        residual_statistics_ready =
            false;

    // Deliberately leave fully_ready = true.


    const auto result =
        compose(
            composer,
            input);


    if (result.one_minute.
            readiness.numerical_integrity ||

        result.numerical_integrity ||
        result.fully_ready)
    {
        report.stock_benchmark_readiness_integrity =
            false;
    }
}


// ============================================================================
// TEST 12 — Market-regime readiness structural integrity
// ============================================================================

void testRegimeReadinessIntegrity(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    Inputs input =
        makeInputs();


    input.regime.one_minute.regime =
        MarketRegime::UNAVAILABLE;

    // Deliberately leave regime_ready = true.


    const auto result =
        compose(
            composer,
            input);


    if (result.one_minute.
            readiness.numerical_integrity ||

        result.numerical_integrity ||
        result.fully_ready)
    {
        report.regime_readiness_integrity =
            false;
    }
}


// ============================================================================
// TEST 13 — Symbol protection
// ============================================================================

void testSymbolProtection(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");


    {
        Inputs input =
            makeInputs();

        input.returns.symbol =
            "WRONG";

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.return_symbol_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.volume.symbol =
            "WRONG";

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.volume_symbol_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.stock_benchmark.stock_symbol =
            "WRONG";

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.stock_symbol_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.stock_benchmark.benchmark_symbol =
            "WRONG";

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.benchmark_symbol_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.regime.stock_symbol =
            "WRONG";

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.regime_stock_symbol_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.regime.benchmark_symbol =
            "WRONG";

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.regime_benchmark_symbol_protection =
                false;
        }
    }
}


// ============================================================================
// TEST 14 — Decision-time protection
// ============================================================================

void testDecisionTimeProtection(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");


    {
        Inputs input =
            makeInputs();

        input.volume.decision_time +=
            60;

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.volume_decision_time_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.stock_benchmark.decision_time +=
            60;

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.stock_benchmark_decision_time_protection =
                false;
        }
    }


    {
        Inputs input =
            makeInputs();

        input.regime.decision_time +=
            60;

        if (!throwsException(
                [&]()
                {
                    compose(
                        composer,
                        input);
                }))
        {
            report.regime_decision_time_protection =
                false;
        }
    }
}


// ============================================================================
// TEST 15 — Stateless / deterministic composition
//
// Calling the composer twice with exactly the same Phase-4 state must produce
// the same result. The composer has no rolling state and performs no update.
// ============================================================================

void testStatelessComposition(
    TestReport& report)
{
    MathematicalStateComposer composer(
        "RELIANCE",
        "NIFTY%2050");

    const Inputs input =
        makeInputs();


    const auto first =
        compose(
            composer,
            input);

    const auto second =
        compose(
            composer,
            input);


    if (first.stock_symbol !=
            second.stock_symbol ||

        first.benchmark_symbol !=
            second.benchmark_symbol ||

        first.decision_time !=
            second.decision_time ||

        first.fully_ready !=
            second.fully_ready ||

        first.numerical_integrity !=
            second.numerical_integrity ||

        !approximatelyEqual(
            first.one_minute.
                return_statistics.z_score,
            second.one_minute.
                return_statistics.z_score) ||

        !approximatelyEqual(
            first.five_minute.
                volume_statistics.volume.z_score,
            second.five_minute.
                volume_statistics.volume.z_score) ||

        !approximatelyEqual(
            first.fifteen_minute.
                stock_benchmark_statistics.
                    residual_z_score,
            second.fifteen_minute.
                stock_benchmark_statistics.
                    residual_z_score) ||

        first.one_minute.
            market_regime.regime !=
            second.one_minute.
                market_regime.regime)
    {
        report.deterministic_stateless_composition =
            false;
    }
}


// ============================================================================
// Reporting
// ============================================================================

void printResult(
    const std::string& name,
    bool passed)
{
    std::cout
        << name
        << " : "
        << (passed ? "PASSED" : "FAILED")
        << '\n';
}


void printReport(
    const TestReport& report)
{
    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.6 — MATHEMATICAL STATE COMPOSER\n"
        << "UNIT / ARTIFICIAL VALIDATION\n"
        << "============================================================\n";


    printResult(
        "Constructor validation             ",
        report.constructor_validation);

    printResult(
        "Metadata composition               ",
        report.metadata_composition);

    printResult(
        "Lossless return composition        ",
        report.lossless_return_composition);

    printResult(
        "Lossless volume composition        ",
        report.lossless_volume_composition);

    printResult(
        "Lossless Stock/NIFTY composition   ",
        report.lossless_stock_benchmark_composition);

    printResult(
        "Lossless regime composition        ",
        report.lossless_regime_composition);

    printResult(
        "Full readiness                     ",
        report.full_readiness);

    printResult(
        "Partial readiness                  ",
        report.partial_readiness);

    printResult(
        "Timeframe independence             ",
        report.timeframe_independence);

    printResult(
        "Warm-up NaN handling               ",
        report.warmup_nan_allowed);

    printResult(
        "Available NaN rejection            ",
        report.available_nan_rejected);

    printResult(
        "Negative stddev rejection          ",
        report.invalid_stddev_rejected);

    printResult(
        "Invalid correlation rejection      ",
        report.invalid_correlation_rejected);

    printResult(
        "Invalid trend-score rejection      ",
        report.invalid_trend_score_rejected);

    printResult(
        "Return readiness integrity         ",
        report.return_readiness_integrity);

    printResult(
        "Volume readiness integrity         ",
        report.volume_readiness_integrity);

    printResult(
        "Stock/NIFTY readiness integrity    ",
        report.stock_benchmark_readiness_integrity);

    printResult(
        "Regime readiness integrity         ",
        report.regime_readiness_integrity);

    printResult(
        "Return symbol protection           ",
        report.return_symbol_protection);

    printResult(
        "Volume symbol protection           ",
        report.volume_symbol_protection);

    printResult(
        "Stock symbol protection            ",
        report.stock_symbol_protection);

    printResult(
        "Benchmark symbol protection        ",
        report.benchmark_symbol_protection);

    printResult(
        "Regime stock-symbol protection     ",
        report.regime_stock_symbol_protection);

    printResult(
        "Regime benchmark protection        ",
        report.regime_benchmark_symbol_protection);

    printResult(
        "Volume decision-time protection    ",
        report.volume_decision_time_protection);

    printResult(
        "Stock/NIFTY decision-time protect. ",
        report.stock_benchmark_decision_time_protection);

    printResult(
        "Regime decision-time protection    ",
        report.regime_decision_time_protection);

    printResult(
        "Stateless deterministic composer   ",
        report.deterministic_stateless_composition);


    std::cout
        << "============================================================\n";


    if (report.passed())
    {
        std::cout
            << "PHASE 4.6 MATHEMATICAL STATE COMPOSER PASSED\n";
    }
    else
    {
        std::cout
            << "PHASE 4.6 MATHEMATICAL STATE COMPOSER FAILED\n";
    }


    std::cout
        << "============================================================\n";
}

} // namespace


// ============================================================================
// main
// ============================================================================

int main()
{
    TestReport report;


    try
    {
        testConstructorValidation(
            report);

        testFullComposition(
            report);

        testPartialReadiness(
            report);

        testWarmupNaN(
            report);

        testAvailableNaN(
            report);

        testInvalidStandardDeviation(
            report);

        testInvalidCorrelation(
            report);

        testInvalidTrendScore(
            report);

        testReturnReadiness(
            report);

        testVolumeReadinessIntegrity(
            report);

        testStockBenchmarkReadinessIntegrity(
            report);

        testRegimeReadinessIntegrity(
            report);

        testSymbolProtection(
            report);

        testDecisionTimeProtection(
            report);

        testStatelessComposition(
            report);
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nUnexpected test exception:\n"
            << exception.what()
            << '\n';

        return
            EXIT_FAILURE;
    }


    printReport(
        report);


    return
        report.passed()
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
}