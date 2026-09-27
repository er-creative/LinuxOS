#include "devai/market/Candle.hpp"
#include "devai/market/MarketDataRepairEngine.hpp"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>
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

}


int main()
{
    MarketDataRepairEngine engine;


    // =====================================================
    // Use an actual local-time session timestamp.
    //
    // The absolute date does not matter for this artificial
    // test; it only needs to represent a normal session.
    // =====================================================

    std::tm value{};

    value.tm_year = 2026 - 1900;
    value.tm_mon = 8;       // September
    value.tm_mday = 1;
    value.tm_hour = 9;
    value.tm_min = 15;
    value.tm_sec = 0;
    value.tm_isdst = -1;

    const std::int64_t start =
        static_cast<std::int64_t>(
            std::mktime(&value)
        );


    // =====================================================
    // Existing:
    //
    // 09:15
    // 09:16
    // missing 09:17
    // missing 09:18
    // 09:19
    // =====================================================

    std::vector<Candle> existing{
        makeCandle(
            "TEST",
            start,
            100.0
        ),

        makeCandle(
            "TEST",
            start + 60,
            101.0
        ),

        makeCandle(
            "TEST",
            start + 240,
            104.0
        )
    };


    const auto gaps =
        engine.findInternalGaps(
            "TEST",
            existing
        );


    assert(
        gaps.size() == 1
    );

    assert(
        gaps[0].missing_minutes == 2
    );

    assert(
        gaps[0].first_missing_timestamp ==
        start + 120
    );

    assert(
        gaps[0].last_missing_timestamp ==
        start + 180
    );


    // =====================================================
    // Simulated provider response.
    // =====================================================

    std::vector<Candle> downloaded{
        makeCandle(
            "TEST",
            start + 120,
            102.0
        ),

        makeCandle(
            "TEST",
            start + 180,
            103.0
        )
    };


    std::vector<Candle> repaired;


    const RepairResult result =
        engine.repair(
            "TEST",
            existing,
            downloaded,
            repaired
        );


    assert(
        result.gaps_found == 1
    );

    assert(
        result.missing_candles_requested == 2
    );

    assert(
        result.replacement_candles_accepted == 2
    );

    assert(
        repaired.size() == 5
    );

    assert(
        result.success
    );


    const auto remaining =
        engine.findInternalGaps(
            "TEST",
            repaired
        );


    assert(
        remaining.empty()
    );


    std::cout
        << "\n"
        << "============================================\n"
        << "MarketDataRepairEngine Tests PASSED\n"
        << "============================================\n"
        << "Original candles     : "
        << result.original_candles
        << '\n'
        << "Gap events           : "
        << result.gaps_found
        << '\n'
        << "Missing candles      : "
        << result.missing_candles_requested
        << '\n'
        << "Replacement received : "
        << result.replacement_candles_received
        << '\n'
        << "Replacement accepted : "
        << result.replacement_candles_accepted
        << '\n'
        << "Final candles        : "
        << result.final_candles
        << '\n'
        << "Remaining gaps       : "
        << remaining.size()
        << '\n'
        << "============================================\n";


    return 0;
}