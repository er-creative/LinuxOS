#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

using namespace devai::market;


namespace
{

// ============================================================================
// Local Timestamp
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


    return static_cast<std::int64_t>(
        std::mktime(&value)
    );
}


// ============================================================================
// Candle Factory
// ============================================================================

Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double price)
{
    Candle candle;

    candle.symbol =
        symbol;

    candle.timestamp =
        timestamp;

    candle.open =
        price;

    candle.high =
        price + 1.0;

    candle.low =
        price - 1.0;

    candle.close =
        price + 0.5;

    candle.volume =
        1000;

    return candle;
}

}


// ============================================================================
// Main
// ============================================================================

int main()
{
    const std::string symbol =
        "TEST";


    // ========================================================================
    // Two trading sessions
    // ========================================================================

    const std::int64_t day1 =
        makeTimestamp(
            2026, 9, 1,
            9, 15
        );


    const std::int64_t day2 =
        makeTimestamp(
            2026, 9, 2,
            9, 15
        );


    // ========================================================================
    // 1-minute candles
    //
    // Day 1: 09:15 -> 09:29
    // Day 2: 09:15 -> 09:29
    // ========================================================================

    std::vector<Candle> one_minute;


    for (int minute = 0;
         minute < 15;
         ++minute)
    {
        one_minute.push_back(
            makeCandle(
                symbol,
                day1 + minute * 60,
                100.0 + minute
            )
        );
    }


    for (int minute = 0;
         minute < 15;
         ++minute)
    {
        one_minute.push_back(
            makeCandle(
                symbol,
                day2 + minute * 60,
                200.0 + minute
            )
        );
    }


    // ========================================================================
    // 5-minute candles
    // ========================================================================

    std::vector<Candle> five_minute{
        makeCandle(
            symbol,
            day1,
            100.0
        ),

        makeCandle(
            symbol,
            day1 + 5 * 60,
            105.0
        ),

        makeCandle(
            symbol,
            day1 + 10 * 60,
            110.0
        ),

        makeCandle(
            symbol,
            day2,
            200.0
        ),

        makeCandle(
            symbol,
            day2 + 5 * 60,
            205.0
        ),

        makeCandle(
            symbol,
            day2 + 10 * 60,
            210.0
        )
    };


    // ========================================================================
    // 15-minute candles
    // ========================================================================

    std::vector<Candle> fifteen_minute{
        makeCandle(
            symbol,
            day1,
            100.0
        ),

        makeCandle(
            symbol,
            day2,
            200.0
        )
    };


    // ========================================================================
    // Prepare availability streams
    // ========================================================================

    MultiTimeframeSynchronizerConfig config;

    config.mode =
        AvailabilityMode::ZERO_LATENCY;


    MultiTimeframeSynchronizer synchronizer(
        config
    );


    const auto timed_one =
        synchronizer.prepareOneMinute(
            one_minute
        );


    const auto timed_five =
        synchronizer.prepareFiveMinute(
            five_minute
        );


    const auto timed_fifteen =
        synchronizer.prepareFifteenMinute(
            fifteen_minute
        );


    MultiTimeframeCursor cursor(
        timed_one,
        timed_five,
        timed_fifteen
    );


    SessionState session;


    MarketSnapshotBuilder builder(
        symbol
    );


    // ========================================================================
    // TEST 1
    //
    // Day 1 - 09:16
    //
    // 1m available
    // 5m unavailable
    // 15m unavailable
    // ========================================================================

    MarketSnapshot snapshot =
        builder.build(
            day1 + 60,
            cursor,
            session
        );


    assert(
        snapshot.symbol ==
        symbol
    );


    assert(
        snapshot.sessionActive()
    );


    assert(
        snapshot.new_session
    );


    assert(
        snapshot.hasOneMinute()
    );


    assert(
        !snapshot.hasFiveMinute()
    );


    assert(
        !snapshot.hasFifteenMinute()
    );


    assert(
        !snapshot.fullySynchronized()
    );


    assert(
        snapshot.one_minute
            ->timestamp ==
        day1
    );


    // ========================================================================
    // TEST 2
    //
    // Day 1 - 09:20
    //
    // 1m = 09:19
    // 5m = 09:15
    // 15m unavailable
    // ========================================================================

    snapshot =
        builder.build(
            day1 + 5 * 60,
            cursor,
            session
        );


    assert(
        !snapshot.new_session
    );


    assert(
        snapshot.hasOneMinute()
    );


    assert(
        snapshot.hasFiveMinute()
    );


    assert(
        !snapshot.hasFifteenMinute()
    );


    assert(
        snapshot.one_minute
            ->timestamp ==
        day1 + 4 * 60
    );


    assert(
        snapshot.five_minute
            ->timestamp ==
        day1
    );


    // ========================================================================
    // TEST 3
    //
    // Day 1 - 09:30
    //
    // all three timeframes available
    // ========================================================================

    snapshot =
        builder.build(
            day1 + 15 * 60,
            cursor,
            session
        );


    assert(
        snapshot.hasOneMinute()
    );


    assert(
        snapshot.hasFiveMinute()
    );


    assert(
        snapshot.hasFifteenMinute()
    );


    assert(
        snapshot.fullySynchronized()
    );


    assert(
        snapshot.one_minute
            ->timestamp ==
        day1 + 14 * 60
    );


    assert(
        snapshot.five_minute
            ->timestamp ==
        day1 + 10 * 60
    );


    assert(
        snapshot.fifteen_minute
            ->timestamp ==
        day1
    );


    // ========================================================================
    // TEST 4
    //
    // Jump to Day 2 at 09:15.
    //
    // The cursor internally still knows Day 1's latest values.
    //
    // SessionState must prevent those candles from entering the snapshot.
    // ========================================================================

    snapshot =
        builder.build(
            day2,
            cursor,
            session
        );


    assert(
        snapshot.new_session
    );


    assert(
        snapshot.sessionActive()
    );


    assert(
        !snapshot.hasOneMinute()
    );


    assert(
        !snapshot.hasFiveMinute()
    );


    assert(
        !snapshot.hasFifteenMinute()
    );


    assert(
        !snapshot.fullySynchronized()
    );


    // ========================================================================
    // TEST 5
    //
    // Day 2 - 09:16
    //
    // Today's first completed 1-minute candle is now available.
    //
    // Yesterday's 5m / 15m candles must remain hidden.
    // ========================================================================

    snapshot =
        builder.build(
            day2 + 60,
            cursor,
            session
        );


    assert(
        !snapshot.new_session
    );


    assert(
        snapshot.hasOneMinute()
    );


    assert(
        !snapshot.hasFiveMinute()
    );


    assert(
        !snapshot.hasFifteenMinute()
    );


    assert(
        snapshot.one_minute
            ->timestamp ==
        day2
    );


    // ========================================================================
    // TEST 6
    //
    // Day 2 - 09:20
    // ========================================================================

    snapshot =
        builder.build(
            day2 + 5 * 60,
            cursor,
            session
        );


    assert(
        snapshot.hasOneMinute()
    );


    assert(
        snapshot.hasFiveMinute()
    );


    assert(
        !snapshot.hasFifteenMinute()
    );


    assert(
        snapshot.five_minute
            ->timestamp ==
        day2
    );


    // ========================================================================
    // TEST 7
    //
    // Day 2 - 09:30
    //
    // Fully synchronized again.
    // ========================================================================

    snapshot =
        builder.build(
            day2 + 15 * 60,
            cursor,
            session
        );


    assert(
        snapshot.fullySynchronized()
    );


    assert(
        snapshot.one_minute
            ->timestamp ==
        day2 + 14 * 60
    );


    assert(
        snapshot.five_minute
            ->timestamp ==
        day2 + 10 * 60
    );


    assert(
        snapshot.fifteen_minute
            ->timestamp ==
        day2
    );


    // ========================================================================
    // TEST 8
    //
    // Snapshot is a copy.
    //
    // Advancing the runtime must not mutate an old snapshot.
    // ========================================================================

    const MarketSnapshot saved_snapshot =
        snapshot;


    const std::int64_t saved_one_timestamp =
        saved_snapshot.one_minute
            ->timestamp;


    // Same timestamp is permitted by the forward-only cursor.
    snapshot =
        builder.build(
            day2 + 15 * 60,
            cursor,
            session
        );


    assert(
        saved_snapshot.one_minute
            ->timestamp ==
        saved_one_timestamp
    );


    // ========================================================================
    // RESULT
    // ========================================================================

    std::cout
        << "\n"
        << "============================================\n"
        << "MarketSnapshot Tests PASSED\n"
        << "============================================\n"
        << "Symbol integrity             : PASSED\n"
        << "Decision-time state          : PASSED\n"
        << "1-minute snapshot            : PASSED\n"
        << "5-minute snapshot            : PASSED\n"
        << "15-minute snapshot           : PASSED\n"
        << "Full synchronization         : PASSED\n"
        << "New-session detection        : PASSED\n"
        << "Previous-day protection      : PASSED\n"
        << "Snapshot copy independence   : PASSED\n"
        << "============================================\n";


    return 0;
}