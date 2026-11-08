#include "devai/strategy/dev_ai_strategy/BaselineDevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevAgentEpisodeRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include "devai/ranking/CrossSectionalMathematicalState.hpp"
#include "devai/ranking/CrossSectionalRank.hpp"
#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using namespace devai::strategy;
using namespace devai::ranking;


// ============================================================================
// Test statistics
// ============================================================================

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};


// ============================================================================
// require
// ============================================================================

void require(
    const bool condition,
    const std::string& description,
    TestStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << description
            << '\n';
    }
    else
    {
        ++stats.failures;

        std::cerr
            << "[FAIL] "
            << description
            << '\n';
    }
}


// ============================================================================
// approximatelyEqual
// ============================================================================

bool approximatelyEqual(
    const double lhs,
    const double rhs,
    const double tolerance = 1e-12)
{
    return
        std::fabs(lhs - rhs) <=
        tolerance;
}


// ============================================================================
// expectThrow
// ============================================================================

template <typename Function>
void expectThrow(
    Function&& function,
    const std::string& description,
    TestStats& stats)
{
    bool threw = false;

    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw = true;
    }

    require(
        threw,
        description,
        stats);
}


// ============================================================================
// makeRank
//
// Construct one valid Phase-5 cross-sectional rank.
//
// The actual raw metric value is not important for Phase 3.5.
// ObservationBuilder consumes the causal cross-sectional state that already
// exists at the current decision time.
// ============================================================================

CrossSectionalRank makeRank(
    const std::string& symbol,
    const std::int64_t decision_time,
    const double centered_percentile,
    const std::size_t universe_size = 2)
{
    CrossSectionalRank rank;

    rank.symbol =
        symbol;

    rank.decision_time =
        decision_time;

    // Give the synthetic ranking a finite underlying value.
    rank.value =
        centered_percentile;

    // Rank itself is finite and valid.
    //
    // The exact rank number is irrelevant to the Episode Runner.
    rank.rank =
        centered_percentile >= 0.0
            ? 1.0
            : 2.0;

    rank.universe_size =
        universe_size;

    // Convert centered percentile [-1,+1] to percentile [0,1].
    rank.percentile =
        (centered_percentile + 1.0) /
        2.0;

    rank.centered_percentile =
        centered_percentile;

    rank.available =
        true;

    return rank;
}


// ============================================================================
// populateHorizon
//
// Populate all 13 frozen Phase-5 mathematical ranking metrics.
// ============================================================================

void populateHorizon(
    CrossSectionalHorizonState& horizon,
    const std::string& symbol,
    const std::int64_t decision_time,
    const double base_value)
{
    auto& rankings =
        horizon.rankings;

    rankings.return_value =
        makeRank(
            symbol,
            decision_time,
            base_value);

    rankings.return_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.01);

    rankings.return_robust_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.02);

    rankings.volume_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.03);

    rankings.rvol20_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.04);

    rankings.relative_return =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.05);

    rankings.residual_return =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.06);

    rankings.residual_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.07);

    rankings.residual_robust_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.08);

    rankings.atr14_percent =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.09);

    rankings.realized_volatility_20 =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.10);

    rankings.rolling_beta =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.11);

    rankings.rolling_correlation =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.12);


    // ------------------------------------------------------------------------
    // Readiness
    // ------------------------------------------------------------------------

    auto& readiness =
        horizon.readiness;


    readiness.return_value =
        {true, 2};

    readiness.return_z_score =
        {true, 2};

    readiness.return_robust_z_score =
        {true, 2};

    readiness.volume_z_score =
        {true, 2};

    readiness.rvol20_z_score =
        {true, 2};

    readiness.relative_return =
        {true, 2};

    readiness.residual_return =
        {true, 2};

    readiness.residual_z_score =
        {true, 2};

    readiness.residual_robust_z_score =
        {true, 2};

    readiness.atr14_percent =
        {true, 2};

    readiness.realized_volatility_20 =
        {true, 2};

    readiness.rolling_beta =
        {true, 2};

    readiness.rolling_correlation =
        {true, 2};


    readiness.available_metric_count =
        13;

    readiness.total_metric_count =
        13;

    readiness.numerical_integrity =
        true;

    readiness.fully_ready =
        true;
}


// ============================================================================
// makeStockState
// ============================================================================

CrossSectionalMathematicalState makeStockState(
    const std::string& symbol,
    const std::int64_t decision_time,
    const double base_value)
{
    CrossSectionalMathematicalState state;

    state.symbol =
        symbol;

    state.benchmark_symbol =
        "NIFTY%2050";

    state.decision_time =
        decision_time;


    populateHorizon(
        state.one_minute,
        symbol,
        decision_time,
        base_value);

    populateHorizon(
        state.five_minute,
        symbol,
        decision_time,
        base_value + 0.10);

    populateHorizon(
        state.fifteen_minute,
        symbol,
        decision_time,
        base_value + 0.20);


    state.one_minute_ready =
        true;

    state.five_minute_ready =
        true;

    state.fifteen_minute_ready =
        true;

    state.numerical_integrity =
        true;

    state.fully_ready =
        true;


    return state;
}


// ============================================================================
// populateUniverseSizes
// ============================================================================

void populateUniverseSizes(
    CrossSectionalMetricUniverseSizes& sizes,
    const std::size_t universe_size)
{
    sizes.return_value =
        universe_size;

    sizes.return_z_score =
        universe_size;

    sizes.return_robust_z_score =
        universe_size;

    sizes.volume_z_score =
        universe_size;

    sizes.rvol20_z_score =
        universe_size;

    sizes.relative_return =
        universe_size;

    sizes.residual_return =
        universe_size;

    sizes.residual_z_score =
        universe_size;

    sizes.residual_robust_z_score =
        universe_size;

    sizes.atr14_percent =
        universe_size;

    sizes.realized_volatility_20 =
        universe_size;

    sizes.rolling_beta =
        universe_size;

    sizes.rolling_correlation =
        universe_size;
}


// ============================================================================
// makeSnapshot
//
// Two stocks are used because the frozen DevEpisode minimum universe defaults
// to two stocks.
//
// RELIANCE is the traded stock.
// HDFCBANK provides the second valid cross-sectional universe member.
// ============================================================================

CrossSectionalUniverseSnapshot makeSnapshot(
    const std::int64_t decision_time,
    const double state_shift)
{
    CrossSectionalUniverseSnapshot snapshot;

    snapshot.decision_time =
        decision_time;

    snapshot.benchmark_symbol =
        "NIFTY%2050";


    snapshot.stocks.push_back(
        makeStockState(
            "HDFCBANK",
            decision_time,
            -0.50 + state_shift));

    snapshot.stocks.push_back(
        makeStockState(
            "RELIANCE",
            decision_time,
            0.20 + state_shift));


    snapshot.stock_count =
        snapshot.stocks.size();

    snapshot.fully_ready_stock_count =
        snapshot.stock_count;

    snapshot.partially_ready_stock_count =
        0;


    populateUniverseSizes(
        snapshot.one_minute_universe_sizes,
        snapshot.stock_count);

    populateUniverseSizes(
        snapshot.five_minute_universe_sizes,
        snapshot.stock_count);

    populateUniverseSizes(
        snapshot.fifteen_minute_universe_sizes,
        snapshot.stock_count);


    snapshot.numerical_integrity =
        true;

    snapshot.fully_ready =
        true;


    return snapshot;
}


// ============================================================================
// makeEpisode
//
// Four states:
//
//     S0 = 09:15
//     S1 = 09:16
//     S2 = 09:17
//     S3 = 09:18
//
// Therefore exactly three complete RL transitions can exist:
//
//     S0 -> S1
//     S1 -> S2
//     S2 -> S3
// ============================================================================

DevEpisode makeEpisode()
{
    constexpr std::int64_t T0 =
        1'000'000;

    std::vector<CrossSectionalUniverseSnapshot>
        snapshots;


    snapshots.push_back(
        makeSnapshot(
            T0,
            0.00));

    snapshots.push_back(
        makeSnapshot(
            T0 + 60,
            0.01));

    snapshots.push_back(
        makeSnapshot(
            T0 + 120,
            0.02));

    snapshots.push_back(
        makeSnapshot(
            T0 + 180,
            0.03));


    DevEpisodeConfig config;

    config.minimum_universe_size =
        2;


    return DevEpisode(
        std::move(snapshots),
        config);
}


// ============================================================================
// Test 1
//
// Construction / reset / initial state
// ============================================================================

void testResetAndInitialState(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy);


    require(
        runner.symbol() == "RELIANCE",
        "Runner preserves traded symbol",
        stats);


    require(
        runner.transitionsProduced() == 0,
        "Runner initially reports zero transitions",
        stats);


    require(
        !runner.canStep(),
        "Runner cannot step before reset",
        stats);


    runner.reset(
        999'940,
        100.0);


    require(
        episode.isInitialized(),
        "Runner reset initializes DevEpisode",
        stats);


    require(
        episode.stepIndex() == 0,
        "Runner reset positions episode at first state",
        stats);


    require(
        policy.selectionCount() == 0,
        "Runner reset resets policy diagnostics",
        stats);


    require(
        runner.transitionsProduced() == 0,
        "Runner reset clears transition counter",
        stats);


    require(
        runner.canStep(),
        "Runner can step after valid reset",
        stats);


    require(
        environment.state().decision_time ==
            999'940,
        "TradingEnvironment reset time is preserved",
        stats);
}


// ============================================================================
// Test 2
//
// One complete causal transition
// ============================================================================

void testSingleTransition(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;


    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy);


    runner.reset(
        999'940,
        100.0);


    const auto result =
        runner.step(
            101.0);


    require(
        result.transition.decision_time ==
            1'000'000,
        "Transition begins at current episode decision time",
        stats);


    require(
        result.transition.next_decision_time ==
            1'000'060,
        "Transition advances to exact next decision time",
        stats);


    require(
        result.transition.next_decision_time >
            result.transition.decision_time,
        "Transition chronology is strictly forward",
        stats);


    require(
        result.transition.action ==
            DevAction::HOLD,
        "Baseline Dev agent selects HOLD",
        stats);


    require(
        result.trading_step.action ==
            DevAction::HOLD,
        "TradingEnvironment receives selected HOLD action",
        stats);


    require(
        approximatelyEqual(
            result.transition.reward,
            result.trading_step.reward.total),
        "Transition uses exact TradingEnvironment reward.total",
        stats);


    require(
        result.source_step_index == 0,
        "Transition source index is correct",
        stats);


    require(
        result.destination_step_index == 1,
        "Transition destination index is correct",
        stats);


    require(
        episode.stepIndex() == 1,
        "Episode advances exactly once",
        stats);


    require(
        policy.selectionCount() == 1,
        "Policy is called exactly once",
        stats);


    require(
        policy.lastDecisionTime() ==
            1'000'000,
        "Policy receives current causal decision time",
        stats);


    require(
        runner.transitionsProduced() == 1,
        "Runner records one produced transition",
        stats);


    require(
        !result.transition.terminal,
        "First transition is non-terminal",
        stats);


    require(
        !result.stored_in_replay,
        "Transition is not stored when no replay buffer is attached",
        stats);


    DevTransitionValidator validator;

    bool valid_transition =
        true;

    try
    {
        validator.validate(
            result.transition);
    }
    catch (const std::exception&)
    {
        valid_transition =
            false;
    }


    require(
        valid_transition,
        "Runner produces structurally valid DevTransition",
        stats);
}


// ============================================================================
// Test 3
//
// Replay-buffer integration
// ============================================================================

void testReplayBufferIntegration(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        16,
        12345U);


    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy,
        &replay_buffer);


    runner.reset(
        999'940,
        100.0);


    const auto result =
        runner.step(
            100.5);


    require(
        result.stored_in_replay,
        "Runner reports replay insertion",
        stats);


    require(
        replay_buffer.size() == 1,
        "Replay buffer receives one transition",
        stats);


    const auto& stored =
        replay_buffer.at(0);


    require(
        stored.decision_time ==
            result.transition.decision_time,
        "Replay buffer preserves transition decision time",
        stats);


    require(
        stored.next_decision_time ==
            result.transition.next_decision_time,
        "Replay buffer preserves next decision time",
        stats);


    require(
        stored.action ==
            result.transition.action,
        "Replay buffer preserves selected action",
        stats);


    require(
        approximatelyEqual(
            stored.reward,
            result.transition.reward),
        "Replay buffer preserves exact reward",
        stats);
}


// ============================================================================
// Test 4
//
// N states -> N - 1 complete transitions
// ============================================================================

void testCompleteEpisode(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        16,
        7U);


    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy,
        &replay_buffer);


    runner.reset(
        999'940,
        100.0);


    const std::vector<double>
        market_prices{
            101.0,
            102.0,
            103.0
        };


    std::vector<DevAgentEpisodeRunnerResult>
        results;


    for (const double market_price :
         market_prices)
    {
        require(
            runner.canStep(),
            "Runner reports another complete transition is available",
            stats);

        results.push_back(
            runner.step(
                market_price));
    }


    require(
        episode.totalSteps() == 4,
        "Synthetic episode contains four states",
        stats);


    require(
        results.size() == 3,
        "Four states produce exactly three complete transitions",
        stats);


    require(
        runner.transitionsProduced() == 3,
        "Runner reports exactly three produced transitions",
        stats);


    require(
        policy.selectionCount() == 3,
        "Policy is called once per complete transition",
        stats);


    require(
        replay_buffer.size() == 3,
        "Replay buffer contains all three transitions",
        stats);


    require(
        episode.stepIndex() == 3,
        "Episode finishes on final state",
        stats);


    require(
        episode.isTerminal(),
        "Episode reports terminal after reaching final state",
        stats);


    require(
        !runner.canStep(),
        "Runner cannot step beyond final episode state",
        stats);


    require(
        !results[0].transition.terminal,
        "First transition is non-terminal",
        stats);


    require(
        !results[1].transition.terminal,
        "Second transition is non-terminal",
        stats);


    require(
        results[2].transition.terminal,
        "Final complete transition is terminal",
        stats);


    require(
        results[2].transition.next_decision_time ==
            1'000'180,
        "Terminal transition points to genuine final historical state",
        stats);


    require(
        results[2].transition.next_decision_time >
            results[2].transition.decision_time,
        "Terminal transition preserves strict forward chronology",
        stats);


    require(
        results[2].destination_step_index == 3,
        "Terminal transition destination is final state",
        stats);
}


// ============================================================================
// Test 5
//
// Baseline HOLD produces no position exposure.
// ============================================================================

void testBaselineHoldBehavior(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;


    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy);


    runner.reset(
        999'940,
        100.0);


    const auto result =
        runner.step(
            105.0);


    require(
        approximatelyEqual(
            result.trading_step.previous_position,
            0.0),
        "Baseline starts from flat position",
        stats);


    require(
        approximatelyEqual(
            result.trading_step.target_position,
            0.0),
        "HOLD preserves flat target position",
        stats);


    require(
        approximatelyEqual(
            result.trading_step.turnover,
            0.0),
        "Flat HOLD produces zero turnover",
        stats);


    require(
        result.trading_step.executions.empty(),
        "Flat HOLD produces no execution records",
        stats);


    require(
        approximatelyEqual(
            result.trading_step.reward.total,
            result.transition.reward),
        "Runner does not independently recalculate reward",
        stats);
}


// ============================================================================
// Test 6
//
// Invalid use is rejected.
// ============================================================================

void testInvalidInputs(
    TestStats& stats)
{
    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;


        expectThrow(
            [&]()
            {
                DevAgentEpisodeRunner runner(
                    "",
                    episode,
                    environment,
                    policy);
            },
            "Empty traded symbol is rejected",
            stats);
    }


    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;

        DevAgentEpisodeRunner runner(
            "RELIANCE",
            episode,
            environment,
            policy);


        expectThrow(
            [&]()
            {
                (void)runner.step(
                    100.0);
            },
            "step() before reset is rejected",
            stats);
    }


    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;

        DevAgentEpisodeRunner runner(
            "RELIANCE",
            episode,
            environment,
            policy);


        expectThrow(
            [&]()
            {
                runner.reset(
                    999'940,
                    0.0);
            },
            "Zero reset market price is rejected",
            stats);


        expectThrow(
            [&]()
            {
                runner.reset(
                    999'940,
                    -100.0);
            },
            "Negative reset market price is rejected",
            stats);
    }


    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;

        DevAgentEpisodeRunner runner(
            "RELIANCE",
            episode,
            environment,
            policy);


        expectThrow(
            [&]()
            {
                runner.reset(
                    1'000'000,
                    100.0);
            },
            "Reset at first decision time is rejected",
            stats);


        expectThrow(
            [&]()
            {
                runner.reset(
                    1'000'060,
                    100.0);
            },
            "Reset after first decision time is rejected",
            stats);
    }


    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;

        DevAgentEpisodeRunner runner(
            "RELIANCE",
            episode,
            environment,
            policy);


        runner.reset(
            999'940,
            100.0);


        expectThrow(
            [&]()
            {
                (void)runner.step(
                    0.0);
            },
            "Zero step market price is rejected",
            stats);


        expectThrow(
            [&]()
            {
                (void)runner.step(
                    -1.0);
            },
            "Negative step market price is rejected",
            stats);
    }
}


// ============================================================================
// Test 7
//
// Reset isolation
// ============================================================================

void testResetIsolation(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        16,
        99U);


    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy,
        &replay_buffer);


    runner.reset(
        999'940,
        100.0);


    (void)runner.step(
        101.0);

    (void)runner.step(
        102.0);


    require(
        runner.transitionsProduced() == 2,
        "Runner accumulates transition count before reset",
        stats);


    require(
        policy.selectionCount() == 2,
        "Policy accumulates selection count before reset",
        stats);


    require(
        replay_buffer.size() == 2,
        "Replay buffer contains pre-reset experience",
        stats);


    runner.reset(
        999'940,
        100.0);


    require(
        runner.transitionsProduced() == 0,
        "Runner transition counter resets at episode boundary",
        stats);


    require(
        policy.selectionCount() == 0,
        "Policy diagnostics reset at episode boundary",
        stats);


    require(
        episode.stepIndex() == 0,
        "Episode returns to first state after reset",
        stats);


    require(
        runner.canStep(),
        "Runner can begin a new episode after reset",
        stats);


    // Important:
    //
    // Runner reset deliberately does NOT clear replay memory.
    //
    // Replay memory may contain experience from multiple historical episodes.
    require(
        replay_buffer.size() == 2,
        "Runner reset preserves replay-buffer experience",
        stats);
}


// ============================================================================
// Test 8
//
// Attempting to step beyond terminal state is rejected.
// ============================================================================

void testTerminalProtection(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;


    DevAgentEpisodeRunner runner(
        "RELIANCE",
        episode,
        environment,
        policy);


    runner.reset(
        999'940,
        100.0);


    (void)runner.step(
        101.0);

    (void)runner.step(
        102.0);

    const auto terminal_result =
        runner.step(
            103.0);


    require(
        terminal_result.transition.terminal,
        "Last available transition is terminal",
        stats);


    require(
        episode.isTerminal(),
        "Episode is terminal after final transition",
        stats);


    expectThrow(
        [&]()
        {
            (void)runner.step(
                104.0);
        },
        "Runner rejects step beyond terminal episode state",
        stats);
}


// ============================================================================
// Main
// ============================================================================

} // namespace


int main()
{
    TestStats stats;


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.5 — DEV AGENT EPISODE RUNNER TEST\n"
        << "============================================================\n";


    try
    {
        testResetAndInitialState(
            stats);

        testSingleTransition(
            stats);

        testReplayBufferIntegration(
            stats);

        testCompleteEpisode(
            stats);

        testBaselineHoldBehavior(
            stats);

        testInvalidInputs(
            stats);

        testResetIsolation(
            stats);

        testTerminalProtection(
            stats);
    }
    catch (const std::exception& exception)
    {
        ++stats.failures;

        std::cerr
            << "\n[FATAL] Unexpected exception: "
            << exception.what()
            << '\n';
    }
    catch (...)
    {
        ++stats.failures;

        std::cerr
            << "\n[FATAL] Unknown unexpected exception.\n";
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 3.5 TEST SUMMARY\n"
        << "============================================================\n"
        << "Checks   : "
        << stats.checks
        << '\n'
        << "Failures : "
        << stats.failures
        << '\n'
        << "============================================================\n";


    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.5 DEV AGENT EPISODE RUNNER: PASSED\n"
            << "============================================================\n";

        return 0;
    }


    std::cerr
        << "DEVAI PHASE 3.5 DEV AGENT EPISODE RUNNER: FAILED\n"
        << "============================================================\n";

    return 1;
}