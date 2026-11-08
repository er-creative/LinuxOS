#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>


using namespace devai::strategy;


namespace
{

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
// makeValidTransition
// ============================================================================

DevTransition makeValidTransition()
{
    DevTransition transition;

    transition.action =
        DevAction::LONG;

    transition.reward =
        0.00125;

    transition.terminal =
        false;

    transition.decision_time =
        1'000;

    transition.next_decision_time =
        1'060;

    return transition;
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
        << "DEVAI PHASE 3.1 — DEV TRANSITION VALIDATION\n"
        << "======================================================================\n";


    TestStats stats;

    DevTransitionValidator validator;


    // ========================================================================
    // 1. Default construction
    // ========================================================================

    {
        const DevTransition transition;

        require(
            transition.action ==
                DevAction::HOLD,
            "Default action is HOLD",
            stats);

        require(
            transition.reward ==
                0.0,
            "Default reward is zero",
            stats);

        require(
            !transition.terminal,
            "Default transition is non-terminal",
            stats);

        require(
            transition.decision_time ==
                0,
            "Default decision time is zero",
            stats);

        require(
            transition.next_decision_time ==
                0,
            "Default next decision time is zero",
            stats);
    }


    // ========================================================================
    // 2. Valid transition
    // ========================================================================

    {
        const DevTransition transition =
            makeValidTransition();

        require(
            validator.hasNumericalIntegrity(
                transition),
            "Valid transition has numerical integrity",
            stats);

        require(
            validator.hasChronologicalIntegrity(
                transition),
            "Valid transition has chronological integrity",
            stats);

        bool accepted = true;

        try
        {
            validator.validate(
                transition);
        }
        catch (...)
        {
            accepted = false;
        }

        require(
            accepted,
            "Validator accepts valid transition",
            stats);
    }


    // ========================================================================
    // 3. Legal Dev actions
    // ========================================================================

    require(
        DevTransitionValidator::isValidAction(
            DevAction::HOLD),
        "HOLD is a valid transition action",
        stats);

    require(
        DevTransitionValidator::isValidAction(
            DevAction::LONG),
        "LONG is a valid transition action",
        stats);

    require(
        DevTransitionValidator::isValidAction(
            DevAction::SHORT),
        "SHORT is a valid transition action",
        stats);

    require(
        DevTransitionValidator::isValidAction(
            DevAction::EXIT),
        "EXIT is a valid transition action",
        stats);


    // ========================================================================
    // 4. Invalid action
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.action =
            static_cast<DevAction>(999);

        require(
            !validator.hasNumericalIntegrity(
                transition),
            "Invalid action fails numerical integrity",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "Invalid action is rejected",
            stats);
    }


    // ========================================================================
    // 5. Non-finite reward
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.reward =
            std::numeric_limits<double>::quiet_NaN();

        require(
            !validator.hasNumericalIntegrity(
                transition),
            "NaN reward fails numerical integrity",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "NaN reward is rejected",
            stats);
    }


    {
        DevTransition transition =
            makeValidTransition();

        transition.reward =
            std::numeric_limits<double>::infinity();

        require(
            !validator.hasNumericalIntegrity(
                transition),
            "Infinite reward fails numerical integrity",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "Infinite reward is rejected",
            stats);
    }


    // ========================================================================
    // 6. Invalid decision time
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.decision_time =
            0;

        require(
            !validator.hasChronologicalIntegrity(
                transition),
            "Zero decision time fails chronology",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "Zero decision time is rejected",
            stats);
    }


    // ========================================================================
    // 7. Invalid next decision time
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.next_decision_time =
            0;

        require(
            !validator.hasChronologicalIntegrity(
                transition),
            "Zero next decision time fails chronology",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "Zero next decision time is rejected",
            stats);
    }


    // ========================================================================
    // 8. Same-time transition
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.next_decision_time =
            transition.decision_time;

        require(
            !validator.hasChronologicalIntegrity(
                transition),
            "Same-time transition fails chronology",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "Same-time transition is rejected",
            stats);
    }


    // ========================================================================
    // 9. Backward-time transition
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.next_decision_time =
            transition.decision_time - 60;

        require(
            !validator.hasChronologicalIntegrity(
                transition),
            "Backward transition fails chronology",
            stats);

        require(
            throwsInvalidArgument(
                [&]()
                {
                    validator.validate(
                        transition);
                }),
            "Backward transition is rejected",
            stats);
    }


    // ========================================================================
    // 10. Genuine time gaps are allowed
    //
    // CETE must not require synthetic candles merely to create a transition.
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.next_decision_time =
            transition.decision_time + 600;

        require(
            validator.hasChronologicalIntegrity(
                transition),
            "Strictly forward genuine time gap is allowed",
            stats);

        bool accepted = true;

        try
        {
            validator.validate(
                transition);
        }
        catch (...)
        {
            accepted = false;
        }

        require(
            accepted,
            "Validator accepts strictly forward time gap",
            stats);
    }


    // ========================================================================
    // 11. Terminal transition
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.terminal =
            true;

        require(
            validator.hasNumericalIntegrity(
                transition),
            "Terminal transition remains structurally valid",
            stats);

        bool accepted = true;

        try
        {
            validator.validate(
                transition);
        }
        catch (...)
        {
            accepted = false;
        }

        require(
            accepted,
            "Validator accepts valid terminal transition",
            stats);
    }


    // ========================================================================
    // 12. Negative reward is legal
    //
    // Losing experiences are legitimate RL transitions.
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.reward =
            -0.025;

        require(
            validator.hasNumericalIntegrity(
                transition),
            "Finite negative reward is valid",
            stats);
    }


    // ========================================================================
    // 13. Zero reward is legal
    // ========================================================================

    {
        DevTransition transition =
            makeValidTransition();

        transition.reward =
            0.0;

        require(
            validator.hasNumericalIntegrity(
                transition),
            "Zero reward is valid",
            stats);
    }


    // ========================================================================
    // Summary
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.1 VALIDATION SUMMARY\n"
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
            << "DEVAI PHASE 3.1 — DEV TRANSITION VALIDATION: FAILED\n"
            << "============================================================\n";

        return 1;
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.1 — DEV TRANSITION VALIDATION: PASSED\n"
        << "============================================================\n";


    return 0;
}