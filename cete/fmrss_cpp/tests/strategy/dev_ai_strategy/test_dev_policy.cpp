#include "devai/strategy/dev_ai_strategy/DevPolicy.hpp"

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


// ============================================================================
// TestPolicy
//
// Minimal concrete policy used ONLY to validate the Phase 3.3 interface.
//
// This is NOT the Phase 3.4 baseline trading policy.
// ============================================================================

class TestPolicy final : public DevPolicy
{
public:

    explicit TestPolicy(
        const DevAction action)
        :
        action_(action)
    {
    }


    [[nodiscard]]
    DevAction selectAction(
        const DevAIObservation& observation,
        const std::int64_t decision_time) override
    {
        // Phase 3.3 test policy deliberately does not interpret the
        // observation. We only prove that the interface can receive it.
        (void)observation;

        DevPolicyValidator::validateDecisionTime(
            decision_time);

        DevPolicyValidator::validateAction(
            action_);

        ++selection_count_;

        last_decision_time_ =
            decision_time;

        return action_;
    }


    [[nodiscard]]
    std::string_view name() const noexcept override
    {
        return "TestPolicy";
    }


    void reset() noexcept override
    {
        selection_count_ = 0;
        last_decision_time_ = 0;
        ++reset_count_;
    }


    [[nodiscard]]
    std::size_t selectionCount() const noexcept
    {
        return selection_count_;
    }


    [[nodiscard]]
    std::size_t resetCount() const noexcept
    {
        return reset_count_;
    }


    [[nodiscard]]
    std::int64_t lastDecisionTime() const noexcept
    {
        return last_decision_time_;
    }


private:

    DevAction action_{DevAction::HOLD};

    std::size_t selection_count_{0};
    std::size_t reset_count_{0};

    std::int64_t last_decision_time_{0};
};


// ============================================================================
// StatelessPolicy
//
// Verifies that DevPolicy's default reset() implementation is usable.
// ============================================================================

class StatelessPolicy final : public DevPolicy
{
public:

    [[nodiscard]]
    DevAction selectAction(
        const DevAIObservation& observation,
        const std::int64_t decision_time) override
    {
        (void)observation;

        DevPolicyValidator::validateDecisionTime(
            decision_time);

        return DevAction::HOLD;
    }


    [[nodiscard]]
    std::string_view name() const noexcept override
    {
        return "StatelessPolicy";
    }
};

} // namespace


// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.3 — DEV POLICY INTERFACE VALIDATION\n"
        << "======================================================================\n";


    TestStats stats;


    // ========================================================================
    // 1. Legal action validation
    // ========================================================================

    require(
        DevPolicyValidator::isValidAction(
            DevAction::HOLD),
        "HOLD is a valid policy action",
        stats);


    require(
        DevPolicyValidator::isValidAction(
            DevAction::LONG),
        "LONG is a valid policy action",
        stats);


    require(
        DevPolicyValidator::isValidAction(
            DevAction::SHORT),
        "SHORT is a valid policy action",
        stats);


    require(
        DevPolicyValidator::isValidAction(
            DevAction::EXIT),
        "EXIT is a valid policy action",
        stats);


    // ========================================================================
    // 2. Invalid action validation
    // ========================================================================

    const auto invalid_action =
        static_cast<DevAction>(999);


    require(
        !DevPolicyValidator::isValidAction(
            invalid_action),
        "Malformed action is not considered valid",
        stats);


    require(
        throwsInvalidArgument(
            [&]()
            {
                DevPolicyValidator::validateAction(
                    invalid_action);
            }),
        "Malformed policy action is rejected",
        stats);


    // ========================================================================
    // 3. Valid action validator does not throw
    // ========================================================================

    bool legal_actions_accepted = true;

    try
    {
        DevPolicyValidator::validateAction(
            DevAction::HOLD);

        DevPolicyValidator::validateAction(
            DevAction::LONG);

        DevPolicyValidator::validateAction(
            DevAction::SHORT);

        DevPolicyValidator::validateAction(
            DevAction::EXIT);
    }
    catch (...)
    {
        legal_actions_accepted = false;
    }


    require(
        legal_actions_accepted,
        "All four legal Dev actions pass policy validation",
        stats);


    // ========================================================================
    // 4. Decision-time validation
    // ========================================================================

    bool positive_time_accepted = true;

    try
    {
        DevPolicyValidator::validateDecisionTime(
            1);
    }
    catch (...)
    {
        positive_time_accepted = false;
    }


    require(
        positive_time_accepted,
        "Positive decision time is accepted",
        stats);


    require(
        throwsInvalidArgument(
            []()
            {
                DevPolicyValidator::validateDecisionTime(
                    0);
            }),
        "Zero decision time is rejected",
        stats);


    require(
        throwsInvalidArgument(
            []()
            {
                DevPolicyValidator::validateDecisionTime(
                    -1);
            }),
        "Negative decision time is rejected",
        stats);


    // ========================================================================
    // 5. Interface polymorphism
    //
    // The episode runner will later hold a DevPolicy reference/pointer rather
    // than depending on one concrete implementation.
    // ========================================================================

    DevAIObservation observation{};

    TestPolicy concrete_policy(
        DevAction::LONG);

    DevPolicy& policy =
        concrete_policy;


    require(
        policy.name() ==
            std::string_view{"TestPolicy"},
        "Concrete policy exposes its name through DevPolicy interface",
        stats);


    const DevAction selected_action =
        policy.selectAction(
            observation,
            1'000);


    require(
        selected_action ==
            DevAction::LONG,
        "Policy interface returns selected DevAction",
        stats);


    require(
        concrete_policy.selectionCount() ==
            1,
        "Concrete policy records one action selection",
        stats);


    require(
        concrete_policy.lastDecisionTime() ==
            1'000,
        "Policy receives current decision time",
        stats);


    // ========================================================================
    // 6. Observation can cross the interface without mutation
    //
    // Phase 3.3 does not need to know the internal 84-field layout.
    // const reference enforces the policy-side read-only contract.
    // ========================================================================

    const DevAIObservation& const_observation =
        observation;


    const DevAction second_action =
        policy.selectAction(
            const_observation,
            1'060);


    require(
        second_action ==
            DevAction::LONG,
        "Const DevAI observation crosses policy boundary successfully",
        stats);


    require(
        concrete_policy.selectionCount() ==
            2,
        "Repeated policy selections are supported",
        stats);


    require(
        concrete_policy.lastDecisionTime() ==
            1'060,
        "Latest causal decision time reaches policy implementation",
        stats);


    // ========================================================================
    // 7. Policy reset
    // ========================================================================

    policy.reset();


    require(
        concrete_policy.selectionCount() ==
            0,
        "Policy reset clears episode-local selection state",
        stats);


    require(
        concrete_policy.lastDecisionTime() ==
            0,
        "Policy reset clears episode-local decision time",
        stats);


    require(
        concrete_policy.resetCount() ==
            1,
        "Policy reset hook is invoked through interface",
        stats);


    // ========================================================================
    // 8. Policy can select again after reset
    // ========================================================================

    const DevAction post_reset_action =
        policy.selectAction(
            observation,
            2'000);


    require(
        post_reset_action ==
            DevAction::LONG,
        "Policy remains usable after reset",
        stats);


    require(
        concrete_policy.selectionCount() ==
            1,
        "Policy state restarts correctly after reset",
        stats);


    // ========================================================================
    // 9. Invalid decision time propagates through policy boundary
    // ========================================================================

    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const DevAction action =
                    policy.selectAction(
                        observation,
                        0);
            }),
        "Policy rejects zero decision time",
        stats);


    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const DevAction action =
                    policy.selectAction(
                        observation,
                        -100);
            }),
        "Policy rejects negative decision time",
        stats);


    // ========================================================================
    // 10. Invalid action produced by concrete policy is rejected
    // ========================================================================

    TestPolicy malformed_policy(
        invalid_action);


    require(
        throwsInvalidArgument(
            [&]()
            {
                [[maybe_unused]]
                const DevAction action =
                    malformed_policy.selectAction(
                        observation,
                        3'000);
            }),
        "Invalid action produced by policy implementation is rejected",
        stats);


    require(
        malformed_policy.selectionCount() ==
            0,
        "Rejected policy action does not count as successful selection",
        stats);


    // ========================================================================
    // 11. Different concrete policies can share the same interface
    // ========================================================================

    TestPolicy hold_policy(
        DevAction::HOLD);

    TestPolicy long_policy(
        DevAction::LONG);

    TestPolicy short_policy(
        DevAction::SHORT);

    TestPolicy exit_policy(
        DevAction::EXIT);


    DevPolicy* policies[] =
    {
        &hold_policy,
        &long_policy,
        &short_policy,
        &exit_policy
    };


    const DevAction expected_actions[] =
    {
        DevAction::HOLD,
        DevAction::LONG,
        DevAction::SHORT,
        DevAction::EXIT
    };


    bool polymorphic_actions_correct = true;


    for (std::size_t i = 0;
         i < 4;
         ++i)
    {
        const DevAction action =
            policies[i]->selectAction(
                observation,
                4'000 +
                    static_cast<std::int64_t>(i * 60));


        if (action != expected_actions[i])
        {
            polymorphic_actions_correct = false;
            break;
        }
    }


    require(
        polymorphic_actions_correct,
        "Multiple concrete policies operate through one DevPolicy contract",
        stats);


    // ========================================================================
    // 12. Default reset implementation
    // ========================================================================

    StatelessPolicy stateless_policy;

    DevPolicy& stateless_interface =
        stateless_policy;


    stateless_interface.reset();


    require(
        stateless_interface.name() ==
            std::string_view{"StatelessPolicy"},
        "Stateless policy supports default no-op reset",
        stats);


    require(
        stateless_interface.selectAction(
            observation,
            5'000) ==
            DevAction::HOLD,
        "Stateless policy remains usable after default reset",
        stats);


    // ========================================================================
    // Summary
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.3 VALIDATION SUMMARY\n"
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
            << "DEVAI PHASE 3.3 — DEV POLICY INTERFACE VALIDATION: FAILED\n"
            << "============================================================\n";

        return 1;
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.3 — DEV POLICY INTERFACE VALIDATION: PASSED\n"
        << "============================================================\n";


    return 0;
}