#include "devai/features/VolatilityFeatureEngine.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>


using namespace devai::market;
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


Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double open,
    double high,
    double low,
    double close,
    double volume = 1000.0)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        open;

    candle.high =
        high;

    candle.low =
        low;

    candle.close =
        close;

    candle.volume =
        volume;

    return candle;
}


MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    const Candle& one_minute,
    bool new_session = false)
{
    MarketSnapshot snapshot;

    snapshot.symbol =
        symbol;

    snapshot.decision_time =
        decision_time;

    snapshot.one_minute =
        one_minute;

    snapshot.new_session =
        new_session;

    return snapshot;
}


// ============================================================================
// True Range
// ============================================================================

void testTrueRange()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    auto first =
        makeSnapshot(
            "TEST",
            1060,
            makeCandle(
                "TEST",
                1000,
                100.0,
                105.0,
                98.0,
                102.0
            )
        );


    const auto first_features =
        engine.update(
            first
        );


    assert(
        first_features.one_minute.has_true_range
    );


    assert(
        approximatelyEqual(
            first_features.one_minute.true_range,
            7.0
        )
    );


    auto second =
        makeSnapshot(
            "TEST",
            1120,
            makeCandle(
                "TEST",
                1060,
                102.0,
                110.0,
                101.0,
                108.0
            )
        );


    const auto second_features =
        engine.update(
            second
        );


    // max(
    //     110 - 101 = 9,
    //     |110 - 102| = 8,
    //     |101 - 102| = 1
    // )
    //
    // = 9

    assert(
        approximatelyEqual(
            second_features.one_minute.true_range,
            9.0
        )
    );
}


// ============================================================================
// Gap-aware True Range
// ============================================================================

void testGapTrueRange()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    static_cast<void>(
        engine.update(
            makeSnapshot(
                "TEST",
                1060,
                makeCandle(
                    "TEST",
                    1000,
                    100.0,
                    102.0,
                    99.0,
                    100.0
                )
            )
        )
    );


    const auto features =
        engine.update(
            makeSnapshot(
                "TEST",
                1120,
                makeCandle(
                    "TEST",
                    1060,
                    110.0,
                    112.0,
                    109.0,
                    111.0
                )
            )
        );


    // Previous close = 100
    //
    // high-low = 3
    // |112-100| = 12
    // |109-100| = 9
    //
    // TR = 12

    assert(
        approximatelyEqual(
            features.one_minute.true_range,
            12.0
        )
    );
}


// ============================================================================
// ATR14 seed
// ============================================================================

void testATR14()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    double previous_close =
        100.0;


    for (
        std::size_t index = 0;
        index < 14;
        ++index
    )
    {
        const std::int64_t timestamp =
            1000 +
            static_cast<std::int64_t>(
                index
            ) *
            60;


        Candle candle =
            makeCandle(
                "TEST",
                timestamp,
                previous_close,
                previous_close + 1.0,
                previous_close - 1.0,
                previous_close
            );


        const auto features =
            engine.update(
                makeSnapshot(
                    "TEST",
                    timestamp + 60,
                    candle
                )
            );


        if (index < 13)
        {
            assert(
                !features.one_minute.has_atr14
            );
        }
        else
        {
            assert(
                features.one_minute.has_atr14
            );


            assert(
                approximatelyEqual(
                    features.one_minute.atr14,
                    2.0
                )
            );
        }
    }
}


// ============================================================================
// Wilder ATR update
// ============================================================================

void testWilderATR()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    for (
        std::size_t index = 0;
        index < 14;
        ++index
    )
    {
        const std::int64_t timestamp =
            1000 +
            static_cast<std::int64_t>(
                index
            ) *
            60;


        static_cast<void>(
            engine.update(
                makeSnapshot(
                    "TEST",
                    timestamp + 60,
                    makeCandle(
                        "TEST",
                        timestamp,
                        100.0,
                        101.0,
                        99.0,
                        100.0
                    )
                )
            )
        );
    }


    const std::int64_t timestamp =
        1000 +
        14 * 60;


    const auto features =
        engine.update(
            makeSnapshot(
                "TEST",
                timestamp + 60,
                makeCandle(
                    "TEST",
                    timestamp,
                    100.0,
                    104.0,
                    100.0,
                    102.0
                )
            )
        );


    const double expected =
        (
            2.0 *
            13.0 +
            4.0
        ) /
        14.0;


    assert(
        approximatelyEqual(
            features.one_minute.atr14,
            expected
        )
    );
}


// ============================================================================
// 20 / 50 return windows
// ============================================================================

void testRollingVolatility()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    double close =
        100.0;


    for (
        std::size_t index = 0;
        index < 51;
        ++index
    )
    {
        const std::int64_t timestamp =
            1000 +
            static_cast<std::int64_t>(
                index
            ) *
            60;


        close *=
            (
                index == 0
                    ? 1.0
                    : (
                        index % 2 == 0
                            ? 1.01
                            : 0.99
                    )
            );


        const Candle candle =
            makeCandle(
                "TEST",
                timestamp,
                close,
                close + 0.5,
                close - 0.5,
                close
            );


        const auto features =
            engine.update(
                makeSnapshot(
                    "TEST",
                    timestamp + 60,
                    candle
                )
            );


        if (index == 20)
        {
            assert(
                features.one_minute.has_return_stddev_20
            );


            assert(
                features.one_minute.has_realized_volatility_20
            );
        }


        if (index == 50)
        {
            assert(
                features.one_minute.has_return_stddev_50
            );


            assert(
                features.one_minute.has_realized_volatility_50
            );
        }
    }
}


// ============================================================================
// Duplicate snapshot
// ============================================================================

void testDuplicateProtection()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    const Candle candle =
        makeCandle(
            "TEST",
            1000,
            100.0,
            105.0,
            98.0,
            102.0
        );


    const MarketSnapshot snapshot =
        makeSnapshot(
            "TEST",
            1060,
            candle
        );


    const auto first =
        engine.update(
            snapshot
        );


    const auto second =
        engine.update(
            snapshot
        );


    assert(
        first.one_minute.has_true_range
    );


    assert(
        second.one_minute.has_true_range
    );


    assert(
        approximatelyEqual(
            first.one_minute.true_range,
            second.one_minute.true_range
        )
    );
}


// ============================================================================
// Cross-session continuity
// ============================================================================

void testCrossSessionContinuity()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    double close =
        100.0;


    VolatilityFeatures before_session_change;


    for (
        std::size_t index = 0;
        index < 50;
        ++index
    )
    {
        const std::int64_t timestamp =
            1000 +
            static_cast<std::int64_t>(
                index
            ) *
            60;


        close *=
            (
                index % 2 == 0
                    ? 1.001
                    : 0.999
            );


        before_session_change =
            engine.update(
                makeSnapshot(
                    "TEST",
                    timestamp + 60,
                    makeCandle(
                        "TEST",
                        timestamp,
                        close,
                        close + 1.0,
                        close - 1.0,
                        close
                    )
                )
            );
    }


    assert(
        before_session_change.one_minute.has_atr14
    );


    // New session flag MUST NOT clear continuous volatility history.

    const std::int64_t next_timestamp =
        1000 +
        50 * 60;


    close *=
        1.002;


    const auto after_session_change =
        engine.update(
            makeSnapshot(
                "TEST",
                next_timestamp + 60,
                makeCandle(
                    "TEST",
                    next_timestamp,
                    close,
                    close + 1.0,
                    close - 1.0,
                    close
                ),
                true
            )
        );


    assert(
        after_session_change.one_minute.has_atr14
    );


    assert(
        after_session_change.one_minute.has_return_stddev_20
    );


    assert(
        after_session_change.one_minute.has_return_stddev_50
    );


    assert(
        after_session_change.one_minute.has_realized_volatility_20
    );


    assert(
        after_session_change.one_minute.has_realized_volatility_50
    );
}


// ============================================================================
// 1m / 5m / 15m
// ============================================================================

void testAllTimeframes()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    MarketSnapshot snapshot;

    snapshot.symbol =
        "TEST";

    snapshot.decision_time =
        2000;


    snapshot.one_minute =
        makeCandle(
            "TEST",
            1900,
            100.0,
            102.0,
            99.0,
            101.0
        );


    snapshot.five_minute =
        makeCandle(
            "TEST",
            1600,
            100.0,
            105.0,
            98.0,
            103.0
        );


    snapshot.fifteen_minute =
        makeCandle(
            "TEST",
            1000,
            100.0,
            108.0,
            95.0,
            105.0
        );


    const auto features =
        engine.update(
            snapshot
        );


    assert(
        features.one_minute.has_true_range
    );


    assert(
        features.five_minute.has_true_range
    );


    assert(
        features.fifteen_minute.has_true_range
    );
}


// ============================================================================
// Symbol protection
// ============================================================================

void testSymbolProtection()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    bool thrown =
        false;


    try
    {
        MarketSnapshot snapshot;

        snapshot.symbol =
            "OTHER";

        snapshot.decision_time =
            1000;


        static_cast<void>(
            engine.update(
                snapshot
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
// Backward decision-time protection
// ============================================================================

void testBackwardTimeProtection()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    MarketSnapshot first;

    first.symbol =
        "TEST";

    first.decision_time =
        2000;


    static_cast<void>(
        engine.update(
            first
        )
    );


    MarketSnapshot second;

    second.symbol =
        "TEST";

    second.decision_time =
        1000;


    bool thrown =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                second
            )
        );
    }
    catch (
        const std::runtime_error&
    )
    {
        thrown =
            true;
    }


    assert(thrown);
}


// ============================================================================
// Explicit reset
// ============================================================================

void testExplicitReset()
{
    VolatilityFeatureEngine engine(
        "TEST"
    );


    for (
        std::size_t index = 0;
        index < 20;
        ++index
    )
    {
        const std::int64_t timestamp =
            1000 +
            static_cast<std::int64_t>(
                index
            ) *
            60;


        static_cast<void>(
            engine.update(
                makeSnapshot(
                    "TEST",
                    timestamp + 60,
                    makeCandle(
                        "TEST",
                        timestamp,
                        100.0,
                        101.0,
                        99.0,
                        100.0
                    )
                )
            )
        );
    }


    engine.reset();


    const auto features =
        engine.update(
            makeSnapshot(
                "TEST",
                10000,
                makeCandle(
                    "TEST",
                    9940,
                    100.0,
                    101.0,
                    99.0,
                    100.0
                )
            )
        );


    assert(
        features.one_minute.has_true_range
    );


    assert(
        !features.one_minute.has_atr14
    );


    assert(
        !features.one_minute.has_return_stddev_20
    );


    assert(
        !features.one_minute.has_realized_volatility_20
    );
}

} // namespace


int main()
{
    testTrueRange();

    testGapTrueRange();

    testATR14();

    testWilderATR();

    testRollingVolatility();

    testDuplicateProtection();

    testCrossSessionContinuity();

    testAllTimeframes();

    testSymbolProtection();

    testBackwardTimeProtection();

    testExplicitReset();


    std::cout
        << "\n"
        << "================================================\n"
        << "VolatilityFeatureEngine Tests PASSED\n"
        << "================================================\n"
        << "True Range                    : PASSED\n"
        << "Gap-aware True Range          : PASSED\n"
        << "ATR14 seed                    : PASSED\n"
        << "Wilder ATR update             : PASSED\n"
        << "Return StdDev 20/50           : PASSED\n"
        << "Realized Volatility 20/50     : PASSED\n"
        << "Duplicate candle protection   : PASSED\n"
        << "Cross-session continuity      : PASSED\n"
        << "1m / 5m / 15m volatility     : PASSED\n"
        << "Symbol protection             : PASSED\n"
        << "Backward-time protection      : PASSED\n"
        << "Explicit reset                : PASSED\n"
        << "================================================\n";


    return 0;
}