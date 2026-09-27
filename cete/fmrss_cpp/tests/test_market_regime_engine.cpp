#include "devai/statistics/MarketRegimeEngine.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>


using namespace devai::features;
using namespace devai::statistics;


namespace
{

constexpr std::int64_t DECISION_TIME =
    1'800'000'000;


// ============================================================================
// Helpers
// ============================================================================

TimeframeTrendMomentumFeatures makeTrend(
    double price_vs_ema20,
    double ema20_vs_ema50,
    double ema20_slope,
    double ema50_slope)
{
    TimeframeTrendMomentumFeatures value;

    value.ema20 =
        100.0;

    value.ema50 =
        99.0;

    value.rsi14 =
        55.0;

    value.price_vs_ema20 =
        price_vs_ema20;

    value.price_vs_ema50 =
        price_vs_ema20;

    value.ema20_vs_ema50 =
        ema20_vs_ema50;

    value.ema20_slope =
        ema20_slope;

    value.ema50_slope =
        ema50_slope;

    value.has_ema20 =
        true;

    value.has_ema50 =
        true;

    value.has_rsi14 =
        true;

    value.has_ema20_slope =
        true;

    value.has_ema50_slope =
        true;

    return value;
}


TimeframeVolatilityFeatures makeVolatility(
    double atr14_percent = 0.50,
    double realized_volatility_20 = 0.005)
{
    TimeframeVolatilityFeatures value;

    value.atr14 =
        0.50;

    value.atr14_percent =
        atr14_percent;

    value.realized_volatility_20 =
        realized_volatility_20;

    value.has_atr14 =
        true;

    value.has_atr14_percent =
        true;

    value.has_realized_volatility_20 =
        true;

    return value;
}


ReturnHorizonStatistics makeReturnStatistics(
    double return_z,
    double robust_return_z,
    double absolute_return_z)
{
    ReturnHorizonStatistics value;

    value.return_value =
        0.001;

    value.absolute_return =
        0.001;

    value.z_score =
        return_z;

    value.robust_z_score =
        robust_return_z;

    value.absolute_return_z_score =
        absolute_return_z;

    value.has_observation =
        true;

    value.observation_count =
        60;

    value.ready =
        true;

    return value;
}


StockBenchmarkHorizonStatistics
makeStockBenchmarkStatistics(
    double residual_z,
    double residual_robust_z,
    double correlation = 0.75)
{
    StockBenchmarkHorizonStatistics value;

    value.stock_return =
        0.002;

    value.benchmark_return =
        0.001;

    value.relative_return =
        0.001;

    value.rolling_beta =
        1.10;

    value.rolling_alpha =
        0.0001;

    value.rolling_correlation =
        correlation;

    value.residual_return =
        0.0008;

    value.residual_rolling_mean =
        0.0001;

    value.residual_rolling_standard_deviation =
        0.001;

    value.residual_z_score =
        residual_z;

    value.residual_rolling_median =
        0.0001;

    value.residual_rolling_mad =
        0.0008;

    value.residual_robust_z_score =
        residual_robust_z;

    value.has_observation =
        true;

    value.paired_observation_count =
        60;

    value.residual_observation_count =
        60;

    value.regression_ready =
        true;

    value.residual_statistics_ready =
        true;

    value.fully_ready =
        true;

    return value;
}


// ============================================================================
// Complete input helper
// ============================================================================

struct Inputs
{
    TrendMomentumFeatures trend;
    VolatilityFeatures volatility;
    ReturnStatisticalFeatures returns;
    StockBenchmarkStatisticalFeatures benchmark;
};


Inputs makeInputs()
{
    Inputs input;


    input.trend.symbol =
        "RELIANCE";

    input.trend.decision_time =
        DECISION_TIME;


    input.volatility.symbol =
        "RELIANCE";

    input.volatility.decision_time =
        DECISION_TIME;


    input.returns.symbol =
        "RELIANCE";

    input.returns.decision_time =
        DECISION_TIME;


    input.benchmark.stock_symbol =
        "RELIANCE";

    input.benchmark.benchmark_symbol =
        "NIFTY%2050";

    input.benchmark.decision_time =
        DECISION_TIME;


    // ------------------------------------------------------------------------
    // Default state:
    //
    // Strong uptrend
    // Normal volatility
    // Stable statistics
    // ------------------------------------------------------------------------

    const auto trend =
        makeTrend(
            0.01,
            0.01,
            0.001,
            0.0005);


    const auto volatility =
        makeVolatility();


    const auto returns =
        makeReturnStatistics(
            0.30,
            0.40,
            0.20);


    const auto benchmark =
        makeStockBenchmarkStatistics(
            0.30,
            0.40);


    input.trend.one_minute =
        trend;

    input.trend.five_minute =
        trend;

    input.trend.fifteen_minute =
        trend;


    input.volatility.one_minute =
        volatility;

    input.volatility.five_minute =
        volatility;

    input.volatility.fifteen_minute =
        volatility;


    input.returns.one_minute =
        returns;

    input.returns.five_minute =
        returns;

    input.returns.fifteen_minute =
        returns;


    input.returns.one_minute_ready =
        true;

    input.returns.five_minute_ready =
        true;

    input.returns.fifteen_minute_ready =
        true;

    input.returns.fully_ready =
        true;


    input.benchmark.one_minute =
        benchmark;

    input.benchmark.five_minute =
        benchmark;

    input.benchmark.fifteen_minute =
        benchmark;


    input.benchmark.one_minute_ready =
        true;

    input.benchmark.five_minute_ready =
        true;

    input.benchmark.fifteen_minute_ready =
        true;

    input.benchmark.fully_ready =
        true;


    return input;
}


// ============================================================================
// Update helper
// ============================================================================

MarketRegimeFeatures run(
    MarketRegimeEngine& engine,
    const Inputs& input)
{
    return
        engine.update(
            input.trend,
            input.volatility,
            input.returns,
            input.benchmark);
}


// ============================================================================
// 1. Strong Uptrend
// ============================================================================

void testStrongUptrend()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_score ==
        4);


    assert(
        output.one_minute.trend_state ==
        TrendState::STRONG_UPTREND);


    assert(
        output.one_minute.regime ==
        MarketRegime::TRENDING_UP);
}


// ============================================================================
// 2. Strong Downtrend
// ============================================================================

void testStrongDowntrend()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.trend.one_minute =
        makeTrend(
            -0.01,
            -0.01,
            -0.001,
            -0.0005);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_score ==
        -4);


    assert(
        output.one_minute.trend_state ==
        TrendState::STRONG_DOWNTREND);


    assert(
        output.one_minute.regime ==
        MarketRegime::TRENDING_DOWN);
}


// ============================================================================
// 3. Ordinary Uptrend
// ============================================================================

void testUptrend()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.trend.one_minute =
        makeTrend(
            0.01,
            0.01,
            -0.001,
            0.0005);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_score ==
        2);


    assert(
        output.one_minute.trend_state ==
        TrendState::UPTREND);


    assert(
        output.one_minute.regime ==
        MarketRegime::TRENDING_UP);
}


// ============================================================================
// 4. Neutral / Range Bound
// ============================================================================

void testRangeBound()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.trend.one_minute =
        makeTrend(
            0.01,
            -0.01,
            0.001,
            -0.001);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_score ==
        0);


    assert(
        output.one_minute.trend_state ==
        TrendState::NEUTRAL);


    assert(
        output.one_minute.regime ==
        MarketRegime::RANGE_BOUND);
}


// ============================================================================
// 5. Low Volatility
// ============================================================================

void testLowVolatility()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            0.20,
            0.30,
            -1.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.volatility_state ==
        VolatilityState::LOW);
}


// ============================================================================
// 6. Normal Volatility
// ============================================================================

void testNormalVolatility()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    const auto input =
        makeInputs();


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.volatility_state ==
        VolatilityState::NORMAL);
}


// ============================================================================
// 7. High Volatility
// ============================================================================

void testHighVolatility()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            0.20,
            0.30,
            1.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.volatility_state ==
        VolatilityState::HIGH);


    assert(
        output.one_minute.regime ==
        MarketRegime::HIGH_VOLATILITY);
}


// ============================================================================
// 8. Extreme Volatility
// ============================================================================

void testExtremeVolatility()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            0.20,
            0.30,
            2.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.volatility_state ==
        VolatilityState::EXTREME);


    assert(
        output.one_minute.regime ==
        MarketRegime::HIGH_VOLATILITY);
}


// ============================================================================
// 9. Stable Statistics
// ============================================================================

void testStableStatistics()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    const auto input =
        makeInputs();


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::STABLE);
}


// ============================================================================
// 10. Elevated Statistics
// ============================================================================

void testElevatedStatistics()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            1.40,
            0.40,
            0.20);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::ELEVATED);
}


// ============================================================================
// 11. Unstable from return Z
// ============================================================================

void testUnstableReturn()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            2.50,
            0.50,
            0.20);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::UNSTABLE);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNSTABLE);
}


// ============================================================================
// 12. Unstable from robust return Z
// ============================================================================

void testUnstableRobustReturn()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            0.40,
            2.50,
            0.20);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::UNSTABLE);
}


// ============================================================================
// 13. Unstable from residual Z
// ============================================================================

void testUnstableResidual()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.benchmark.one_minute =
        makeStockBenchmarkStatistics(
            2.50,
            0.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::UNSTABLE);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNSTABLE);
}


// ============================================================================
// 14. Unstable from robust residual Z
// ============================================================================

void testUnstableRobustResidual()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.benchmark.one_minute =
        makeStockBenchmarkStatistics(
            0.40,
            -2.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::UNSTABLE);
}


// ============================================================================
// 15. Priority — instability overrides trend
// ============================================================================

void testUnstableOverridesTrend()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    // Strong uptrend remains present.

    input.benchmark.one_minute =
        makeStockBenchmarkStatistics(
            3.00,
            0.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_state ==
        TrendState::STRONG_UPTREND);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNSTABLE);
}


// ============================================================================
// 16. Priority — high volatility overrides trend
// ============================================================================

void testHighVolatilityOverridesTrend()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        makeReturnStatistics(
            0.30,
            0.40,
            1.50);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_state ==
        TrendState::STRONG_UPTREND);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::STABLE);


    assert(
        output.one_minute.regime ==
        MarketRegime::HIGH_VOLATILITY);
}


// ============================================================================
// 17. Independent timeframes
// ============================================================================

void testIndependentTimeframes()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    // 1m strong uptrend.

    input.trend.one_minute =
        makeTrend(
            0.01,
            0.01,
            0.001,
            0.001);


    // 5m strong downtrend.

    input.trend.five_minute =
        makeTrend(
            -0.01,
            -0.01,
            -0.001,
            -0.001);


    // 15m neutral.

    input.trend.fifteen_minute =
        makeTrend(
            0.01,
            -0.01,
            0.001,
            -0.001);


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.regime ==
        MarketRegime::TRENDING_UP);


    assert(
        output.five_minute.regime ==
        MarketRegime::TRENDING_DOWN);


    assert(
        output.fifteen_minute.regime ==
        MarketRegime::RANGE_BOUND);
}


// ============================================================================
// 18. Trend warm-up / unavailable
// ============================================================================

void testTrendUnavailable()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.trend.one_minute =
        TimeframeTrendMomentumFeatures{};


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.trend_state ==
        TrendState::UNAVAILABLE);


    assert(
        !output.one_minute.trend_ready);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNAVAILABLE);


    assert(
        !output.one_minute.regime_ready);
}


// ============================================================================
// 19. Volatility unavailable
// ============================================================================

void testVolatilityUnavailable()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.volatility.one_minute =
        TimeframeVolatilityFeatures{};


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.volatility_state ==
        VolatilityState::UNAVAILABLE);


    assert(
        !output.one_minute.volatility_ready);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNAVAILABLE);
}


// ============================================================================
// 20. Return statistics unavailable
// ============================================================================

void testReturnStatisticsUnavailable()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.one_minute =
        ReturnHorizonStatistics{};


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.volatility_state ==
        VolatilityState::UNAVAILABLE);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::UNAVAILABLE);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNAVAILABLE);
}


// ============================================================================
// 21. Phase 4.4 unavailable
// ============================================================================

void testStockBenchmarkUnavailable()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.benchmark.one_minute =
        StockBenchmarkHorizonStatistics{};


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute.statistical_stability ==
        StatisticalStability::UNAVAILABLE);


    assert(
        output.one_minute.regime ==
        MarketRegime::UNAVAILABLE);
}


// ============================================================================
// 22. Full readiness
// ============================================================================

void testFullReadiness()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    const auto input =
        makeInputs();


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute_ready);

    assert(
        output.five_minute_ready);

    assert(
        output.fifteen_minute_ready);

    assert(
        output.fully_ready);
}


// ============================================================================
// 23. Partial readiness
// ============================================================================

void testPartialReadiness()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.benchmark.fifteen_minute =
        StockBenchmarkHorizonStatistics{};


    const auto output =
        run(
            engine,
            input);


    assert(
        output.one_minute_ready);

    assert(
        output.five_minute_ready);

    assert(
        !output.fifteen_minute_ready);

    assert(
        !output.fully_ready);
}


// ============================================================================
// 24. Evidence propagation
// ============================================================================

void testEvidencePropagation()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.volatility.one_minute.atr14_percent =
        0.72;

    input.volatility.one_minute.realized_volatility_20 =
        0.0085;


    input.returns.one_minute.z_score =
        0.60;

    input.returns.one_minute.absolute_return_z_score =
        0.80;


    input.benchmark.one_minute.residual_z_score =
        -0.70;

    input.benchmark.one_minute.rolling_correlation =
        0.82;


    const auto output =
        run(
            engine,
            input);


    assert(
        std::fabs(
            output.one_minute.atr14_percent -
            0.72) <
        1.0e-12);


    assert(
        std::fabs(
            output.one_minute.realized_volatility_20 -
            0.0085) <
        1.0e-12);


    assert(
        std::fabs(
            output.one_minute.return_z_score -
            0.60) <
        1.0e-12);


    assert(
        std::fabs(
            output.one_minute.absolute_return_z_score -
            0.80) <
        1.0e-12);


    assert(
        std::fabs(
            output.one_minute.residual_z_score -
            (-0.70)) <
        1.0e-12);


    assert(
        std::fabs(
            output.one_minute.rolling_correlation -
            0.82) <
        1.0e-12);
}


// ============================================================================
// 25. Symbol protection — trend
// ============================================================================

void testTrendSymbolProtection()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.trend.symbol =
        "SBIN";


    bool threw =
        false;


    try
    {
        static_cast<void>(
            run(
                engine,
                input));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    assert(threw);
}


// ============================================================================
// 26. Symbol protection — volatility
// ============================================================================

void testVolatilitySymbolProtection()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.volatility.symbol =
        "SBIN";


    bool threw =
        false;


    try
    {
        static_cast<void>(
            run(
                engine,
                input));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    assert(threw);
}


// ============================================================================
// 27. Symbol protection — return statistics
// ============================================================================

void testReturnSymbolProtection()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.symbol =
        "SBIN";


    bool threw =
        false;


    try
    {
        static_cast<void>(
            run(
                engine,
                input));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    assert(threw);
}


// ============================================================================
// 28. Phase 4.4 stock protection
// ============================================================================

void testBenchmarkStockProtection()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.benchmark.stock_symbol =
        "SBIN";


    bool threw =
        false;


    try
    {
        static_cast<void>(
            run(
                engine,
                input));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    assert(threw);
}


// ============================================================================
// 29. Phase 4.4 benchmark protection
// ============================================================================

void testBenchmarkSymbolProtection()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.benchmark.benchmark_symbol =
        "OTHER";


    bool threw =
        false;


    try
    {
        static_cast<void>(
            run(
                engine,
                input));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    assert(threw);
}


// ============================================================================
// 30. Decision-time protection
// ============================================================================

void testDecisionTimeProtection()
{
    MarketRegimeEngine engine(
        "RELIANCE",
        "NIFTY%2050");


    auto input =
        makeInputs();


    input.returns.decision_time =
        DECISION_TIME + 60;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            run(
                engine,
                input));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    assert(threw);
}


// ============================================================================
// 31. Constructor protection
// ============================================================================

void testConstructorProtection()
{
    bool empty_stock_threw =
        false;

    bool empty_benchmark_threw =
        false;

    bool same_symbol_threw =
        false;

    bool bad_elevated_threw =
        false;

    bool bad_extreme_threw =
        false;


    try
    {
        MarketRegimeEngine engine(
            "",
            "NIFTY%2050");

        static_cast<void>(engine);
    }
    catch (const std::invalid_argument&)
    {
        empty_stock_threw =
            true;
    }


    try
    {
        MarketRegimeEngine engine(
            "RELIANCE",
            "");

        static_cast<void>(engine);
    }
    catch (const std::invalid_argument&)
    {
        empty_benchmark_threw =
            true;
    }


    try
    {
        MarketRegimeEngine engine(
            "RELIANCE",
            "RELIANCE");

        static_cast<void>(engine);
    }
    catch (const std::invalid_argument&)
    {
        same_symbol_threw =
            true;
    }


    try
    {
        MarketRegimeEngine engine(
            "RELIANCE",
            "NIFTY%2050",
            0.0,
            2.0);

        static_cast<void>(engine);
    }
    catch (const std::invalid_argument&)
    {
        bad_elevated_threw =
            true;
    }


    try
    {
        MarketRegimeEngine engine(
            "RELIANCE",
            "NIFTY%2050",
            2.0,
            1.0);

        static_cast<void>(engine);
    }
    catch (const std::invalid_argument&)
    {
        bad_extreme_threw =
            true;
    }


    assert(empty_stock_threw);
    assert(empty_benchmark_threw);
    assert(same_symbol_threw);
    assert(bad_elevated_threw);
    assert(bad_extreme_threw);
}


// ============================================================================
// MAIN
// ============================================================================

} // namespace


int main()
{
    testStrongUptrend();
    testStrongDowntrend();
    testUptrend();
    testRangeBound();

    testLowVolatility();
    testNormalVolatility();
    testHighVolatility();
    testExtremeVolatility();

    testStableStatistics();
    testElevatedStatistics();
    testUnstableReturn();
    testUnstableRobustReturn();
    testUnstableResidual();
    testUnstableRobustResidual();

    testUnstableOverridesTrend();
    testHighVolatilityOverridesTrend();

    testIndependentTimeframes();

    testTrendUnavailable();
    testVolatilityUnavailable();
    testReturnStatisticsUnavailable();
    testStockBenchmarkUnavailable();

    testFullReadiness();
    testPartialReadiness();

    testEvidencePropagation();

    testTrendSymbolProtection();
    testVolatilitySymbolProtection();
    testReturnSymbolProtection();
    testBenchmarkStockProtection();
    testBenchmarkSymbolProtection();

    testDecisionTimeProtection();
    testConstructorProtection();


    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.5 — MARKET REGIME ENGINE TEST\n"
        << "============================================================\n"
        << "Strong Uptrend                     : PASSED\n"
        << "Strong Downtrend                   : PASSED\n"
        << "Uptrend                            : PASSED\n"
        << "Range Bound                        : PASSED\n"
        << "Low Volatility                     : PASSED\n"
        << "Normal Volatility                  : PASSED\n"
        << "High Volatility                    : PASSED\n"
        << "Extreme Volatility                 : PASSED\n"
        << "Stable Statistics                  : PASSED\n"
        << "Elevated Statistics                : PASSED\n"
        << "Unstable Return                    : PASSED\n"
        << "Unstable Robust Return             : PASSED\n"
        << "Unstable Residual                  : PASSED\n"
        << "Unstable Robust Residual           : PASSED\n"
        << "Instability Priority               : PASSED\n"
        << "High-Volatility Priority           : PASSED\n"
        << "Independent Timeframes             : PASSED\n"
        << "Trend Warm-up                      : PASSED\n"
        << "Volatility Warm-up                 : PASSED\n"
        << "Return Statistics Warm-up          : PASSED\n"
        << "Stock/NIFTY Statistics Warm-up     : PASSED\n"
        << "Full Readiness                     : PASSED\n"
        << "Partial Readiness                  : PASSED\n"
        << "Evidence Propagation               : PASSED\n"
        << "Trend Symbol Protection            : PASSED\n"
        << "Volatility Symbol Protection       : PASSED\n"
        << "Return Symbol Protection           : PASSED\n"
        << "Stock Symbol Protection            : PASSED\n"
        << "Benchmark Symbol Protection        : PASSED\n"
        << "Decision-Time Integrity            : PASSED\n"
        << "Constructor Protection             : PASSED\n"
        << "============================================================\n"
        << "PHASE 4.5 MARKET REGIME ENGINE PASSED\n"
        << "============================================================\n";


    return 0;
}