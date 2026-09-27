#include "devai/market/Candle.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using namespace devai::market;


// =========================================================
// Test Helpers
// =========================================================

void require(
    bool condition,
    const std::string& message
)
{
    if (!condition)
    {
        std::cerr
            << "\nTEST FAILED: "
            << message
            << '\n';

        std::exit(EXIT_FAILURE);
    }
}


Candle makeCandle(
    const std::string& symbol,
    std::int64_t timestamp,
    double price
)
{
    Candle candle;

    candle.symbol = symbol;
    candle.timestamp = timestamp;

    candle.open  = price;
    candle.high  = price + 1.0;
    candle.low   = price - 1.0;
    candle.close = price + 0.5;

    candle.volume = 1000;

    return candle;
}

}


// =========================================================
// Main
// =========================================================

int main()
{
    using namespace devai::market;


    // -----------------------------------------------------
    // Artificial session start.
    //
    // Divisible by 15 minutes for clean testing.
    // -----------------------------------------------------

    constexpr std::int64_t start =
        1'800'000'000;

    static_assert(start % 900 == 0);


    // =====================================================
    // TEST 1
    //
    // Zero-latency historical synchronization
    // =====================================================

    {
        MultiTimeframeSynchronizerConfig config;

        config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer synchronizer(
            config
        );


        std::vector<Candle> one_minute;
        std::vector<Candle> five_minute;
        std::vector<Candle> fifteen_minute;


        // 15 × 1-minute candles

        for (int i = 0; i < 15; ++i)
        {
            one_minute.push_back(
                makeCandle(
                    "TEST",
                    start + i * 60,
                    100.0 + i
                )
            );
        }


        // Three 5-minute candles:
        //
        // start
        // start + 5m
        // start + 10m

        for (int i = 0; i < 3; ++i)
        {
            five_minute.push_back(
                makeCandle(
                    "TEST",
                    start + i * 300,
                    200.0 + i
                )
            );
        }


        // One 15-minute candle

        fifteen_minute.push_back(
            makeCandle(
                "TEST",
                start,
                300.0
            )
        );


        const auto timed_1m =
            synchronizer.prepareOneMinute(
                one_minute
            );

        const auto timed_5m =
            synchronizer.prepareFiveMinute(
                five_minute
            );

        const auto timed_15m =
            synchronizer.prepareFifteenMinute(
                fifteen_minute
            );


        // -------------------------------------------------
        // At +5 minutes:
        //
        // 1m latest = +4m
        // 5m latest = start
        // 15m       = unavailable
        // -------------------------------------------------

        const auto state_5 =
            synchronizer.stateAt(
                start + 300,
                timed_1m,
                timed_5m,
                timed_15m
            );


        require(
            state_5.one_minute.has_value(),
            "1m should be available at +5m."
        );

        require(
            state_5.one_minute->candle.timestamp ==
                start + 240,
            "Latest 1m at +5m should be +4m candle."
        );


        require(
            state_5.five_minute.has_value(),
            "5m should be available at +5m."
        );

        require(
            state_5.five_minute->candle.timestamp ==
                start,
            "Latest 5m at +5m should be first 5m candle."
        );


        require(
            !state_5.fifteen_minute.has_value(),
            "15m must NOT be available at +5m."
        );


        // -------------------------------------------------
        // At +15 minutes:
        //
        // 1m  = +14m
        // 5m  = +10m
        // 15m = start
        // -------------------------------------------------

        const auto state_15 =
            synchronizer.stateAt(
                start + 900,
                timed_1m,
                timed_5m,
                timed_15m
            );


        require(
            state_15.one_minute.has_value(),
            "1m missing at +15m."
        );

        require(
            state_15.one_minute->candle.timestamp ==
                start + 840,
            "Wrong 1m candle at +15m."
        );


        require(
            state_15.five_minute.has_value(),
            "5m missing at +15m."
        );

        require(
            state_15.five_minute->candle.timestamp ==
                start + 600,
            "Wrong 5m candle at +15m."
        );


        require(
            state_15.fifteen_minute.has_value(),
            "15m should become available at +15m."
        );

        require(
            state_15.fifteen_minute->candle.timestamp ==
                start,
            "Wrong 15m candle at +15m."
        );
    }


    // =====================================================
    // TEST 2
    //
    // Simulated 4-minute 5m latency
    // =====================================================

    {
        MultiTimeframeSynchronizerConfig config;

        config.mode =
            AvailabilityMode::SIMULATED_LATENCY;

        config.one_minute_latency_seconds = 0;

        config.five_minute_latency_seconds =
            4 * 60;

        config.fifteen_minute_latency_seconds = 0;


        MultiTimeframeSynchronizer synchronizer(
            config
        );


        std::vector<Candle> five_minute;

        five_minute.push_back(
            makeCandle(
                "TEST",
                start,
                200.0
            )
        );


        const auto timed_5m =
            synchronizer.prepareFiveMinute(
                five_minute
            );


        const std::vector<TimedCandle> empty;


        // Candle:
        //
        // starts      +0
        // completes   +5
        // latency     +4
        // available   +9


        const auto at_5 =
            synchronizer.stateAt(
                start + 300,
                empty,
                timed_5m,
                empty
            );


        require(
            !at_5.five_minute.has_value(),
            "Delayed 5m candle must not be "
            "available at +5m."
        );


        const auto at_8 =
            synchronizer.stateAt(
                start + 480,
                empty,
                timed_5m,
                empty
            );


        require(
            !at_8.five_minute.has_value(),
            "Delayed 5m candle must not be "
            "available at +8m."
        );


        const auto at_9 =
            synchronizer.stateAt(
                start + 540,
                empty,
                timed_5m,
                empty
            );


        require(
            at_9.five_minute.has_value(),
            "Delayed 5m candle should become "
            "available at +9m."
        );
    }


    // =====================================================
    // TEST 3
    //
    // LIVE actual receipt time
    // =====================================================

    {
        MultiTimeframeSynchronizerConfig config;

        config.mode =
            AvailabilityMode::LIVE_RECEIPT;


        MultiTimeframeSynchronizer synchronizer(
            config
        );


        const Candle candle =
            makeCandle(
                "TEST",
                start,
                500.0
            );


        // 5m candle theoretically completes +5m,
        // but actually arrives +9m 7s.

        const std::int64_t actual_received =
            start +
            9 * 60 +
            7;


        const TimedCandle live =
            synchronizer.makeLiveTimedCandle(
                candle,
                5 * 60,
                actual_received
            );


        require(
            live.completed_at ==
                start + 300,
            "Wrong live completion time."
        );


        require(
            live.received_at ==
                actual_received,
            "Wrong live received_at."
        );


        const std::vector<TimedCandle> live_5m{
            live
        };

        const std::vector<TimedCandle> empty;


        // One second before arrival

        const auto before =
            synchronizer.stateAt(
                actual_received - 1,
                empty,
                live_5m,
                empty
            );


        require(
            !before.five_minute.has_value(),
            "Live candle exposed before "
            "actual receipt time."
        );


        // Exact receipt time

        const auto at_arrival =
            synchronizer.stateAt(
                actual_received,
                empty,
                live_5m,
                empty
            );


        require(
            at_arrival.five_minute.has_value(),
            "Live candle not available at "
            "actual receipt time."
        );
    }


    // =====================================================
    // Final
    // =====================================================

    std::cout
        << "\n"
        << "========================================\n"
        << "MultiTimeframeSynchronizer Tests PASSED\n"
        << "========================================\n"
        << "Zero-latency synchronization : PASSED\n"
        << "Simulated 4-minute latency   : PASSED\n"
        << "Live receipt-time protection : PASSED\n"
        << "Look-ahead protection        : PASSED\n"
        << "========================================\n";


    return EXIT_SUCCESS;
}