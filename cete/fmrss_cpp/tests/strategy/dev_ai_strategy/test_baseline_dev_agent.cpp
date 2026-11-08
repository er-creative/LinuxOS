#include "devai/strategy/dev_ai_strategy/BaselineDevAgent.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>


using namespace devai::strategy;


namespace
{

// ============================================================================
// TestStats
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
    const std::string& message,
    TestStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << message
            << '\n';

        return;
    }

    ++stats.failures;

    std::cout
        << "[FAIL] "
        << message
        << '\n';
}


// ============================================================================
// throwsInvalidArgument
// ============================================================================

template <typename Function>
bool throwsInvalidArgument(
    Function&& function)
{
    try
    {
        function();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }

    return false;
}

} // namespace


// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.4 — BASELINE DEV AGENT VALIDATION\n"
        << "======================================================================\n";


    TestStats stats;


    // ========================================================================
    // 1. Construction
    // ========================================================================

    BaselineDevAgent agent;


    require(
        agent.name() ==
            std::string_view{"BaselineDevAgent"},
        "Baseline agent exposes stable policy name",
        stats);


    require(
        agent.selectionCount() == 0,
        "New baseline agent has zero action selections",
        stats);


    require(
        agent.lastDecisionTime() == 0,
        "New baseline agent has no previous decision time",
        stats);


    // ========================================================================
    // 2. Baseline policy action
    // ========================================================================

    DevAIObservation observation{};


    const DevAction first_action =
        agent.selectAction(
            observation,
            1'000);


    require(
        first_action ==
            DevAction::HOLD,
        "Baseline agent deterministically selects HOLD",
        stats);


    require(
        DevPolicyValidator::isValidAction(
            first_action),
        "Baseline action satisfies DevPolicy action contract",
        stats);


    require(
        agent.selectionCount() == 1,
        "Successful action selection increments diagnostic count",
        stats);


    require(
        agent.lastDecisionTime() == 1'000,
        "Baseline agent records current decision time",
        stats);


    // ========================================================================
    // 3. Determinism
    //
    // Same observation and valid time progression must continue producing
    // the same baseline action.
    // ========================================================================

    const DevAction second_action =
        agent.selectAction(
            observation,
            1'060);


    require(
        second_action ==
            DevAction::HOLD,
        "Repeated baseline decision remains HOLD",
        stats);


    require(
        first_action ==
            second_action,
        "Baseline policy is deterministic",
        stats);


    require(
        agent.selectionCount() == 2,
        "Repeated valid selection increments diagnostic count",
        stats);


    require(
        agent.lastDecisionTime() == 1'060,
        "Latest valid decision time is recorded",
        stats);


    // ========================================================================
    // 4. DevPolicy polymorphism
    //
    // Phase 3.5 must be able to consume the agent only as DevPolicy.
    // ========================================================================

    DevPolicy& policy =
        agent;


    const DevAction polymorphic_action =
        policy.selectAction(
            observation,
            1'120);


    require(
        polymorphic_action ==
            DevAction::HOLD,
        "Baseline agent operates through DevPolicy interface",
        stats);


    require(
        policy.name() ==
            std::string_view{"BaselineDevAgent"},
        "Baseline name is available through DevPolicy interface",
        stats);


    require(
        agent.selectionCount() == 3,
        "Polymorphic action selection updates agent state",
        stats);


    require(
        agent.lastDecisionTime() == 1'120,
        "Polymorphic selection receives current decision time",
        stats);


    // ========================================================================
    // 5. Const observation boundary
    // ========================================================================

    const DevAIObservation& const_observation =
        observation;


    const DevAction const_observation_action =
        policy.selectAction(
            const_observation,
            1'180);


    require(
        const_observation_action ==
            DevAction::HOLD,
        "Baseline accepts const DevAI observation",
        stats);


    require(
        agent.selectionCount() == 4,
        "Const-observation selection is recorded",
        stats);


    // ========================================================================
    // 6. Invalid zero decision time
    //
    // Invalid calls must not alter agent-local diagnostic state.
    // ========================================================================

    const std::size_t count_before_zero_time =
        agent.selectionCount();

    const std::int64_t time_before_zero_time =
        agent.lastDecisionTime();


    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const DevAction action =
                    agent.selectAction(
                        observation,
                        0);
            }),
        "Baseline agent rejects zero decision time",
        stats);


    require(
        agent.selectionCount() ==
            count_before_zero_time,
        "Rejected zero-time decision does not increment selection count",
        stats);


    require(
        agent.lastDecisionTime() ==
            time_before_zero_time,
        "Rejected zero-time decision does not modify last decision time",
        stats);


    // ========================================================================
    // 7. Invalid negative decision time
    // ========================================================================

    const std::size_t count_before_negative_time =
        agent.selectionCount();

    const std::int64_t time_before_negative_time =
        agent.lastDecisionTime();


    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const DevAction action =
                    agent.selectAction(
                        observation,
                        -100);
            }),
        "Baseline agent rejects negative decision time",
        stats);


    require(
        agent.selectionCount() ==
            count_before_negative_time,
        "Rejected negative-time decision does not increment selection count",
        stats);


    require(
        agent.lastDecisionTime() ==
            time_before_negative_time,
        "Rejected negative-time decision does not modify last decision time",
        stats);


    // ========================================================================
    // 8. Episode reset
    // ========================================================================

    agent.reset();


    require(
        agent.selectionCount() == 0,
        "Baseline reset clears action-selection count",
        stats);


    require(
        agent.lastDecisionTime() == 0,
        "Baseline reset clears last decision time",
        stats);


    require(
        agent.name() ==
            std::string_view{"BaselineDevAgent"},
        "Baseline reset does not alter policy identity",
        stats);


    // ========================================================================
    // 9. Reuse after reset
    // ========================================================================

    const DevAction post_reset_action =
        agent.selectAction(
            observation,
            2'000);


    require(
        post_reset_action ==
            DevAction::HOLD,
        "Baseline agent remains usable after reset",
        stats);


    require(
        agent.selectionCount() == 1,
        "Baseline selection count restarts after reset",
        stats);


    require(
        agent.lastDecisionTime() == 2'000,
        "Baseline records new episode decision time",
        stats);


    // ========================================================================
    // 10. Multiple independent agents
    // ========================================================================

    BaselineDevAgent first_agent;
    BaselineDevAgent second_agent;


    [[maybe_unused]]
    const DevAction first_independent_action =
        first_agent.selectAction(
            observation,
            3'000);


    require(
        first_agent.selectionCount() == 1,
        "First baseline instance maintains independent state",
        stats);


    require(
        second_agent.selectionCount() == 0,
        "Second baseline instance is unaffected by first instance",
        stats);


    [[maybe_unused]]
    const DevAction second_independent_action =
        second_agent.selectAction(
            observation,
            4'000);


    require(
        first_agent.selectionCount() == 1,
        "First baseline instance is unaffected by second instance",
        stats);


    require(
        second_agent.selectionCount() == 1,
        "Second baseline instance maintains its own state",
        stats);


    // ========================================================================
    // 11. No learning / no adaptation
    //
    // Repeated calls must not alter the action-selection rule.
    // ========================================================================

    bool always_hold = true;


    for (std::int64_t i = 0;
         i < 100;
         ++i)
    {
        const DevAction action =
            first_agent.selectAction(
                observation,
                5'000 + (i * 60));


        if (action != DevAction::HOLD)
        {
            always_hold = false;
            break;
        }
    }


    require(
        always_hold,
        "Baseline remains HOLD-only across repeated decisions",
        stats);


    require(
        first_agent.selectionCount() == 101,
        "Baseline records all repeated valid decisions",
        stats);


    // ========================================================================
    // Summary
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.4 VALIDATION SUMMARY\n"
        << "======================================================================\n"
        << "Total checks : "
        << stats.checks
        << '\n'
        << "Failures     : "
        << stats.failures
        << '\n'
        << "======================================================================\n";


    if (stats.failures != 0)
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 3.4 — BASELINE DEV AGENT VALIDATION: FAILED\n"
            << "============================================================\n";

        return 1;
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.4 — BASELINE DEV AGENT VALIDATION: PASSED\n"
        << "============================================================\n";


    return 0;
}