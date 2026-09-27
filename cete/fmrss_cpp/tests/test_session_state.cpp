#include "devai/market/SessionState.hpp"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>

using namespace devai::market;


namespace
{

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


    return static_cast<std::int64_t>(
        std::mktime(&value)
    );
}


Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        100.0;

    candle.high =
        101.0;

    candle.low =
        99.0;

    candle.close =
        100.5;

    candle.volume =
        1000;

    return candle;
}


TimedCandle makeTimed(
    const std::string& symbol,
    std::int64_t timestamp,
    std::int64_t completed_at)
{
    TimedCandle result;

    result.candle =
        makeCandle(
            symbol,
            timestamp
        );

    result.completed_at =
        completed_at;

    result.received_at =
        completed_at;

    return result;
}

}


// ============================================================================
// Main
// ============================================================================

int main()
{
    SessionState session;


    const std::int64_t day1_0914 =
        makeTimestamp(
            2026, 9, 1,
            9, 14
        );

    const std::int64_t day1_0915 =
        makeTimestamp(
            2026, 9, 1,
            9, 15
        );

    const std::int64_t day1_0920 =
        makeTimestamp(
            2026, 9, 1,
            9, 20
        );

    const std::int64_t day1_1529 =
        makeTimestamp(
            2026, 9, 1,
            15, 29
        );

    const std::int64_t day1_1530 =
        makeTimestamp(
            2026, 9, 1,
            15, 30
        );


    // =====================================================
    // BEFORE OPEN
    // =====================================================

    bool new_session =
        session.update(
            day1_0914
        );


    assert(
        new_session
    );

    assert(
        session.beforeOpen()
    );

    assert(
        !session.active()
    );


    // =====================================================
    // OPEN
    // =====================================================

    new_session =
        session.update(
            day1_0915
        );


    assert(
        !new_session
    );

    assert(
        session.active()
    );


    // =====================================================
    // DURING SESSION
    // =====================================================

    (void) session.update(
        day1_1529
    );


    assert(
        session.active()
    );


    // =====================================================
    // CLOSE
    //
    // 15:30 is no longer ACTIVE.
    // =====================================================

    (void) session.update(
        day1_1530
    );


    assert(
        session.afterClose()
    );

    assert(
        !session.active()
    );


    // =====================================================
    // NEXT DAY
    //
    // Create a state containing yesterday's candles.
    // =====================================================

    MultiTimeframeState state;

    state.decision_time =
        makeTimestamp(
            2026, 9, 2,
            9, 15
        );


    state.one_minute =
        makeTimed(
            "TEST",
            makeTimestamp(
                2026, 9, 1,
                15, 29
            ),
            makeTimestamp(
                2026, 9, 1,
                15, 30
            )
        );


    state.five_minute =
        makeTimed(
            "TEST",
            makeTimestamp(
                2026, 9, 1,
                15, 25
            ),
            makeTimestamp(
                2026, 9, 1,
                15, 30
            )
        );


    state.fifteen_minute =
        makeTimed(
            "TEST",
            makeTimestamp(
                2026, 9, 1,
                15, 15
            ),
            makeTimestamp(
                2026, 9, 1,
                15, 30
            )
        );


    // =====================================================
    // New trading date
    // =====================================================

    const bool day2 =
        session.update(
            state.decision_time
        );


    assert(
        day2
    );

    assert(
        session.active()
    );


    // =====================================================
    // Filter previous-day carryover
    // =====================================================

    session.filter(
        state
    );


    assert(
        !state.one_minute.has_value()
    );

    assert(
        !state.five_minute.has_value()
    );

    assert(
        !state.fifteen_minute.has_value()
    );


    // =====================================================
    // Today's first completed 1-minute candle
    //
    // At 09:16:
    //
    // 1m 09:15 is valid
    // 5m unavailable
    // 15m unavailable
    // =====================================================

    const std::int64_t day2_0915 =
        makeTimestamp(
            2026, 9, 2,
            9, 15
        );

    const std::int64_t day2_0916 =
        makeTimestamp(
            2026, 9, 2,
            9, 16
        );


    state.decision_time =
        day2_0916;


    state.one_minute =
        makeTimed(
            "TEST",
            day2_0915,
            day2_0916
        );


    (void) session.update(
        day2_0916
    );


    session.filter(
        state
    );


    assert(
        state.one_minute.has_value()
    );

    assert(
        !state.five_minute.has_value()
    );

    assert(
        !state.fifteen_minute.has_value()
    );


    // =====================================================
    // Today's first completed 5-minute candle
    //
    // At 09:20:
    //
    // 5m 09:15 is valid.
    // =====================================================

    state.decision_time =
        day1_0920 +
        24 * 60 * 60;


    state.five_minute =
        makeTimed(
            "TEST",
            day2_0915,
            makeTimestamp(
                2026, 9, 2,
                9, 20
            )
        );


    (void) session.update(
        state.decision_time
    );


    session.filter(
        state
    );


    assert(
        state.one_minute.has_value()
    );

    assert(
        state.five_minute.has_value()
    );

    assert(
        !state.fifteen_minute.has_value()
    );


    // =====================================================
    // RESET
    // =====================================================

    session.reset();


    assert(
        !session.initialized()
    );


    std::cout
        << "\n"
        << "============================================\n"
        << "SessionState Tests PASSED\n"
        << "============================================\n"
        << "Before-open detection        : PASSED\n"
        << "Active-session detection     : PASSED\n"
        << "After-close detection        : PASSED\n"
        << "New-session detection        : PASSED\n"
        << "Previous-day 1m protection   : PASSED\n"
        << "Previous-day 5m protection   : PASSED\n"
        << "Previous-day 15m protection  : PASSED\n"
        << "Current-session retention    : PASSED\n"
        << "Reset                        : PASSED\n"
        << "============================================\n";


    return 0;
}