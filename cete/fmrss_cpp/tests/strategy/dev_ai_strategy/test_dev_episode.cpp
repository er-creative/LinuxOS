#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"

namespace
{

using devai::strategy::DevEpisode;
using devai::strategy::DevEpisodeConfig;

using Snapshot =
    devai::ranking::CrossSectionalUniverseSnapshot;


// ============================================================================
// Test helpers
// ============================================================================

void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}


Snapshot makeSnapshot(
    std::int64_t decision_time,
    const std::string& benchmark)
{
    Snapshot snapshot;

    snapshot.decision_time =
        decision_time;

    snapshot.benchmark_symbol =
        benchmark;

    snapshot.stock_count = 2;

    snapshot.numerical_integrity =
        true;


    // We intentionally need real stock-state objects because DevEpisode
    // validates stock_count against stocks.size().
    //
    // Default construct the exact stock type already used by the snapshot.

    using StockState =
        typename decltype(snapshot.stocks)::value_type;


    StockState stock_a;
    stock_a.symbol = "RELIANCE";
    stock_a.decision_time =
        decision_time;


    StockState stock_b;
    stock_b.symbol = "SBIN";
    stock_b.decision_time =
        decision_time;


    snapshot.stocks.push_back(
        std::move(stock_a));

    snapshot.stocks.push_back(
        std::move(stock_b));


    return snapshot;
}


template<typename Function>
bool throwsException(Function&& function)
{
    try
    {
        function();
    }
    catch (const std::exception&)
    {
        return true;
    }

    return false;
}

} // namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "==========================================================================\n"
        << "DEVAI PHASE 2.2 — DEV EPISODE DEFINITION VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        constexpr std::int64_t T0 =
            1000000;

        constexpr std::int64_t T1 =
            T0 + 60;

        constexpr std::int64_t T2 =
            T1 + 60;


        std::vector<Snapshot>
            snapshots;

        snapshots.push_back(
            makeSnapshot(
                T0,
                "NIFTY%2050"));

        snapshots.push_back(
            makeSnapshot(
                T1,
                "NIFTY%2050"));

        snapshots.push_back(
            makeSnapshot(
                T2,
                "NIFTY%2050"));


        DevEpisodeConfig config;

        config.minimum_universe_size = 2;


        DevEpisode episode(
            snapshots,
            config);


        // ====================================================================
        // 2.2.1 Initialization
        // ====================================================================

        require(
            episode.isInitialized(),
            "Episode was not initialized.");

        require(
            episode.totalSteps() == 3,
            "Incorrect total episode steps.");

        std::cout
            << "[PASS] Episode initialization\n";


        // ====================================================================
        // 2.2.2 Episode boundaries
        // ====================================================================

        require(
            episode.startDecisionTime() == T0,
            "Incorrect episode start time.");

        require(
            episode.endDecisionTime() == T2,
            "Incorrect episode end time.");

        require(
            episode.benchmarkSymbol() ==
                "NIFTY%2050",
            "Incorrect benchmark symbol.");

        std::cout
            << "[PASS] Episode boundary definition\n";


        // ====================================================================
        // 2.2.3 Initial current state
        // ====================================================================

        require(
            episode.stepIndex() == 0,
            "Initial step index must be zero.");

        require(
            episode.currentDecisionTime() ==
                T0,
            "Incorrect initial decision time.");

        require(
            !episode.isTerminal(),
            "Episode incorrectly terminal at first step.");

        std::cout
            << "[PASS] Initial episode state\n";


        // ====================================================================
        // 2.2.4 First chronological advancement
        // ====================================================================

        episode.advance();


        require(
            episode.stepIndex() == 1,
            "Incorrect step index after first advance.");

        require(
            episode.currentDecisionTime() ==
                T1,
            "Incorrect decision time after first advance.");

        require(
            !episode.isTerminal(),
            "Episode terminal too early.");

        std::cout
            << "[PASS] Chronological progression\n";


        // ====================================================================
        // 2.2.5 Terminal state
        // ====================================================================

        episode.advance();


        require(
            episode.stepIndex() == 2,
            "Incorrect terminal step index.");

        require(
            episode.currentDecisionTime() ==
                T2,
            "Incorrect terminal decision time.");

        require(
            episode.isTerminal(),
            "Episode failed to enter terminal state.");

        std::cout
            << "[PASS] Terminal-state detection\n";


        // ====================================================================
        // 2.2.6 No advancement beyond terminal
        // ====================================================================

        require(
            throwsException(
                [&episode]()
                {
                    episode.advance();
                }),
            "Episode allowed advancement beyond terminal state.");

        std::cout
            << "[PASS] Terminal advancement protection\n";


        // ====================================================================
        // 2.2.7 Reset
        // ====================================================================

        episode.reset();


        require(
            episode.stepIndex() == 0,
            "Reset did not restore step zero.");

        require(
            episode.currentDecisionTime() ==
                T0,
            "Reset did not restore initial decision time.");

        require(
            !episode.isTerminal(),
            "Reset episode incorrectly terminal.");

        std::cout
            << "[PASS] Episode reset\n";


        // ====================================================================
        // 2.2.8 Deterministic progression
        // ====================================================================

        const auto first_time =
            episode.currentDecisionTime();

        episode.advance();

        const auto second_time =
            episode.currentDecisionTime();

        episode.reset();


        require(
            episode.currentDecisionTime() ==
                first_time,
            "Reset changed first decision time.");

        episode.advance();


        require(
            episode.currentDecisionTime() ==
                second_time,
            "Episode progression is not deterministic.");

        std::cout
            << "[PASS] Deterministic episode progression\n";


        // ====================================================================
        // 2.2.9 Duplicate/non-increasing time rejection
        // ====================================================================

        std::vector<Snapshot>
            duplicate_time;

        duplicate_time.push_back(
            makeSnapshot(
                T0,
                "NIFTY%2050"));

        duplicate_time.push_back(
            makeSnapshot(
                T0,
                "NIFTY%2050"));


        require(
            throwsException(
                [&duplicate_time]()
                {
                    DevEpisode invalid(
                        duplicate_time);
                }),
            "Duplicate decision time was accepted.");

        std::cout
            << "[PASS] Duplicate-time rejection\n";


        // ====================================================================
        // 2.2.10 Backward-time rejection
        // ====================================================================

        std::vector<Snapshot>
            backward_time;

        backward_time.push_back(
            makeSnapshot(
                T1,
                "NIFTY%2050"));

        backward_time.push_back(
            makeSnapshot(
                T0,
                "NIFTY%2050"));


        require(
            throwsException(
                [&backward_time]()
                {
                    DevEpisode invalid(
                        backward_time);
                }),
            "Backward decision time was accepted.");

        std::cout
            << "[PASS] Backward-time rejection\n";


        // ====================================================================
        // 2.2.11 Benchmark-change rejection
        // ====================================================================

        std::vector<Snapshot>
            benchmark_change;

        benchmark_change.push_back(
            makeSnapshot(
                T0,
                "NIFTY%2050"));

        benchmark_change.push_back(
            makeSnapshot(
                T1,
                "OTHER_BENCHMARK"));


        require(
            throwsException(
                [&benchmark_change]()
                {
                    DevEpisode invalid(
                        benchmark_change);
                }),
            "Benchmark change was accepted.");

        std::cout
            << "[PASS] Benchmark-identity protection\n";


        // ====================================================================
        // 2.2.12 Numerical-integrity rejection
        // ====================================================================

        std::vector<Snapshot>
            invalid_integrity;

        auto invalid_snapshot =
            makeSnapshot(
                T0,
                "NIFTY%2050");

        invalid_snapshot.numerical_integrity =
            false;

        invalid_integrity.push_back(
            std::move(invalid_snapshot));


        require(
            throwsException(
                [&invalid_integrity]()
                {
                    DevEpisode invalid(
                        invalid_integrity);
                }),
            "Invalid numerical snapshot was accepted.");

        std::cout
            << "[PASS] Numerical-integrity protection\n";


        // ====================================================================
        // 2.2.13 Empty episode rejection
        // ====================================================================

        require(
            throwsException(
                []()
                {
                    std::vector<Snapshot>
                        empty;

                    DevEpisode invalid(
                        std::move(empty));
                }),
            "Empty episode was accepted.");

        std::cout
            << "[PASS] Empty-episode rejection\n";


        // ====================================================================
        // Final state summary
        // ====================================================================

        episode.reset();

        const auto state =
            episode.state();


        require(
            state.initialized,
            "State summary not initialized.");

        require(
            state.step_index == 0,
            "State summary step index incorrect.");

        require(
            state.total_steps == 3,
            "State summary total steps incorrect.");

        require(
            state.start_decision_time == T0,
            "State summary start time incorrect.");

        require(
            state.end_decision_time == T2,
            "State summary end time incorrect.");

        require(
            state.current_decision_time == T0,
            "State summary current time incorrect.");


        std::cout
            << "[PASS] Episode-state generation\n";


        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Episode steps             : "
            << episode.totalSteps()
            << "\n"
            << "Start decision time       : "
            << episode.startDecisionTime()
            << "\n"
            << "End decision time         : "
            << episode.endDecisionTime()
            << "\n"
            << "Benchmark                 : "
            << episode.benchmarkSymbol()
            << "\n"
            << "Chronological progression : VERIFIED\n"
            << "Future snapshot exposure  : NONE THROUGH CURRENT-STATE API\n"
            << "Terminal protection       : VERIFIED\n"
            << "Reset isolation           : VERIFIED\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.2 DEV EPISODE DEFINITION VALIDATION PASSED\n"
            << "==========================================================================\n";


        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n[FAIL] "
            << exception.what()
            << "\n";

        return 1;
    }
}