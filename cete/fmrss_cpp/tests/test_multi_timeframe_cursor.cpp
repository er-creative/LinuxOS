#include "devai/market/Candle.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace devai::market;


namespace
{

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

}


int main()
{
    const std::int64_t start =
        makeTimestamp(
            2026,
            9,
            1,
            9,
            15
        );


    // =====================================================
    // Artificial completed market candles
    // =====================================================

    std::vector<Candle> one_minute;

    for (int minute = 0; minute < 15; ++minute)
    {
        one_minute.push_back(
            makeCandle(
                "TEST",
                start + minute * 60,
                100.0 + minute
            )
        );
    }


    std::vector<Candle> five_minute{
        makeCandle(
            "TEST",
            start,
            100.0
        ),

        makeCandle(
            "TEST",
            start + 5 * 60,
            105.0
        ),

        makeCandle(
            "TEST",
            start + 10 * 60,
            110.0
        )
    };


    std::vector<Candle> fifteen_minute{
        makeCandle(
            "TEST",
            start,
            100.0
        )
    };


    // =====================================================
    // Prepare availability timestamps
    // =====================================================

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


    // =====================================================
    // 09:16
    //
    // 1m  : 09:15
    // 5m  : unavailable
    // 15m : unavailable
    // =====================================================

    cursor.advanceTo(
        start + 60
    );


    assert(
        cursor.state().one_minute.has_value()
    );

    assert(
        cursor.state().one_minute
            ->candle.timestamp ==
        start
    );

    assert(
        !cursor.state().five_minute.has_value()
    );

    assert(
        !cursor.state().fifteen_minute.has_value()
    );


    // =====================================================
    // 09:20
    //
    // 1m : 09:19
    // 5m : 09:15
    // =====================================================

    cursor.advanceTo(
        start + 5 * 60
    );


    assert(
        cursor.state().one_minute
            ->candle.timestamp ==
        start + 4 * 60
    );

    assert(
        cursor.state().five_minute.has_value()
    );

    assert(
        cursor.state().five_minute
            ->candle.timestamp ==
        start
    );

    assert(
        !cursor.state().fifteen_minute.has_value()
    );


    // =====================================================
    // 09:25
    //
    // 1m : 09:24
    // 5m : 09:20
    // =====================================================

    cursor.advanceTo(
        start + 10 * 60
    );


    assert(
        cursor.state().one_minute
            ->candle.timestamp ==
        start + 9 * 60
    );

    assert(
        cursor.state().five_minute
            ->candle.timestamp ==
        start + 5 * 60
    );


    // =====================================================
    // 09:30
    //
    // 1m  : 09:29
    // 5m  : 09:25
    // 15m : 09:15
    // =====================================================

    cursor.advanceTo(
        start + 15 * 60
    );


    assert(
        cursor.state().one_minute
            ->candle.timestamp ==
        start + 14 * 60
    );

    assert(
        cursor.state().five_minute
            ->candle.timestamp ==
        start + 10 * 60
    );

    assert(
        cursor.state().fifteen_minute.has_value()
    );

    assert(
        cursor.state().fifteen_minute
            ->candle.timestamp ==
        start
    );

    assert(
        cursor.fullySynchronized()
    );


    // =====================================================
    // Forward-only protection
    // =====================================================

    bool backward_rejected =
        false;

    try
    {
        cursor.advanceTo(
            start + 14 * 60
        );
    }
    catch (const std::invalid_argument&)
    {
        backward_rejected =
            true;
    }


    assert(
        backward_rejected
    );


    // =====================================================
    // Reset
    // =====================================================

    cursor.reset();


    assert(
        !cursor.state().one_minute.has_value()
    );

    assert(
        !cursor.state().five_minute.has_value()
    );

    assert(
        !cursor.state().fifteen_minute.has_value()
    );

    assert(
        cursor.oneMinuteIndex() == 0
    );

    assert(
        cursor.fiveMinuteIndex() == 0
    );

    assert(
        cursor.fifteenMinuteIndex() == 0
    );


    // =====================================================
    // Reuse after reset
    // =====================================================

    cursor.advanceTo(
        start + 5 * 60
    );


    assert(
        cursor.state().one_minute
            ->candle.timestamp ==
        start + 4 * 60
    );

    assert(
        cursor.state().five_minute
            ->candle.timestamp ==
        start
    );


    std::cout
        << "\n"
        << "============================================\n"
        << "MultiTimeframeCursor Tests PASSED\n"
        << "============================================\n"
        << "Forward-only advancement : PASSED\n"
        << "1-minute availability    : PASSED\n"
        << "5-minute availability    : PASSED\n"
        << "15-minute availability   : PASSED\n"
        << "Look-ahead protection    : PASSED\n"
        << "Backward-time rejection  : PASSED\n"
        << "Reset/reuse              : PASSED\n"
        << "============================================\n";


    return 0;
}