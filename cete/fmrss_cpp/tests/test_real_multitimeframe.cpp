#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace devai::market;


// =========================================================
// Timestamp Formatting
// =========================================================

std::string formatTimestamp(std::int64_t timestamp)
{
    const std::time_t raw_time =
        static_cast<std::time_t>(timestamp);

    std::tm local_tm{};

#if defined(_WIN32)
    localtime_s(&local_tm, &raw_time);
#else
    localtime_r(&raw_time, &local_tm);
#endif

    char buffer[32]{};

    std::strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &local_tm
    );

    return buffer;
}


// =========================================================
// Print Candle Information
// =========================================================

void printTimedCandle(
    const std::string& label,
    const std::optional<TimedCandle>& timed)
{
    std::cout
        << std::left
        << std::setw(6)
        << label
        << " : ";

    if (!timed.has_value())
    {
        std::cout << "NOT AVAILABLE\n";
        return;
    }

    const Candle& candle =
        timed->candle;

    std::cout
        << formatTimestamp(candle.timestamp)
        << "  O="
        << std::fixed
        << std::setprecision(2)
        << candle.open
        << " H="
        << candle.high
        << " L="
        << candle.low
        << " C="
        << candle.close
        << " V="
        << candle.volume
        << '\n';
}


// =========================================================
// Verify No Look-Ahead
// =========================================================

bool verifyNoLookAhead(
    const std::optional<TimedCandle>& timed,
    std::int64_t decision_time)
{
    if (!timed.has_value())
    {
        return true;
    }

    return
        timed->completed_at <= decision_time &&
        timed->received_at <= decision_time;
}


// =========================================================
// Verify Entire Historical Decision Stream
// =========================================================

struct VerificationResult
{
    std::size_t decision_points{0};

    std::size_t one_minute_violations{0};
    std::size_t five_minute_violations{0};
    std::size_t fifteen_minute_violations{0};

    [[nodiscard]]
    std::size_t totalViolations() const noexcept
    {
        return
            one_minute_violations +
            five_minute_violations +
            fifteen_minute_violations;
    }
};


VerificationResult verifyDecisionStream(
    const MultiTimeframeSynchronizer& synchronizer,
    const std::vector<TimedCandle>& timed_1m,
    const std::vector<TimedCandle>& timed_5m,
    const std::vector<TimedCandle>& timed_15m,
    const std::vector<Candle>& original_1m)
{
    VerificationResult result;

    // -----------------------------------------------------
    // Each historical 1-minute candle represents:
    //
    // timestamp -> timestamp + 60 seconds
    //
    // Therefore the decision point for that completed
    // candle is timestamp + 60.
    // -----------------------------------------------------

    for (const Candle& candle : original_1m)
    {
        const std::int64_t decision_time =
            candle.timestamp + 60;

        const MultiTimeframeState state =
            synchronizer.stateAt(
                decision_time,
                timed_1m,
                timed_5m,
                timed_15m
            );

        ++result.decision_points;

        if (!verifyNoLookAhead(
                state.one_minute,
                decision_time))
        {
            ++result.one_minute_violations;
        }

        if (!verifyNoLookAhead(
                state.five_minute,
                decision_time))
        {
            ++result.five_minute_violations;
        }

        if (!verifyNoLookAhead(
                state.fifteen_minute,
                decision_time))
        {
            ++result.fifteen_minute_violations;
        }
    }

    return result;
}


// =========================================================
// Find First Trading-Day Timestamp
// =========================================================

std::int64_t firstSessionStart(
    const std::vector<Candle>& candles)
{
    if (candles.empty())
    {
        throw std::runtime_error(
            "Cannot determine session start from empty data."
        );
    }

    return candles.front().timestamp;
}


// =========================================================
// Display State
// =========================================================

void displayState(
    const std::string& title,
    const MultiTimeframeSynchronizer& synchronizer,
    std::int64_t decision_time,
    const std::vector<TimedCandle>& timed_1m,
    const std::vector<TimedCandle>& timed_5m,
    const std::vector<TimedCandle>& timed_15m)
{
    const MultiTimeframeState state =
        synchronizer.stateAt(
            decision_time,
            timed_1m,
            timed_5m,
            timed_15m
        );

    std::cout
        << "\n"
        << "------------------------------------------------------------\n"
        << title
        << "\n"
        << "Decision time : "
        << formatTimestamp(decision_time)
        << "\n"
        << "------------------------------------------------------------\n";

    printTimedCandle(
        "1m",
        state.one_minute
    );

    printTimedCandle(
        "5m",
        state.five_minute
    );

    printTimedCandle(
        "15m",
        state.fifteen_minute
    );
}


// =========================================================
// Verify Expected Timestamp
// =========================================================

void requireTimestamp(
    const std::optional<TimedCandle>& timed,
    std::int64_t expected,
    const std::string& description)
{
    if (!timed.has_value())
    {
        throw std::runtime_error(
            description +
            ": expected candle but none was available."
        );
    }

    if (timed->candle.timestamp != expected)
    {
        throw std::runtime_error(
            description +
            ": wrong candle timestamp. Expected " +
            formatTimestamp(expected) +
            ", received " +
            formatTimestamp(
                timed->candle.timestamp
            )
        );
    }
}


// =========================================================
// Verify Expected Unavailable State
// =========================================================

void requireUnavailable(
    const std::optional<TimedCandle>& timed,
    const std::string& description)
{
    if (timed.has_value())
    {
        throw std::runtime_error(
            description +
            ": candle became available too early. "
            "Visible candle timestamp = " +
            formatTimestamp(
                timed->candle.timestamp
            )
        );
    }
}

} // anonymous namespace


// =========================================================
// Main
// =========================================================

int main(int argc, char* argv[])
{
    using namespace devai::market;

    try
    {
        // -------------------------------------------------
        // Usage:
        //
        // ./build/test_real_multitimeframe \
        // /home/hadoop/shareMarket_Data/minute/RELIANCE_1min.txt
        // -------------------------------------------------

        if (argc != 2)
        {
            std::cerr
                << "Usage:\n\n"
                << "  "
                << argv[0]
                << " <1-minute-market-data-file>\n\n";

            return 1;
        }


        const std::filesystem::path file_path =
            argv[1];


        if (!std::filesystem::exists(file_path))
        {
            throw std::runtime_error(
                "File does not exist: " +
                file_path.string()
            );
        }


        // =================================================
        // 1. Load Real 1-Minute Data
        // =================================================

        const std::filesystem::path data_folder =
            file_path.parent_path();

        const std::string filename =
            file_path.filename().string();


        MarketDataLoader loader(
            data_folder
        );


        const auto load_result =
            loader.loadFile(
                filename
            );


        const std::vector<Candle>& one_minute =
            load_result.candles;


        if (one_minute.empty())
        {
            throw std::runtime_error(
                "No valid 1-minute candles loaded."
            );
        }


        const std::string symbol =
            one_minute.front().symbol;


        std::cout
            << "\n"
            << "============================================================\n"
            << "REAL MULTI-TIMEFRAME SYNCHRONIZATION TEST\n"
            << "============================================================\n"
            << "File                     : "
            << file_path
            << '\n'
            << "Symbol                   : "
            << symbol
            << '\n'
            << "1-minute candles loaded  : "
            << one_minute.size()
            << '\n'
            << "Invalid rows             : "
            << load_result.invalid_rows
            << '\n'
            << "Duplicate rows           : "
            << load_result.duplicate_rows
            << '\n';


        // =================================================
        // 2. Aggregate Real 1m -> 5m + 15m
        // =================================================

        CandleAggregator aggregator;


        const auto five_result =
            aggregator.aggregate(
                one_minute,
                Timeframe::FIVE_MINUTES
            );


        const auto fifteen_result =
            aggregator.aggregate(
                one_minute,
                Timeframe::FIFTEEN_MINUTES
            );


        const std::vector<Candle>& five_minute =
            five_result.candles;

        const std::vector<Candle>& fifteen_minute =
            fifteen_result.candles;


        std::cout
            << "\n"
            << "5-minute candles          : "
            << five_minute.size()
            << '\n'
            << "5m incomplete buckets     : "
            << five_result.incomplete_buckets
            << '\n'
            << "15-minute candles         : "
            << fifteen_minute.size()
            << '\n'
            << "15m incomplete buckets    : "
            << fifteen_result.incomplete_buckets
            << '\n';


        if (five_minute.empty())
        {
            throw std::runtime_error(
                "No complete 5-minute candles generated."
            );
        }


        if (fifteen_minute.empty())
        {
            throw std::runtime_error(
                "No complete 15-minute candles generated."
            );
        }


        const std::int64_t session_start =
            firstSessionStart(
                one_minute
            );


        std::cout
            << "First session timestamp   : "
            << formatTimestamp(
                session_start
            )
            << '\n';


        // =================================================
        // 3. ZERO-LATENCY TEST
        // =================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "ZERO-LATENCY REAL-DATA TEST\n"
            << "============================================================\n";


        MultiTimeframeSynchronizerConfig zero_config;

        zero_config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer zero_sync(
            zero_config
        );


        const auto zero_1m =
            zero_sync.prepareOneMinute(
                one_minute
            );

        const auto zero_5m =
            zero_sync.prepareFiveMinute(
                five_minute
            );

        const auto zero_15m =
            zero_sync.prepareFifteenMinute(
                fifteen_minute
            );


        // -------------------------------------------------
        // At 09:16 equivalent:
        //
        // latest 1m  = 09:15
        // 5m         = unavailable
        // 15m        = unavailable
        // -------------------------------------------------

        {
            const auto state =
                zero_sync.stateAt(
                    session_start + 60,
                    zero_1m,
                    zero_5m,
                    zero_15m
                );


            requireTimestamp(
                state.one_minute,
                session_start,
                "Zero latency +1m"
            );


            requireUnavailable(
                state.five_minute,
                "Zero latency +1m 5m"
            );


            requireUnavailable(
                state.fifteen_minute,
                "Zero latency +1m 15m"
            );
        }


        // -------------------------------------------------
        // At 09:20 equivalent:
        //
        // latest 1m = 09:19
        // latest 5m = 09:15
        // 15m       = unavailable
        // -------------------------------------------------

        {
            const auto state =
                zero_sync.stateAt(
                    session_start + 300,
                    zero_1m,
                    zero_5m,
                    zero_15m
                );


            requireTimestamp(
                state.one_minute,
                session_start + 240,
                "Zero latency +5m 1m"
            );


            requireTimestamp(
                state.five_minute,
                session_start,
                "Zero latency +5m 5m"
            );


            requireUnavailable(
                state.fifteen_minute,
                "Zero latency +5m 15m"
            );
        }


        // -------------------------------------------------
        // At 09:25 equivalent:
        //
        // latest 1m = 09:24
        // latest 5m = 09:20
        // 15m       = unavailable
        // -------------------------------------------------

        {
            const auto state =
                zero_sync.stateAt(
                    session_start + 600,
                    zero_1m,
                    zero_5m,
                    zero_15m
                );


            requireTimestamp(
                state.one_minute,
                session_start + 540,
                "Zero latency +10m 1m"
            );


            requireTimestamp(
                state.five_minute,
                session_start + 300,
                "Zero latency +10m 5m"
            );


            requireUnavailable(
                state.fifteen_minute,
                "Zero latency +10m 15m"
            );
        }


        // -------------------------------------------------
        // At 09:30 equivalent:
        //
        // latest 1m  = 09:29
        // latest 5m  = 09:25
        // latest 15m = 09:15
        // -------------------------------------------------

        {
            const auto state =
                zero_sync.stateAt(
                    session_start + 900,
                    zero_1m,
                    zero_5m,
                    zero_15m
                );


            requireTimestamp(
                state.one_minute,
                session_start + 840,
                "Zero latency +15m 1m"
            );


            requireTimestamp(
                state.five_minute,
                session_start + 600,
                "Zero latency +15m 5m"
            );


            requireTimestamp(
                state.fifteen_minute,
                session_start,
                "Zero latency +15m 15m"
            );
        }


        // -------------------------------------------------
        // Display selected real states
        // -------------------------------------------------

        displayState(
            "ZERO LATENCY - +1 MINUTE",
            zero_sync,
            session_start + 60,
            zero_1m,
            zero_5m,
            zero_15m
        );


        displayState(
            "ZERO LATENCY - +5 MINUTES",
            zero_sync,
            session_start + 300,
            zero_1m,
            zero_5m,
            zero_15m
        );


        displayState(
            "ZERO LATENCY - +10 MINUTES",
            zero_sync,
            session_start + 600,
            zero_1m,
            zero_5m,
            zero_15m
        );


        displayState(
            "ZERO LATENCY - +15 MINUTES",
            zero_sync,
            session_start + 900,
            zero_1m,
            zero_5m,
            zero_15m
        );


        // =================================================
        // 4. Verify Entire Zero-Latency History
        // =================================================

        const VerificationResult zero_verification =
            verifyDecisionStream(
                zero_sync,
                zero_1m,
                zero_5m,
                zero_15m,
                one_minute
            );


        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "ZERO-LATENCY FULL-HISTORY VERIFICATION\n"
            << "------------------------------------------------------------\n"
            << "Decision points           : "
            << zero_verification.decision_points
            << '\n'
            << "1m look-ahead violations  : "
            << zero_verification.one_minute_violations
            << '\n'
            << "5m look-ahead violations  : "
            << zero_verification.five_minute_violations
            << '\n'
            << "15m look-ahead violations : "
            << zero_verification.fifteen_minute_violations
            << '\n';


        // =================================================
        // 5. SIMULATED-LATENCY TEST
        //
        // This is deliberately a TEST configuration.
        //
        // It does NOT mean these should become your
        // permanent production latency values.
        //
        // 1m  = 5 seconds
        // 5m  = 10 seconds
        // 15m = 15 seconds
        // =================================================

        MultiTimeframeSynchronizerConfig latency_config;

        latency_config.mode =
            AvailabilityMode::SIMULATED_LATENCY;

        latency_config.one_minute_latency_seconds =
            5;

        latency_config.five_minute_latency_seconds =
            10;

        latency_config.fifteen_minute_latency_seconds =
            15;


        MultiTimeframeSynchronizer latency_sync(
            latency_config
        );


        const auto latency_1m =
            latency_sync.prepareOneMinute(
                one_minute
            );

        const auto latency_5m =
            latency_sync.prepareFiveMinute(
                five_minute
            );

        const auto latency_15m =
            latency_sync.prepareFifteenMinute(
                fifteen_minute
            );


        // =================================================
        // 6. Check Exactly At 09:30 Equivalent
        //
        // Newly completed:
        //
        // 1m 09:29 -> not available until +5 sec
        // 5m 09:25 -> not available until +10 sec
        // 15m 09:15 -> not available until +15 sec
        //
        // Older candles may still be visible.
        // =================================================

        const std::int64_t boundary =
            session_start + 900;


        const auto at_boundary =
            latency_sync.stateAt(
                boundary,
                latency_1m,
                latency_5m,
                latency_15m
            );


        // Latest available 1m should still be 09:28.
        requireTimestamp(
            at_boundary.one_minute,
            session_start + 780,
            "Latency boundary 1m"
        );


        // Latest available 5m should still be 09:20.
        requireTimestamp(
            at_boundary.five_minute,
            session_start + 300,
            "Latency boundary 5m"
        );


        // First 15m candle has not yet arrived.
        requireUnavailable(
            at_boundary.fifteen_minute,
            "Latency boundary 15m"
        );


        displayState(
            "SIMULATED LATENCY - EXACT +15 MINUTE BOUNDARY",
            latency_sync,
            boundary,
            latency_1m,
            latency_5m,
            latency_15m
        );


        // =================================================
        // 7. +5 Seconds
        //
        // New 1m should now appear.
        // New 5m and 15m should still be delayed.
        // =================================================

        const auto plus_5 =
            latency_sync.stateAt(
                boundary + 5,
                latency_1m,
                latency_5m,
                latency_15m
            );


        requireTimestamp(
            plus_5.one_minute,
            session_start + 840,
            "Latency +5 seconds 1m"
        );


        requireTimestamp(
            plus_5.five_minute,
            session_start + 300,
            "Latency +5 seconds 5m"
        );


        requireUnavailable(
            plus_5.fifteen_minute,
            "Latency +5 seconds 15m"
        );


        // =================================================
        // 8. +10 Seconds
        //
        // New 5m should now appear.
        // 15m remains delayed.
        // =================================================

        const auto plus_10 =
            latency_sync.stateAt(
                boundary + 10,
                latency_1m,
                latency_5m,
                latency_15m
            );


        requireTimestamp(
            plus_10.one_minute,
            session_start + 840,
            "Latency +10 seconds 1m"
        );


        requireTimestamp(
            plus_10.five_minute,
            session_start + 600,
            "Latency +10 seconds 5m"
        );


        requireUnavailable(
            plus_10.fifteen_minute,
            "Latency +10 seconds 15m"
        );


        // =================================================
        // 9. +15 Seconds
        //
        // All three newly completed timeframes should now
        // be visible.
        // =================================================

        const auto plus_15 =
            latency_sync.stateAt(
                boundary + 15,
                latency_1m,
                latency_5m,
                latency_15m
            );


        requireTimestamp(
            plus_15.one_minute,
            session_start + 840,
            "Latency +15 seconds 1m"
        );


        requireTimestamp(
            plus_15.five_minute,
            session_start + 600,
            "Latency +15 seconds 5m"
        );


        requireTimestamp(
            plus_15.fifteen_minute,
            session_start,
            "Latency +15 seconds 15m"
        );


        displayState(
            "SIMULATED LATENCY - +5 SECONDS",
            latency_sync,
            boundary + 5,
            latency_1m,
            latency_5m,
            latency_15m
        );


        displayState(
            "SIMULATED LATENCY - +10 SECONDS",
            latency_sync,
            boundary + 10,
            latency_1m,
            latency_5m,
            latency_15m
        );


        displayState(
            "SIMULATED LATENCY - +15 SECONDS",
            latency_sync,
            boundary + 15,
            latency_1m,
            latency_5m,
            latency_15m
        );


        // =================================================
        // 10. Verify Entire History Under Simulated Latency
        // =================================================

        const VerificationResult latency_verification =
            verifyDecisionStream(
                latency_sync,
                latency_1m,
                latency_5m,
                latency_15m,
                one_minute
            );


        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "SIMULATED-LATENCY FULL-HISTORY VERIFICATION\n"
            << "------------------------------------------------------------\n"
            << "Decision points           : "
            << latency_verification.decision_points
            << '\n'
            << "1m look-ahead violations  : "
            << latency_verification.one_minute_violations
            << '\n'
            << "5m look-ahead violations  : "
            << latency_verification.five_minute_violations
            << '\n'
            << "15m look-ahead violations : "
            << latency_verification.fifteen_minute_violations
            << '\n';


        // =================================================
        // Final Result
        // =================================================

        const std::size_t total_violations =
            zero_verification.totalViolations() +
            latency_verification.totalViolations();


        std::cout
            << "\n"
            << "============================================================\n";


        if (total_violations == 0)
        {
            std::cout
                << "REAL MULTI-TIMEFRAME TEST PASSED\n"
                << "============================================================\n"
                << "Real 1m loading            : PASSED\n"
                << "Real 5m aggregation        : PASSED\n"
                << "Real 15m aggregation       : PASSED\n"
                << "Zero-latency sync          : PASSED\n"
                << "Simulated-latency sync     : PASSED\n"
                << "Look-ahead protection      : PASSED\n"
                << "============================================================\n";

            return 0;
        }


        std::cout
            << "REAL MULTI-TIMEFRAME TEST FAILED\n"
            << "Total look-ahead violations: "
            << total_violations
            << '\n'
            << "============================================================\n";


        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "\n[ERROR] "
            << error.what()
            << '\n';

        return 1;
    }
}