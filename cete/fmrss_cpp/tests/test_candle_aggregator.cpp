#include "devai/market/CandleAggregator.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using devai::market::Candle;
using devai::market::CandleAggregator;
using devai::market::Timeframe;


// =========================================================
// Helper
// =========================================================

Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double open,
    double high,
    double low,
    double close,
    std::uint64_t volume)
{
    Candle candle;

    candle.symbol = symbol;
    candle.timestamp = timestamp;

    candle.open = open;
    candle.high = high;
    candle.low = low;
    candle.close = close;

    candle.volume = volume;

    return candle;
}


// =========================================================
// Main Test
// =========================================================

int main()
{
    CandleAggregator aggregator;

    // -----------------------------------------------------
    // Use an artificial timestamp aligned to a 15-minute
    // boundary.
    //
    // 900 seconds = 15 minutes.
    // -----------------------------------------------------

    constexpr std::int64_t start =
        1'800'000'000;

    static_assert(start % 900 == 0);

    std::vector<Candle> one_minute;

    one_minute.reserve(15);

    // -----------------------------------------------------
    // Create 15 sequential 1-minute candles.
    // -----------------------------------------------------

    for (int i = 0; i < 15; ++i)
    {
        const double open =
            100.0 + i;

        const double close =
            open + 0.5;

        one_minute.push_back(
            makeCandle(
                "TEST",
                start + (i * 60),
                open,
                open + 1.0,
                open - 1.0,
                close,
                1000
            )
        );
    }


    // =====================================================
    // Test 5-Minute Aggregation
    // =====================================================

    const auto five =
        aggregator.aggregate(
            one_minute,
            Timeframe::FIVE_MINUTES
        );

    assert(five.candles.size() == 3);
    assert(five.completed_buckets == 3);
    assert(five.incomplete_buckets == 0);

    // First 5-minute candle:
    //
    // open  = first candle open = 100
    // high  = highest = 105
    // low   = lowest = 99
    // close = fifth candle close = 104.5
    // volume = 5 * 1000

    assert(five.candles[0].open == 100.0);
    assert(five.candles[0].high == 105.0);
    assert(five.candles[0].low == 99.0);
    assert(five.candles[0].close == 104.5);
    assert(five.candles[0].volume == 5000);

    assert(
        five.candles[0].timestamp ==
        start
    );


    // =====================================================
    // Test 15-Minute Aggregation
    // =====================================================

    const auto fifteen =
        aggregator.aggregate(
            one_minute,
            Timeframe::FIFTEEN_MINUTES
        );

    assert(fifteen.candles.size() == 1);
    assert(fifteen.completed_buckets == 1);
    assert(fifteen.incomplete_buckets == 0);

    assert(
        fifteen.candles[0].open ==
        100.0
    );

    assert(
        fifteen.candles[0].high ==
        115.0
    );

    assert(
        fifteen.candles[0].low ==
        99.0
    );

    assert(
        fifteen.candles[0].close ==
        114.5
    );

    assert(
        fifteen.candles[0].volume ==
        15000
    );

    assert(
        fifteen.candles[0].timestamp ==
        start
    );


    // =====================================================
    // Missing Candle Test
    //
    // Remove one minute from the middle.
    //
    // The corresponding 5-minute bucket must NOT be
    // generated.
    // =====================================================

    std::vector<Candle> missing =
        one_minute;

    // Remove minute #2.
    missing.erase(
        missing.begin() + 2
    );

    const auto incomplete =
        aggregator.aggregate(
            missing,
            Timeframe::FIVE_MINUTES
        );

    assert(
        incomplete.candles.size() == 2
    );

    assert(
        incomplete.incomplete_buckets == 1
    );


    std::cout
        << "========================================\n"
        << "CandleAggregator Tests PASSED\n"
        << "========================================\n"
        << "5m candles  : "
        << five.candles.size()
        << '\n'
        << "15m candles : "
        << fifteen.candles.size()
        << '\n'
        << "Missing-bar detection : PASSED\n"
        << "========================================\n";

    return 0;
}