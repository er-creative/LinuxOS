#include "devai/features/MultiTimeframeFeatureEngine.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>


using namespace devai::features;


namespace
{

constexpr double TOLERANCE =
    1e-10;


bool approximatelyEqual(
    double left,
    double right)
{
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
            left -
            right
        ) <=
        TOLERANCE *
        scale;
}


// ============================================================================
// Complete artificial Phase 3 input bundle
// ============================================================================

struct TestInputs
{
    PriceReturnFeatures price;

    TrendMomentumFeatures trend;

    VolumeFeatures volume;

    VolatilityFeatures volatility;
};


TestInputs makeInputs(
    const std::string& symbol = "TEST",
    std::int64_t decision_time = 1000)
{
    TestInputs input;


    input.price.symbol =
        symbol;

    input.price.decision_time =
        decision_time;


    input.trend.symbol =
        symbol;

    input.trend.decision_time =
        decision_time;


    input.volume.symbol =
        symbol;

    input.volume.decision_time =
        decision_time;


    input.volatility.symbol =
        symbol;

    input.volatility.decision_time =
        decision_time;


    return input;
}


// ============================================================================
// Populate fully available inputs
// ============================================================================

void populateCompleteInputs(
    TestInputs& input)
{
    // ------------------------------------------------------------------------
    // Returns
    // ------------------------------------------------------------------------

    input.price.has_one_minute =
        true;

    input.price.has_five_minute =
        true;

    input.price.has_fifteen_minute =
        true;


    input.price.one_minute_return =
        0.001;

    input.price.five_minute_return =
        0.002;

    input.price.fifteen_minute_return =
        0.003;


    // ------------------------------------------------------------------------
    // Trend / Momentum
    // ------------------------------------------------------------------------

    input.trend.one_minute.ema20 =
        110.0;

    input.trend.one_minute.ema50 =
        100.0;

    input.trend.one_minute.has_ema20 =
        true;

    input.trend.one_minute.has_ema50 =
        true;


    input.trend.five_minute.ema20 =
        120.0;

    input.trend.five_minute.ema50 =
        110.0;

    input.trend.five_minute.has_ema20 =
        true;

    input.trend.five_minute.has_ema50 =
        true;


    input.trend.fifteen_minute.ema20 =
        130.0;

    input.trend.fifteen_minute.ema50 =
        120.0;

    input.trend.fifteen_minute.has_ema20 =
        true;

    input.trend.fifteen_minute.has_ema50 =
        true;


    input.trend.one_minute.ema20_slope =
        0.001;

    input.trend.five_minute.ema20_slope =
        0.002;

    input.trend.fifteen_minute.ema20_slope =
        0.003;


    input.trend.one_minute.has_ema20_slope =
        true;

    input.trend.five_minute.has_ema20_slope =
        true;

    input.trend.fifteen_minute.has_ema20_slope =
        true;


    input.trend.one_minute.rsi14 =
        55.0;

    input.trend.five_minute.rsi14 =
        60.0;

    input.trend.fifteen_minute.rsi14 =
        65.0;


    input.trend.one_minute.has_rsi14 =
        true;

    input.trend.five_minute.has_rsi14 =
        true;

    input.trend.fifteen_minute.has_rsi14 =
        true;


    // ------------------------------------------------------------------------
    // Volume
    // ------------------------------------------------------------------------

    input.volume.one_minute.relative_volume_20 =
        1.0;

    input.volume.five_minute.relative_volume_20 =
        1.5;

    input.volume.fifteen_minute.relative_volume_20 =
        2.0;


    input.volume.one_minute.has_relative_volume_20 =
        true;

    input.volume.five_minute.has_relative_volume_20 =
        true;

    input.volume.fifteen_minute.has_relative_volume_20 =
        true;


    // ------------------------------------------------------------------------
    // Volatility
    // ------------------------------------------------------------------------

    input.volatility.one_minute.atr14_percent =
        0.01;

    input.volatility.five_minute.atr14_percent =
        0.02;

    input.volatility.fifteen_minute.atr14_percent =
        0.04;


    input.volatility.one_minute.has_atr14_percent =
        true;

    input.volatility.five_minute.has_atr14_percent =
        true;

    input.volatility.fifteen_minute.has_atr14_percent =
        true;


    input.volatility.one_minute.realized_volatility_20 =
        0.005;

    input.volatility.five_minute.realized_volatility_20 =
        0.010;

    input.volatility.fifteen_minute.realized_volatility_20 =
        0.020;


    input.volatility.one_minute.has_realized_volatility_20 =
        true;

    input.volatility.five_minute.has_realized_volatility_20 =
        true;

    input.volatility.fifteen_minute.has_realized_volatility_20 =
        true;
}


// ============================================================================
// Positive alignment
// ============================================================================

void testPositiveAlignment()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        features.return_alignment.all_positive
    );


    assert(
        features.return_alignment.positive_count == 3
    );


    assert(
        features.ema_alignment.all_positive
    );


    assert(
        features.ema20_slope_alignment.all_positive
    );


    assert(
        features.rsi_alignment.all_positive
    );


    assert(
        features.core_features_ready
    );
}


// ============================================================================
// Negative alignment
// ============================================================================

void testNegativeAlignment()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    input.price.one_minute_return =
        -0.001;

    input.price.five_minute_return =
        -0.002;

    input.price.fifteen_minute_return =
        -0.003;


    input.trend.one_minute.ema20 =
        90.0;

    input.trend.one_minute.ema50 =
        100.0;


    input.trend.five_minute.ema20 =
        90.0;

    input.trend.five_minute.ema50 =
        100.0;


    input.trend.fifteen_minute.ema20 =
        90.0;

    input.trend.fifteen_minute.ema50 =
        100.0;


    input.trend.one_minute.ema20_slope =
        -0.001;

    input.trend.five_minute.ema20_slope =
        -0.002;

    input.trend.fifteen_minute.ema20_slope =
        -0.003;


    input.trend.one_minute.rsi14 =
        45.0;

    input.trend.five_minute.rsi14 =
        40.0;

    input.trend.fifteen_minute.rsi14 =
        35.0;


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        features.return_alignment.all_negative
    );


    assert(
        features.ema_alignment.all_negative
    );


    assert(
        features.ema20_slope_alignment.all_negative
    );


    assert(
        features.rsi_alignment.all_negative
    );
}


// ============================================================================
// Mixed alignment
// ============================================================================

void testMixedAlignment()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    input.price.one_minute_return =
        0.001;

    input.price.five_minute_return =
        -0.002;

    input.price.fifteen_minute_return =
        0.003;


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        features.return_alignment.mixed
    );


    assert(
        features.return_alignment.positive_count == 2
    );


    assert(
        features.return_alignment.negative_count == 1
    );


    assert(
        !features.return_alignment.all_positive
    );


    assert(
        !features.return_alignment.all_negative
    );
}


// ============================================================================
// RVOL ratios
// ============================================================================

void testRVOLRatios()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        features.rvol20_ratios.has_five_to_one
    );


    assert(
        approximatelyEqual(
            features.rvol20_ratios.five_to_one,
            1.5
        )
    );


    assert(
        features.rvol20_ratios.has_fifteen_to_five
    );


    assert(
        approximatelyEqual(
            features.rvol20_ratios.fifteen_to_five,
            2.0 / 1.5
        )
    );


    assert(
        features.rvol20_ratios.has_fifteen_to_one
    );


    assert(
        approximatelyEqual(
            features.rvol20_ratios.fifteen_to_one,
            2.0
        )
    );
}


// ============================================================================
// ATR ratios
// ============================================================================

void testATRPercentRatios()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        approximatelyEqual(
            features.atr14_percent_ratios.five_to_one,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            features.atr14_percent_ratios.fifteen_to_five,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            features.atr14_percent_ratios.fifteen_to_one,
            4.0
        )
    );
}


// ============================================================================
// Realized-volatility ratios
// ============================================================================

void testRealizedVolatilityRatios()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        approximatelyEqual(
            features.realized_volatility_20_ratios.five_to_one,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            features.realized_volatility_20_ratios.fifteen_to_five,
            2.0
        )
    );


    assert(
        approximatelyEqual(
            features.realized_volatility_20_ratios.fifteen_to_one,
            4.0
        )
    );
}


// ============================================================================
// RSI relationships
// ============================================================================

void testRSIRelationship()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        approximatelyEqual(
            features.rsi_relationship.five_minus_one,
            5.0
        )
    );


    assert(
        approximatelyEqual(
            features.rsi_relationship.fifteen_minus_five,
            5.0
        )
    );


    assert(
        approximatelyEqual(
            features.rsi_relationship.fifteen_minus_one,
            10.0
        )
    );
}


// ============================================================================
// Warm-up / partial availability
// ============================================================================

void testPartialAvailability()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    input.trend.fifteen_minute.has_ema50 =
        false;


    input.trend.fifteen_minute.has_rsi14 =
        false;


    input.volatility.fifteen_minute.has_atr14_percent =
        false;


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        !features.ema_alignment.fully_available
    );


    assert(
        !features.rsi_alignment.fully_available
    );


    assert(
        !features.core_features_ready
    );


    assert(
        !features.atr14_percent_ratios.has_fifteen_to_five
    );


    assert(
        !features.atr14_percent_ratios.has_fifteen_to_one
    );
}


// ============================================================================
// Zero denominator
// ============================================================================

void testZeroRatioProtection()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    input.volume.one_minute.relative_volume_20 =
        0.0;


    const auto features =
        engine.update(
            input.price,
            input.trend,
            input.volume,
            input.volatility
        );


    assert(
        !features.rvol20_ratios.has_five_to_one
    );


    assert(
        !features.rvol20_ratios.has_fifteen_to_one
    );


    assert(
        features.rvol20_ratios.has_fifteen_to_five
    );
}


// ============================================================================
// Symbol protection
// ============================================================================

void testSymbolProtection()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    input.volume.symbol =
        "OTHER";


    bool thrown =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                input.price,
                input.trend,
                input.volume,
                input.volatility
            )
        );
    }
    catch (
        const std::invalid_argument&
    )
    {
        thrown =
            true;
    }


    assert(thrown);
}


// ============================================================================
// Decision-time protection
// ============================================================================

void testDecisionTimeProtection()
{
    MultiTimeframeFeatureEngine engine(
        "TEST"
    );


    TestInputs input =
        makeInputs();


    populateCompleteInputs(
        input
    );


    input.volatility.decision_time =
        999;


    bool thrown =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                input.price,
                input.trend,
                input.volume,
                input.volatility
            )
        );
    }
    catch (
        const std::invalid_argument&
    )
    {
        thrown =
            true;
    }


    assert(thrown);
}

} // namespace


int main()
{
    testPositiveAlignment();

    testNegativeAlignment();

    testMixedAlignment();

    testRVOLRatios();

    testATRPercentRatios();

    testRealizedVolatilityRatios();

    testRSIRelationship();

    testPartialAvailability();

    testZeroRatioProtection();

    testSymbolProtection();

    testDecisionTimeProtection();


    std::cout
        << "\n"
        << "================================================\n"
        << "MultiTimeframeFeatureEngine Tests PASSED\n"
        << "================================================\n"
        << "Positive return alignment      : PASSED\n"
        << "Negative return alignment      : PASSED\n"
        << "Mixed return alignment         : PASSED\n"
        << "EMA alignment                  : PASSED\n"
        << "EMA20 slope alignment          : PASSED\n"
        << "RSI alignment                  : PASSED\n"
        << "RVOL20 ratios                  : PASSED\n"
        << "ATR14% ratios                  : PASSED\n"
        << "Realized Volatility 20 ratios  : PASSED\n"
        << "RSI timeframe differences      : PASSED\n"
        << "Partial availability           : PASSED\n"
        << "Zero-ratio protection          : PASSED\n"
        << "Symbol protection              : PASSED\n"
        << "Decision-time protection       : PASSED\n"
        << "================================================\n";


    return 0;
}