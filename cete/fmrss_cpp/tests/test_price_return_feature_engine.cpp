#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/PriceReturnFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/SessionState.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>

using devai::features::PriceReturnFeatureEngine;
using devai::features::PriceReturnFeatures;

using devai::market::Candle;
using devai::market::MarketSnapshot;
using devai::market::SessionPhase;
using devai::market::TradingDate;


namespace
{

constexpr double EPSILON = 1e-10;


// ============================================================================
// Floating-point comparison
// ============================================================================

bool approximatelyEqual(
    double left,
    double right,
    double epsilon = EPSILON)
{
    return std::fabs(left - right) <= epsilon;
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

    value.tm_year = year - 1900;
    value.tm_mon  = month - 1;
    value.tm_mday = day;

    value.tm_hour = hour;
    value.tm_min  = minute;
    value.tm_sec  = 0;

    value.tm_isdst = -1;

    return static_cast<std::int64_t>(
        std::mktime(&value)
    );
}


// ============================================================================
// Candle helper
// ============================================================================

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

    candle.symbol    = symbol;
    candle.timestamp = timestamp;

    candle.open  = open;
    candle.high  = high;
    candle.low   = low;
    candle.close = close;

    candle.volume = volume;

    return candle;
}


// ============================================================================
// MarketSnapshot helper
// ============================================================================

MarketSnapshot makeSnapshot(
    const std::string& symbol,
    std::int64_t decision_time,
    int year,
    int month,
    int day,
    bool new_session,
    const Candle& one_minute)
{
    MarketSnapshot snapshot;

    snapshot.symbol = symbol;

    snapshot.decision_time = decision_time;

    snapshot.trading_date = TradingDate{
        year,
        month,
        day
    };

    snapshot.session_phase =
        SessionPhase::ACTIVE;

    snapshot.new_session =
        new_session;

    snapshot.one_minute =
        one_minute;

    return snapshot;
}


// ============================================================================
// Create simple sequential 1-minute snapshot
// ============================================================================

MarketSnapshot makeSequentialSnapshot(
    const std::string& symbol,
    std::int64_t candle_time,
    int year,
    int month,
    int day,
    bool new_session,
    double close)
{
    const Candle candle =
        makeCandle(
            symbol,
            candle_time,
            close - 0.25,
            close + 0.50,
            close - 0.50,
            close,
            1000.0
        );

    return makeSnapshot(
        symbol,
        candle_time + 60,
        year,
        month,
        day,
        new_session,
        candle
    );
}

} // namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    const std::string symbol =
        "RELIANCE";


    // ========================================================================
    // TEST 1
    //
    // Candle return + candle geometry
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );

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
                100.0,
                102.0,
                99.0,
                101.0
            );

        const MarketSnapshot snapshot =
            makeSnapshot(
                symbol,
                timestamp + 60,
                2026,
                9,
                25,
                true,
                candle
            );

        const PriceReturnFeatures features =
            engine.update(snapshot);


        assert(
            features.has_one_minute
        );

        assert(
            approximatelyEqual(
                features.one_minute_candle_return,
                0.01
            )
        );

        assert(
            approximatelyEqual(
                features.candle_body_percent,
                0.01
            )
        );

        assert(
            approximatelyEqual(
                features.candle_range_percent,
                0.03
            )
        );

        assert(
            approximatelyEqual(
                features.close_location,
                2.0 / 3.0
            )
        );


        // First observation has no previous close.

        assert(
            std::isnan(
                features.one_minute_return
            )
        );

        assert(
            std::isnan(
                features.one_minute_log_return
            )
        );
    }


    // ========================================================================
    // TEST 2
    //
    // Close-to-close return + log return
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );

        const std::int64_t start =
            makeTimestamp(
                2026,
                9,
                25,
                9,
                15
            );


        const MarketSnapshot first =
            makeSequentialSnapshot(
                symbol,
                start,
                2026,
                9,
                25,
                true,
                100.0
            );

        (void) engine.update(first);


        const MarketSnapshot second =
            makeSequentialSnapshot(
                symbol,
                start + 60,
                2026,
                9,
                25,
                false,
                102.0
            );

        const PriceReturnFeatures features =
            engine.update(second);


        const double expected_return =
            (102.0 / 100.0) - 1.0;

        const double expected_log_return =
            std::log(
                102.0 / 100.0
            );


        assert(
            approximatelyEqual(
                features.one_minute_return,
                expected_return
            )
        );

        assert(
            approximatelyEqual(
                features.one_minute_log_return,
                expected_log_return
            )
        );
    }


    // ========================================================================
    // TEST 3
    //
    // 5-bar / 15-bar / 30-bar returns
    //
    // Use closes:
    //
    // 100, 101, 102, ... 130
    //
    // Total = 31 observations
    //
    // At close 130:
    //
    // 5-bar  reference = 125
    // 15-bar reference = 115
    // 30-bar reference = 100
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );

        const std::int64_t start =
            makeTimestamp(
                2026,
                9,
                25,
                9,
                15
            );

        PriceReturnFeatures features;


        for (
            int index = 0;
            index <= 30;
            ++index
        )
        {
            const double close =
                100.0 +
                static_cast<double>(index);

            const MarketSnapshot snapshot =
                makeSequentialSnapshot(
                    symbol,
                    start +
                        (
                            static_cast<std::int64_t>(index) *
                            60
                        ),
                    2026,
                    9,
                    25,
                    index == 0,
                    close
                );

            features =
                engine.update(snapshot);
        }


        assert(
            engine.oneMinuteHistorySize() ==
            31
        );

        assert(
            features.has_5_bar_history
        );

        assert(
            features.has_15_bar_history
        );

        assert(
            features.has_30_bar_history
        );


        const double expected_5_bar =
            (130.0 / 125.0) - 1.0;

        const double expected_15_bar =
            (130.0 / 115.0) - 1.0;

        const double expected_30_bar =
            (130.0 / 100.0) - 1.0;


        assert(
            approximatelyEqual(
                features.return_5_bars,
                expected_5_bar
            )
        );

        assert(
            approximatelyEqual(
                features.return_15_bars,
                expected_15_bar
            )
        );

        assert(
            approximatelyEqual(
                features.return_30_bars,
                expected_30_bar
            )
        );
    }


    // ========================================================================
    // TEST 4
    //
    // Duplicate snapshot protection
    //
    // Reprocessing the exact same candle must NOT increase history.
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );

        const std::int64_t timestamp =
            makeTimestamp(
                2026,
                9,
                25,
                10,
                0
            );

        const MarketSnapshot snapshot =
            makeSequentialSnapshot(
                symbol,
                timestamp,
                2026,
                9,
                25,
                false,
                150.0
            );


        (void) engine.update(snapshot);

        assert(
            engine.oneMinuteHistorySize() ==
            1
        );


        (void) engine.update(snapshot);

        assert(
            engine.oneMinuteHistorySize() ==
            1
        );


        (void) engine.update(snapshot);

        assert(
            engine.oneMinuteHistorySize() ==
            1
        );
    }


    // ========================================================================
    // TEST 5
    //
    // CROSS-SESSION CONTINUITY
    //
    // CRITICAL CETE RULE:
    //
    // snapshot.new_session == true
    //
    // MUST NOT clear continuous price/return history.
    //
    //
    // Day 1:
    //
    //     31 observations
    //     final close = 130
    //
    // Day 2:
    //
    //     first observation close = 133
    //
    //
    // Expected:
    //
    //     history before = 31
    //     history after  = 32
    //
    // and all rolling-return warm-up remains available.
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );


        const std::int64_t day_one_start =
            makeTimestamp(
                2026,
                9,
                25,
                9,
                15
            );


        PriceReturnFeatures features;


        // --------------------------------------------------------------------
        // Build 31 observations on Day 1.
        // --------------------------------------------------------------------

        for (
            int index = 0;
            index <= 30;
            ++index
        )
        {
            const double close =
                100.0 +
                static_cast<double>(index);


            const MarketSnapshot snapshot =
                makeSequentialSnapshot(
                    symbol,
                    day_one_start +
                        (
                            static_cast<std::int64_t>(index) *
                            60
                        ),
                    2026,
                    9,
                    25,
                    index == 0,
                    close
                );


            features =
                engine.update(snapshot);
        }


        const std::size_t
            history_before_new_session =
                engine.oneMinuteHistorySize();


        assert(
            history_before_new_session ==
            31
        );


        assert(
            features.has_5_bar_history
        );

        assert(
            features.has_15_bar_history
        );

        assert(
            features.has_30_bar_history
        );


        // --------------------------------------------------------------------
        // Day 2.
        //
        // Important:
        //
        // There are no artificial overnight observations.
        //
        // The first real completed trading candle of the new session simply
        // follows the final real candle of the previous session.
        // --------------------------------------------------------------------

        const std::int64_t day_two_time =
            makeTimestamp(
                2026,
                9,
                28,
                9,
                15
            );


        const MarketSnapshot day_two_snapshot =
            makeSequentialSnapshot(
                symbol,
                day_two_time,
                2026,
                9,
                28,
                true,
                133.0
            );


        const PriceReturnFeatures
            day_two_features =
                engine.update(
                    day_two_snapshot
                );


        const std::size_t
            history_after_new_session =
                engine.oneMinuteHistorySize();


        // --------------------------------------------------------------------
        // This is the key assertion.
        //
        // If update() incorrectly clears history on new_session,
        // this becomes 1 and the test fails.
        // --------------------------------------------------------------------

        assert(
            history_after_new_session ==
            32
        );


        // --------------------------------------------------------------------
        // Warmed rolling features must remain warmed.
        // --------------------------------------------------------------------

        assert(
            day_two_features.has_5_bar_history
        );

        assert(
            day_two_features.has_15_bar_history
        );

        assert(
            day_two_features.has_30_bar_history
        );


        // --------------------------------------------------------------------
        // First new-session close-to-close return:
        //
        // previous real close = 130
        // current real close  = 133
        //
        // This intentionally includes the session transition.
        // --------------------------------------------------------------------

        const double expected_return =
            (133.0 / 130.0) - 1.0;


        const double expected_log_return =
            std::log(
                133.0 / 130.0
            );


        assert(
            approximatelyEqual(
                day_two_features.one_minute_return,
                expected_return
            )
        );


        assert(
            approximatelyEqual(
                day_two_features.one_minute_log_return,
                expected_log_return
            )
        );


        // --------------------------------------------------------------------
        // 30-bar history must still refer into Day 1.
        //
        // Before new session:
        //
        // closes = 100 ... 130
        //
        // After adding 133:
        //
        // 32 observations total.
        //
        // 30 observations behind 133 is close 101.
        // --------------------------------------------------------------------

        const double expected_30_bar =
            (133.0 / 101.0) - 1.0;


        assert(
            approximatelyEqual(
                day_two_features.return_30_bars,
                expected_30_bar
            )
        );
    }


    // ========================================================================
    // TEST 6
    //
    // Bounded history
    //
    // maximum_history = 31
    //
    // After more than 31 candles, history must remain exactly 31.
    //
    // This is different from session reset.
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            31
        );


        const std::int64_t start =
            makeTimestamp(
                2026,
                9,
                25,
                9,
                15
            );


        for (
            int index = 0;
            index < 50;
            ++index
        )
        {
            const MarketSnapshot snapshot =
                makeSequentialSnapshot(
                    symbol,
                    start +
                        (
                            static_cast<std::int64_t>(index) *
                            60
                        ),
                    2026,
                    9,
                    25,
                    index == 0,
                    100.0 +
                        static_cast<double>(index)
                );


            (void) engine.update(snapshot);
        }


        assert(
            engine.oneMinuteHistorySize() ==
            31
        );
    }


    // ========================================================================
    // TEST 7
    //
    // Backward decision-time protection
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );


        const std::int64_t timestamp =
            makeTimestamp(
                2026,
                9,
                25,
                11,
                0
            );


        const MarketSnapshot first =
            makeSequentialSnapshot(
                symbol,
                timestamp,
                2026,
                9,
                25,
                false,
                200.0
            );


        (void) engine.update(first);


        MarketSnapshot backward =
            makeSequentialSnapshot(
                symbol,
                timestamp + 60,
                2026,
                9,
                25,
                false,
                201.0
            );


        // Make decision time older than the previous decision time.

        backward.decision_time =
            first.decision_time - 1;


        bool rejected =
            false;


        try
        {
            (void) engine.update(
                backward
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


    // ========================================================================
    // TEST 8
    //
    // Symbol protection
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );


        const std::int64_t timestamp =
            makeTimestamp(
                2026,
                9,
                25,
                11,
                15
            );


        MarketSnapshot snapshot =
            makeSequentialSnapshot(
                "SBIN",
                timestamp,
                2026,
                9,
                25,
                false,
                500.0
            );


        bool rejected =
            false;


        try
        {
            (void) engine.update(
                snapshot
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


    // ========================================================================
    // TEST 9
    //
    // Explicit reset
    //
    // Unlike new_session, reset() MUST clear all timeframe histories and
    // decision-time state.
    // ========================================================================

    {
        PriceReturnFeatureEngine engine(
            symbol,
            64
        );


        const std::int64_t start =
            makeTimestamp(
                2026,
                9,
                25,
                9,
                15
            );


        PriceReturnFeatures features;


        // Build enough history for every rolling feature.

        for (
            int index = 0;
            index <= 30;
            ++index
        )
        {
            const MarketSnapshot snapshot =
                makeSequentialSnapshot(
                    symbol,
                    start +
                        (
                            static_cast<std::int64_t>(index) *
                            60
                        ),
                    2026,
                    9,
                    25,
                    index == 0,
                    100.0 +
                        static_cast<double>(index)
                );


            features =
                engine.update(snapshot);
        }


        assert(
            engine.oneMinuteHistorySize() ==
            31
        );

        assert(
            features.has_30_bar_history
        );


        // --------------------------------------------------------------------
        // Explicit reset.
        // --------------------------------------------------------------------

        engine.reset();


        assert(
            engine.oneMinuteHistorySize() ==
            0
        );

        assert(
            engine.fiveMinuteHistorySize() ==
            0
        );

        assert(
            engine.fifteenMinuteHistorySize() ==
            0
        );


        // --------------------------------------------------------------------
        // First observation after reset.
        //
        // History must begin from scratch.
        // --------------------------------------------------------------------

        const std::int64_t restart_time =
            makeTimestamp(
                2026,
                9,
                29,
                9,
                15
            );


        const MarketSnapshot restart =
            makeSequentialSnapshot(
                symbol,
                restart_time,
                2026,
                9,
                29,
                true,
                200.0
            );


        const PriceReturnFeatures
            restarted_features =
                engine.update(
                    restart
                );


        assert(
            engine.oneMinuteHistorySize() ==
            1
        );


        assert(
            !restarted_features.has_5_bar_history
        );

        assert(
            !restarted_features.has_15_bar_history
        );

        assert(
            !restarted_features.has_30_bar_history
        );


        assert(
            std::isnan(
                restarted_features.one_minute_return
            )
        );


        assert(
            std::isnan(
                restarted_features.one_minute_log_return
            )
        );
    }


    // ========================================================================
    // SUCCESS
    // ========================================================================

    std::cout
        << "\n"
        << "================================================\n"
        << "PriceReturnFeatureEngine Tests PASSED\n"
        << "================================================\n"
        << "Candle return                 : PASSED\n"
        << "Close-to-close return         : PASSED\n"
        << "Log return                    : PASSED\n"
        << "5-bar return                  : PASSED\n"
        << "15-bar return                 : PASSED\n"
        << "30-bar return                 : PASSED\n"
        << "Candle geometry               : PASSED\n"
        << "Duplicate snapshot protection : PASSED\n"
        << "Cross-session continuity      : PASSED\n"
        << "Cross-session return          : PASSED\n"
        << "Bounded history               : PASSED\n"
        << "Backward-time protection      : PASSED\n"
        << "Symbol protection             : PASSED\n"
        << "Explicit reset                : PASSED\n"
        << "================================================\n";


    return 0;
}