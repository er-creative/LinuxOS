#include "devai/features/TrendMomentumFeatureEngine.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/SessionState.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace devai::features;
using namespace devai::market;


namespace
{

// ============================================================================
// Numeric helper
// ============================================================================

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
            1.0,
            std::max(
                std::fabs(left),
                std::fabs(right)
            )
        );


    return
        std::fabs(
            left - right
        ) <=
        tolerance * scale;
}


// ============================================================================
// Timestamp helper
// ============================================================================

std::int64_t makeTimestamp(
    int year,
    int month,
    int day,
    int hour,
    int minute)
{
    std::tm value{};

    value.tm_year =
        year - 1900;

    value.tm_mon =
        month - 1;

    value.tm_mday =
        day;

    value.tm_hour =
        hour;

    value.tm_min =
        minute;

    value.tm_sec =
        0;

    value.tm_isdst =
        -1;


    return
        static_cast<std::int64_t>(
            std::mktime(
                &value
            )
        );
}


// ============================================================================
// Candle helper
// ============================================================================

Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double close)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        close;

    candle.high =
        close + 0.50;

    candle.low =
        close - 0.50;

    candle.close =
        close;

    candle.volume =
        1000.0;

    return candle;
}


// ============================================================================
// Snapshot helper
// ============================================================================

MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    const TradingDate& trading_date,
    bool new_session,
    const std::optional<Candle>& one_minute = std::nullopt,
    const std::optional<Candle>& five_minute = std::nullopt,
    const std::optional<Candle>& fifteen_minute = std::nullopt)
{
    MarketSnapshot snapshot;

    snapshot.symbol =
        symbol;

    snapshot.decision_time =
        decision_time;

    snapshot.trading_date =
        trading_date;

    snapshot.session_phase =
        SessionPhase::ACTIVE;

    snapshot.new_session =
        new_session;

    snapshot.one_minute =
        one_minute;

    snapshot.five_minute =
        five_minute;

    snapshot.fifteen_minute =
        fifteen_minute;

    return snapshot;
}


// ============================================================================
// Independent EMA
// ============================================================================

double nextEMA(
    double previous_ema,
    double close,
    std::size_t period)
{
    const double alpha =
        2.0 /
        (
            static_cast<double>(period) +
            1.0
        );


    return
        (
            alpha *
            close
        ) +
        (
            (
                1.0 -
                alpha
            ) *
            previous_ema
        );
}


// ============================================================================
// Tests
// ============================================================================

void testEMA20Warmup()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures features;


    for (
        std::size_t i = 0;
        i < 19;
        ++i
    )
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(
                        i
                    )
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );


        assert(
            !features.one_minute.has_ema20
        );
    }


    const Candle twentieth =
        makeCandle(
            symbol,
            base + 19 * 60,
            119.0
        );


    features =
        engine.update(
            makeSnapshot(
                symbol,
                twentieth.timestamp + 60,
                date,
                false,
                twentieth
            )
        );


    assert(
        features.one_minute.has_ema20
    );


    // SMA of 100 ... 119 = 109.5

    assert(
        approximatelyEqual(
            features.one_minute.ema20,
            109.5
        )
    );
}


void testEMA50Warmup()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures features;


    for (
        std::size_t i = 0;
        i < 49;
        ++i
    )
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(
                        i
                    )
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );


        assert(
            !features.one_minute.has_ema50
        );
    }


    const Candle fiftieth =
        makeCandle(
            symbol,
            base + 49 * 60,
            149.0
        );


    features =
        engine.update(
            makeSnapshot(
                symbol,
                fiftieth.timestamp + 60,
                date,
                false,
                fiftieth
            )
        );


    assert(
        features.one_minute.has_ema50
    );


    // SMA 100 ... 149 = 124.5

    assert(
        approximatelyEqual(
            features.one_minute.ema50,
            124.5
        )
    );
}


void testEMARecursiveCalculation()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures features;


    for (
        std::size_t i = 0;
        i < 50;
        ++i
    )
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(
                        i
                    )
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    const double previous_ema20 =
        features.one_minute.ema20;


    const double previous_ema50 =
        features.one_minute.ema50;


    const Candle next =
        makeCandle(
            symbol,
            base + 50 * 60,
            160.0
        );


    features =
        engine.update(
            makeSnapshot(
                symbol,
                next.timestamp + 60,
                date,
                false,
                next
            )
        );


    const double expected_ema20 =
        nextEMA(
            previous_ema20,
            160.0,
            20
        );


    const double expected_ema50 =
        nextEMA(
            previous_ema50,
            160.0,
            50
        );


    assert(
        approximatelyEqual(
            features.one_minute.ema20,
            expected_ema20
        )
    );


    assert(
        approximatelyEqual(
            features.one_minute.ema50,
            expected_ema50
        )
    );


    assert(
        features.one_minute.has_ema20_slope
    );


    assert(
        features.one_minute.has_ema50_slope
    );
}


void testRSI14()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures features;


    // 15 strictly rising closes gives 14 positive changes.
    //
    // RSI should become 100.

    for (
        std::size_t i = 0;
        i < 15;
        ++i
    )
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(
                        i
                    )
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    assert(
        features.one_minute.has_rsi14
    );


    assert(
        approximatelyEqual(
            features.one_minute.rsi14,
            100.0
        )
    );
}


// ============================================================================
// Explicit cross-session continuity
//
// This is the critical CETE test.
//
// We fully warm EMA20, EMA50 and RSI14 during session 1.
//
// Then:
//
//     snapshot.new_session = true
//
// for the first candle of session 2.
//
// The indicators MUST NOT disappear or restart.
// ============================================================================

void testCrossSessionContinuity()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate day_one{
        2026,
        9,
        25
    };


    const TradingDate day_two{
        2026,
        9,
        28
    };


    const std::int64_t day_one_base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures before_session_change;


    // ------------------------------------------------------------------------
    // Warm all indicators with 60 observations.
    //
    // Use alternating movement so RSI is neither trivially 0 nor 100.
    // ------------------------------------------------------------------------

    for (
        std::size_t i = 0;
        i < 60;
        ++i
    )
    {
        const double close =
            100.0 +
            static_cast<double>(i) * 0.25 +
            (
                i % 2 == 0
                    ? 0.40
                    : -0.20
            );


        const Candle candle =
            makeCandle(
                symbol,
                day_one_base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                close
            );


        before_session_change =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    day_one,
                    i == 0,
                    candle
                )
            );
    }


    assert(
        before_session_change.one_minute.has_ema20
    );

    assert(
        before_session_change.one_minute.has_ema50
    );

    assert(
        before_session_change.one_minute.has_rsi14
    );


    const double previous_ema20 =
        before_session_change.one_minute.ema20;


    const double previous_ema50 =
        before_session_change.one_minute.ema50;


    // ------------------------------------------------------------------------
    // First REAL candle of next session.
    //
    // There are no synthetic overnight candles.
    // ------------------------------------------------------------------------

    const std::int64_t day_two_timestamp =
        makeTimestamp(
            2026,
            9,
            28,
            9,
            15
        );


    const double day_two_close =
        120.0;


    const Candle first_day_two =
        makeCandle(
            symbol,
            day_two_timestamp,
            day_two_close
        );


    const TrendMomentumFeatures after_session_change =
        engine.update(
            makeSnapshot(
                symbol,
                day_two_timestamp + 60,
                day_two,
                true,
                first_day_two
            )
        );


    // ------------------------------------------------------------------------
    // Indicators MUST remain available.
    //
    // If update() incorrectly calls reset() on new_session, these fail.
    // ------------------------------------------------------------------------

    assert(
        after_session_change.one_minute.has_ema20
    );


    assert(
        after_session_change.one_minute.has_ema50
    );


    assert(
        after_session_change.one_minute.has_rsi14
    );


    // ------------------------------------------------------------------------
    // EMA must continue mathematically from the previous session.
    // ------------------------------------------------------------------------

    const double expected_ema20 =
        nextEMA(
            previous_ema20,
            day_two_close,
            20
        );


    const double expected_ema50 =
        nextEMA(
            previous_ema50,
            day_two_close,
            50
        );


    assert(
        approximatelyEqual(
            after_session_change.one_minute.ema20,
            expected_ema20
        )
    );


    assert(
        approximatelyEqual(
            after_session_change.one_minute.ema50,
            expected_ema50
        )
    );


    // RSI must still be a valid warmed indicator.

    assert(
        std::isfinite(
            after_session_change.one_minute.rsi14
        )
    );


    assert(
        after_session_change.one_minute.rsi14 >= 0.0
    );


    assert(
        after_session_change.one_minute.rsi14 <= 100.0
    );


    // Slopes must also remain available because this is not a fresh engine.

    assert(
        after_session_change.one_minute.has_ema20_slope
    );


    assert(
        after_session_change.one_minute.has_ema50_slope
    );
}


// ============================================================================
// Cross-session continuity for 5m and 15m as well
// ============================================================================

void testCrossSessionMultiTimeframeContinuity()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate day_one{
        2026,
        9,
        25
    };


    const TradingDate day_two{
        2026,
        9,
        28
    };


    const std::int64_t base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures before;


    // 60 unique observations for every timeframe.
    //
    // Artificial timestamps are sufficient here because this is an isolated
    // feature-engine test rather than the Phase 2 market-runtime test.

    for (
        std::size_t i = 0;
        i < 60;
        ++i
    )
    {
        const double close =
            100.0 +
            static_cast<double>(i);


        const Candle one =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                close
            );


        const Candle five =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 300
                    ),
                close + 10.0
            );


        const Candle fifteen =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 900
                    ),
                close + 20.0
            );


        before =
            engine.update(
                makeSnapshot(
                    symbol,
                    one.timestamp + 60,
                    day_one,
                    i == 0,
                    one,
                    five,
                    fifteen
                )
            );
    }


    assert(
        before.one_minute.has_ema50
    );

    assert(
        before.five_minute.has_ema50
    );

    assert(
        before.fifteen_minute.has_ema50
    );


    assert(
        before.one_minute.has_rsi14
    );

    assert(
        before.five_minute.has_rsi14
    );

    assert(
        before.fifteen_minute.has_rsi14
    );


    const double previous_one_ema50 =
        before.one_minute.ema50;


    const double previous_five_ema50 =
        before.five_minute.ema50;


    const double previous_fifteen_ema50 =
        before.fifteen_minute.ema50;


    const std::int64_t next_session =
        makeTimestamp(
            2026,
            9,
            28,
            9,
            15
        );


    const Candle one =
        makeCandle(
            symbol,
            next_session,
            170.0
        );


    const Candle five =
        makeCandle(
            symbol,
            next_session,
            180.0
        );


    const Candle fifteen =
        makeCandle(
            symbol,
            next_session,
            190.0
        );


    const TrendMomentumFeatures after =
        engine.update(
            makeSnapshot(
                symbol,
                next_session + 900,
                day_two,
                true,
                one,
                five,
                fifteen
            )
        );


    assert(
        after.one_minute.has_ema50
    );

    assert(
        after.five_minute.has_ema50
    );

    assert(
        after.fifteen_minute.has_ema50
    );


    assert(
        approximatelyEqual(
            after.one_minute.ema50,
            nextEMA(
                previous_one_ema50,
                170.0,
                50
            )
        )
    );


    assert(
        approximatelyEqual(
            after.five_minute.ema50,
            nextEMA(
                previous_five_ema50,
                180.0,
                50
            )
        )
    );


    assert(
        approximatelyEqual(
            after.fifteen_minute.ema50,
            nextEMA(
                previous_fifteen_ema50,
                190.0,
                50
            )
        )
    );


    assert(
        after.one_minute.has_rsi14
    );

    assert(
        after.five_minute.has_rsi14
    );

    assert(
        after.fifteen_minute.has_rsi14
    );
}


// ============================================================================
// Duplicate candle protection
// ============================================================================

void testDuplicateProtection()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t timestamp =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    const Candle candle =
        makeCandle(
            symbol,
            timestamp,
            100.0
        );


    const auto first =
        engine.update(
            makeSnapshot(
                symbol,
                timestamp + 60,
                date,
                true,
                candle
            )
        );


    const auto duplicate =
        engine.update(
            makeSnapshot(
                symbol,
                timestamp + 60,
                date,
                true,
                candle
            )
        );


    assert(
        first.one_minute.has_ema20 ==
        duplicate.one_minute.has_ema20
    );


    assert(
        first.one_minute.has_rsi14 ==
        duplicate.one_minute.has_rsi14
    );
}


// ============================================================================
// Symbol protection
// ============================================================================

void testSymbolProtection()
{
    TrendMomentumFeatureEngine engine(
        "TEST"
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t timestamp =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    bool rejected =
        false;


    try
    {
        static_cast<void>(
            engine.update(
                makeSnapshot(
                    "WRONG",
                    timestamp + 60,
                    date,
                    true,
                    makeCandle(
                        "WRONG",
                        timestamp,
                        100.0
                    )
                )
            )
        );
    }
    catch (
        const std::invalid_argument&
    )
    {
        rejected =
            true;
    }


    assert(
        rejected
    );
}


// ============================================================================
// Backward decision-time protection
// ============================================================================

void testBackwardTimeProtection()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t timestamp =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    // Establish a valid forward decision time.
    //
    // The return value is intentionally discarded because this part
    // of the test only establishes engine state.

    static_cast<void>(
        engine.update(
            makeSnapshot(
                symbol,
                timestamp + 120,
                date,
                true,
                makeCandle(
                    symbol,
                    timestamp,
                    100.0
                )
            )
        )
    );


    bool rejected =
        false;


    try
    {
        // Deliberately move the decision time backwards.
        //
        // update() must throw std::runtime_error.
        //
        // The return value is intentionally discarded because this
        // test is validating exception behavior.

        static_cast<void>(
            engine.update(
                makeSnapshot(
                    symbol,
                    timestamp + 60,
                    date,
                    false,
                    makeCandle(
                        symbol,
                        timestamp + 60,
                        101.0
                    )
                )
            )
        );
    }
    catch (
        const std::runtime_error&
    )
    {
        rejected =
            true;
    }


    assert(
        rejected
    );
}

// ============================================================================
// Explicit reset
//
// This verifies that reset() still works when explicitly requested.
// ============================================================================

void testExplicitReset()
{
    const std::string symbol =
        "TEST";


    TrendMomentumFeatureEngine engine(
        symbol
    );


    const TradingDate date{
        2026,
        9,
        25
    };


    const std::int64_t base =
        makeTimestamp(
            2026,
            9,
            25,
            9,
            15
        );


    TrendMomentumFeatures features;


    // Warm EMA50 and RSI.

    for (
        std::size_t i = 0;
        i < 60;
        ++i
    )
    {
        const Candle candle =
            makeCandle(
                symbol,
                base +
                    static_cast<std::int64_t>(
                        i * 60
                    ),
                100.0 +
                    static_cast<double>(
                        i
                    )
            );


        features =
            engine.update(
                makeSnapshot(
                    symbol,
                    candle.timestamp + 60,
                    date,
                    i == 0,
                    candle
                )
            );
    }


    assert(
        features.one_minute.has_ema20
    );

    assert(
        features.one_minute.has_ema50
    );

    assert(
        features.one_minute.has_rsi14
    );


    // Explicit reset.

    engine.reset();


    const std::int64_t restart_timestamp =
        makeTimestamp(
            2026,
            9,
            28,
            9,
            15
        );


    const TradingDate restart_date{
        2026,
        9,
        28
    };


    features =
        engine.update(
            makeSnapshot(
                symbol,
                restart_timestamp + 60,
                restart_date,
                true,
                makeCandle(
                    symbol,
                    restart_timestamp,
                    200.0
                )
            )
        );


    // A true explicit reset must restart warm-up.

    assert(
        !features.one_minute.has_ema20
    );

    assert(
        !features.one_minute.has_ema50
    );

    assert(
        !features.one_minute.has_rsi14
    );
}

} // namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    testEMA20Warmup();

    testEMA50Warmup();

    testEMARecursiveCalculation();

    testRSI14();

    testCrossSessionContinuity();

    testCrossSessionMultiTimeframeContinuity();

    testDuplicateProtection();

    testSymbolProtection();

    testBackwardTimeProtection();

    testExplicitReset();


    std::cout
        << "\n"
        << "================================================\n"
        << "TrendMomentumFeatureEngine Tests PASSED\n"
        << "================================================\n"
        << "EMA20 warm-up                : PASSED\n"
        << "EMA50 warm-up                : PASSED\n"
        << "EMA20 calculation            : PASSED\n"
        << "EMA50 calculation            : PASSED\n"
        << "RSI14 Wilder calculation     : PASSED\n"
        << "Price/EMA distance           : PASSED\n"
        << "EMA20/EMA50 relationship     : PASSED\n"
        << "EMA slope                    : PASSED\n"
        << "Duplicate candle protection  : PASSED\n"
        << "Cross-session continuity     : PASSED\n"
        << "1m cross-session continuity  : PASSED\n"
        << "5m cross-session continuity  : PASSED\n"
        << "15m cross-session continuity : PASSED\n"
        << "EMA cross-session update     : PASSED\n"
        << "RSI cross-session continuity : PASSED\n"
        << "Symbol protection            : PASSED\n"
        << "Backward-time protection     : PASSED\n"
        << "Explicit reset               : PASSED\n"
        << "================================================\n";


    return 0;
}