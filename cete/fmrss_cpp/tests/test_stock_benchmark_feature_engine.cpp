#include "devai/features/StockBenchmarkFeatureEngine.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace devai::features;

namespace
{

constexpr std::int64_t DECISION_TIME = 1000;


bool approximatelyEqual(
    double left,
    double right,
    double tolerance = 1e-10)
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


struct Inputs
{
    PriceReturnFeatures stock_price;
    TrendMomentumFeatures stock_trend;
    VolatilityFeatures stock_volatility;

    PriceReturnFeatures benchmark_price;
    TrendMomentumFeatures benchmark_trend;
    VolatilityFeatures benchmark_volatility;
};


Inputs makeInputs()
{
    Inputs input;


    input.stock_price.symbol = "RELIANCE";
    input.stock_trend.symbol = "RELIANCE";
    input.stock_volatility.symbol = "RELIANCE";

    input.benchmark_price.symbol = "NIFTY%2050";
    input.benchmark_trend.symbol = "NIFTY%2050";
    input.benchmark_volatility.symbol = "NIFTY%2050";


    input.stock_price.decision_time = DECISION_TIME;
    input.stock_trend.decision_time = DECISION_TIME;
    input.stock_volatility.decision_time = DECISION_TIME;

    input.benchmark_price.decision_time = DECISION_TIME;
    input.benchmark_trend.decision_time = DECISION_TIME;
    input.benchmark_volatility.decision_time = DECISION_TIME;


    return input;
}


void populateCompleteInputs(
    Inputs& input)
{
    // ========================================================================
    // Relative-return inputs
    // ========================================================================

    input.stock_price.has_one_minute = true;
    input.stock_price.has_five_minute = true;
    input.stock_price.has_fifteen_minute = true;

    input.benchmark_price.has_one_minute = true;
    input.benchmark_price.has_five_minute = true;
    input.benchmark_price.has_fifteen_minute = true;


    input.stock_price.one_minute_return = 0.003;
    input.stock_price.five_minute_return = 0.010;
    input.stock_price.fifteen_minute_return = 0.020;

    input.benchmark_price.one_minute_return = 0.001;
    input.benchmark_price.five_minute_return = 0.004;
    input.benchmark_price.fifteen_minute_return = 0.008;


    // ========================================================================
    // RSI14
    // ========================================================================

    input.stock_trend.one_minute.rsi14 = 60.0;
    input.stock_trend.five_minute.rsi14 = 65.0;
    input.stock_trend.fifteen_minute.rsi14 = 70.0;

    input.benchmark_trend.one_minute.rsi14 = 55.0;
    input.benchmark_trend.five_minute.rsi14 = 57.0;
    input.benchmark_trend.fifteen_minute.rsi14 = 60.0;


    input.stock_trend.one_minute.has_rsi14 = true;
    input.stock_trend.five_minute.has_rsi14 = true;
    input.stock_trend.fifteen_minute.has_rsi14 = true;

    input.benchmark_trend.one_minute.has_rsi14 = true;
    input.benchmark_trend.five_minute.has_rsi14 = true;
    input.benchmark_trend.fifteen_minute.has_rsi14 = true;


    // ========================================================================
    // EMA20 / EMA50
    // ========================================================================

    input.stock_trend.one_minute.ema20 = 102.0;
    input.stock_trend.one_minute.ema50 = 100.0;

    input.stock_trend.five_minute.ema20 = 104.0;
    input.stock_trend.five_minute.ema50 = 100.0;

    input.stock_trend.fifteen_minute.ema20 = 106.0;
    input.stock_trend.fifteen_minute.ema50 = 100.0;


    input.benchmark_trend.one_minute.ema20 = 100.5;
    input.benchmark_trend.one_minute.ema50 = 100.0;

    input.benchmark_trend.five_minute.ema20 = 101.0;
    input.benchmark_trend.five_minute.ema50 = 100.0;

    input.benchmark_trend.fifteen_minute.ema20 = 102.0;
    input.benchmark_trend.fifteen_minute.ema50 = 100.0;


    input.stock_trend.one_minute.has_ema20 = true;
    input.stock_trend.one_minute.has_ema50 = true;

    input.stock_trend.five_minute.has_ema20 = true;
    input.stock_trend.five_minute.has_ema50 = true;

    input.stock_trend.fifteen_minute.has_ema20 = true;
    input.stock_trend.fifteen_minute.has_ema50 = true;


    input.benchmark_trend.one_minute.has_ema20 = true;
    input.benchmark_trend.one_minute.has_ema50 = true;

    input.benchmark_trend.five_minute.has_ema20 = true;
    input.benchmark_trend.five_minute.has_ema50 = true;

    input.benchmark_trend.fifteen_minute.has_ema20 = true;
    input.benchmark_trend.fifteen_minute.has_ema50 = true;


    // ========================================================================
    // EMA20 slopes
    // ========================================================================

    input.stock_trend.one_minute.ema20_slope = 0.004;
    input.stock_trend.five_minute.ema20_slope = 0.008;
    input.stock_trend.fifteen_minute.ema20_slope = 0.012;

    input.benchmark_trend.one_minute.ema20_slope = 0.001;
    input.benchmark_trend.five_minute.ema20_slope = 0.002;
    input.benchmark_trend.fifteen_minute.ema20_slope = 0.003;


    input.stock_trend.one_minute.has_ema20_slope = true;
    input.stock_trend.five_minute.has_ema20_slope = true;
    input.stock_trend.fifteen_minute.has_ema20_slope = true;

    input.benchmark_trend.one_minute.has_ema20_slope = true;
    input.benchmark_trend.five_minute.has_ema20_slope = true;
    input.benchmark_trend.fifteen_minute.has_ema20_slope = true;


    // ========================================================================
    // EMA50 slopes
    // ========================================================================

    input.stock_trend.one_minute.ema50_slope = 0.002;
    input.stock_trend.five_minute.ema50_slope = 0.004;
    input.stock_trend.fifteen_minute.ema50_slope = 0.006;

    input.benchmark_trend.one_minute.ema50_slope = 0.0005;
    input.benchmark_trend.five_minute.ema50_slope = 0.001;
    input.benchmark_trend.fifteen_minute.ema50_slope = 0.0015;


    input.stock_trend.one_minute.has_ema50_slope = true;
    input.stock_trend.five_minute.has_ema50_slope = true;
    input.stock_trend.fifteen_minute.has_ema50_slope = true;

    input.benchmark_trend.one_minute.has_ema50_slope = true;
    input.benchmark_trend.five_minute.has_ema50_slope = true;
    input.benchmark_trend.fifteen_minute.has_ema50_slope = true;


    // ========================================================================
    // ATR14 %
    // ========================================================================

    input.stock_volatility.one_minute.atr14_percent = 0.010;
    input.stock_volatility.five_minute.atr14_percent = 0.020;
    input.stock_volatility.fifteen_minute.atr14_percent = 0.030;

    input.benchmark_volatility.one_minute.atr14_percent = 0.005;
    input.benchmark_volatility.five_minute.atr14_percent = 0.010;
    input.benchmark_volatility.fifteen_minute.atr14_percent = 0.015;


    input.stock_volatility.one_minute.has_atr14_percent = true;
    input.stock_volatility.five_minute.has_atr14_percent = true;
    input.stock_volatility.fifteen_minute.has_atr14_percent = true;

    input.benchmark_volatility.one_minute.has_atr14_percent = true;
    input.benchmark_volatility.five_minute.has_atr14_percent = true;
    input.benchmark_volatility.fifteen_minute.has_atr14_percent = true;


    // ========================================================================
    // Realized volatility 20
    // ========================================================================

    input.stock_volatility.one_minute.realized_volatility_20 = 0.020;
    input.stock_volatility.five_minute.realized_volatility_20 = 0.030;
    input.stock_volatility.fifteen_minute.realized_volatility_20 = 0.040;

    input.benchmark_volatility.one_minute.realized_volatility_20 = 0.010;
    input.benchmark_volatility.five_minute.realized_volatility_20 = 0.015;
    input.benchmark_volatility.fifteen_minute.realized_volatility_20 = 0.020;


    input.stock_volatility.one_minute.has_realized_volatility_20 = true;
    input.stock_volatility.five_minute.has_realized_volatility_20 = true;
    input.stock_volatility.fifteen_minute.has_realized_volatility_20 = true;

    input.benchmark_volatility.one_minute.has_realized_volatility_20 = true;
    input.benchmark_volatility.five_minute.has_realized_volatility_20 = true;
    input.benchmark_volatility.fifteen_minute.has_realized_volatility_20 = true;
}


StockBenchmarkFeatures run(
    StockBenchmarkFeatureEngine& engine,
    const Inputs& input)
{
    return engine.update(
        input.stock_price,
        input.stock_trend,
        input.stock_volatility,

        input.benchmark_price,
        input.benchmark_trend,
        input.benchmark_volatility
    );
}


// ============================================================================
// Relative returns
// ============================================================================

void testRelativeReturns()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(output.relative_return.has_one_minute);
    assert(output.relative_return.has_five_minute);
    assert(output.relative_return.has_fifteen_minute);


    assert(
        approximatelyEqual(
            output.relative_return.one_minute,
            0.002
        )
    );


    assert(
        approximatelyEqual(
            output.relative_return.five_minute,
            0.006
        )
    );


    assert(
        approximatelyEqual(
            output.relative_return.fifteen_minute,
            0.012
        )
    );
}


// ============================================================================
// Relative RSI
// ============================================================================

void testRelativeRSI()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(
        approximatelyEqual(
            output.relative_rsi14.one_minute,
            5.0
        )
    );


    assert(
        approximatelyEqual(
            output.relative_rsi14.five_minute,
            8.0
        )
    );


    assert(
        approximatelyEqual(
            output.relative_rsi14.fifteen_minute,
            10.0
        )
    );
}


// ============================================================================
// Relative EMA relationship
// ============================================================================

void testRelativeEMA()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(
        approximatelyEqual(
            output.relative_ema_relationship.one_minute,
            0.015
        )
    );


    assert(
        approximatelyEqual(
            output.relative_ema_relationship.five_minute,
            0.030
        )
    );


    assert(
        approximatelyEqual(
            output.relative_ema_relationship.fifteen_minute,
            0.040
        )
    );
}


// ============================================================================
// Relative EMA20 slope
// ============================================================================

void testRelativeEMA20Slope()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(
        approximatelyEqual(
            output.relative_ema20_slope.one_minute,
            0.003
        )
    );


    assert(
        approximatelyEqual(
            output.relative_ema20_slope.five_minute,
            0.006
        )
    );


    assert(
        approximatelyEqual(
            output.relative_ema20_slope.fifteen_minute,
            0.009
        )
    );
}


// ============================================================================
// Relative EMA50 slope
// ============================================================================

void testRelativeEMA50Slope()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(
        approximatelyEqual(
            output.relative_ema50_slope.one_minute,
            0.0015
        )
    );


    assert(
        approximatelyEqual(
            output.relative_ema50_slope.five_minute,
            0.003
        )
    );


    assert(
        approximatelyEqual(
            output.relative_ema50_slope.fifteen_minute,
            0.0045
        )
    );
}


// ============================================================================
// Relative ATR14%
// ============================================================================

void testRelativeATR()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(
        approximatelyEqual(
            output.relative_atr14_percent.one_minute,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            output.relative_atr14_percent.five_minute,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            output.relative_atr14_percent.fifteen_minute,
            2.0
        )
    );
}


// ============================================================================
// Relative realized volatility 20
// ============================================================================

void testRelativeRealizedVolatility()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(
        approximatelyEqual(
            output.relative_realized_volatility_20.one_minute,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            output.relative_realized_volatility_20.five_minute,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            output.relative_realized_volatility_20.fifteen_minute,
            2.0
        )
    );
}


// ============================================================================
// Core readiness
// ============================================================================

void testCoreReadiness()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    const auto output =
        run(engine, input);


    assert(output.core_features_ready);
}


// ============================================================================
// Partial availability
// ============================================================================

void testPartialAvailability()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    input.benchmark_trend
        .fifteen_minute
        .has_rsi14 = false;


    const auto output =
        run(engine, input);


    assert(
        !output.relative_rsi14.has_fifteen_minute
    );


    assert(
        !output.core_features_ready
    );
}


// ============================================================================
// Ratio zero-denominator protection
// ============================================================================

void testZeroDenominatorProtection()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    input.benchmark_volatility
        .one_minute
        .atr14_percent = 0.0;


    const auto output =
        run(engine, input);


    assert(
        !output.relative_atr14_percent.has_one_minute
    );


    assert(
        output.relative_atr14_percent.has_five_minute
    );


    assert(
        output.relative_atr14_percent.has_fifteen_minute
    );
}


// ============================================================================
// Stock symbol protection
// ============================================================================

void testStockSymbolProtection()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    input.stock_volatility.symbol =
        "OTHER";


    bool thrown = false;


    try
    {
        static_cast<void>(
            run(engine, input)
        );
    }
    catch (const std::invalid_argument&)
    {
        thrown = true;
    }


    assert(thrown);
}


// ============================================================================
// Benchmark symbol protection
// ============================================================================

void testBenchmarkSymbolProtection()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    input.benchmark_price.symbol =
        "OTHER";


    bool thrown = false;


    try
    {
        static_cast<void>(
            run(engine, input)
        );
    }
    catch (const std::invalid_argument&)
    {
        thrown = true;
    }


    assert(thrown);
}


// ============================================================================
// Decision-time protection
// ============================================================================

void testDecisionTimeProtection()
{
    StockBenchmarkFeatureEngine engine(
        "RELIANCE",
        "NIFTY%2050"
    );


    Inputs input = makeInputs();

    populateCompleteInputs(input);


    input.benchmark_volatility.decision_time =
        DECISION_TIME - 1;


    bool thrown = false;


    try
    {
        static_cast<void>(
            run(engine, input)
        );
    }
    catch (const std::invalid_argument&)
    {
        thrown = true;
    }


    assert(thrown);
}


// ============================================================================
// Same-symbol protection
// ============================================================================

void testSameSymbolProtection()
{
    bool thrown = false;


    try
    {
        StockBenchmarkFeatureEngine engine(
            "RELIANCE",
            "RELIANCE"
        );


        static_cast<void>(engine);
    }
    catch (const std::invalid_argument&)
    {
        thrown = true;
    }


    assert(thrown);
}

} // namespace


int main()
{
    testRelativeReturns();

    testRelativeRSI();

    testRelativeEMA();

    testRelativeEMA20Slope();

    testRelativeEMA50Slope();

    testRelativeATR();

    testRelativeRealizedVolatility();

    testCoreReadiness();

    testPartialAvailability();

    testZeroDenominatorProtection();

    testStockSymbolProtection();

    testBenchmarkSymbolProtection();

    testDecisionTimeProtection();

    testSameSymbolProtection();


    std::cout
        << "\n"
        << "================================================\n"
        << "StockBenchmarkFeatureEngine Tests PASSED\n"
        << "================================================\n"
        << "Relative return 1m/5m/15m      : PASSED\n"
        << "Relative RSI14 1m/5m/15m       : PASSED\n"
        << "Relative EMA relationship      : PASSED\n"
        << "Relative EMA20 slope           : PASSED\n"
        << "Relative EMA50 slope           : PASSED\n"
        << "Relative ATR14%                : PASSED\n"
        << "Relative Realized Volatility20 : PASSED\n"
        << "Core readiness                 : PASSED\n"
        << "Partial availability           : PASSED\n"
        << "Zero denominator protection    : PASSED\n"
        << "Stock symbol protection        : PASSED\n"
        << "Benchmark symbol protection    : PASSED\n"
        << "Decision-time protection       : PASSED\n"
        << "Same-symbol protection         : PASSED\n"
        << "================================================\n";


    return 0;
}