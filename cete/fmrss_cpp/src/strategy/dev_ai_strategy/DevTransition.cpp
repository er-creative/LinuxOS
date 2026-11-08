#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"

#include <cmath>
#include <stdexcept>


namespace devai::strategy
{

// ============================================================================
// finite
// ============================================================================

bool DevTransitionValidator::finite(
    const double value) noexcept
{
    return std::isfinite(value);
}


// ============================================================================
// isValidAction
// ============================================================================

bool DevTransitionValidator::isValidAction(
    const DevAction action) noexcept
{
    switch (action)
    {
        case DevAction::HOLD:
        case DevAction::LONG:
        case DevAction::SHORT:
        case DevAction::EXIT:
            return true;
    }

    return false;
}


// ============================================================================
// hasChronologicalIntegrity
//
// A transition must move strictly forward in decision time.
//
//     decision_time < next_decision_time
//
// We deliberately do NOT require a fixed difference such as 60 seconds.
//
// Why:
//
// DevAI may later operate with:
//
//   - different timeframes
//   - session boundaries
//   - genuine market-data gaps
//
// The important invariant is strict causal ordering.
// ============================================================================

bool DevTransitionValidator::hasChronologicalIntegrity(
    const DevTransition& transition) const noexcept
{
    if (transition.decision_time <= 0)
    {
        return false;
    }

    if (transition.next_decision_time <= 0)
    {
        return false;
    }

    if (transition.next_decision_time <=
        transition.decision_time)
    {
        return false;
    }

    return true;
}


// ============================================================================
// hasNumericalIntegrity
// ============================================================================

bool DevTransitionValidator::hasNumericalIntegrity(
    const DevTransition& transition) const noexcept
{
    if (!finite(transition.reward))
    {
        return false;
    }

    if (!isValidAction(
            transition.action))
    {
        return false;
    }

    if (!hasChronologicalIntegrity(
            transition))
    {
        return false;
    }

    return true;
}


// ============================================================================
// validate
// ============================================================================

void DevTransitionValidator::validate(
    const DevTransition& transition) const
{
    if (transition.decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevTransition decision_time must be positive.");
    }


    if (transition.next_decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevTransition next_decision_time must be positive.");
    }


    if (transition.next_decision_time <=
        transition.decision_time)
    {
        throw std::invalid_argument(
            "DevTransition must move strictly forward in decision time.");
    }


    if (!finite(
            transition.reward))
    {
        throw std::invalid_argument(
            "DevTransition reward must be finite.");
    }


    if (!isValidAction(
            transition.action))
    {
        throw std::invalid_argument(
            "DevTransition contains an invalid DevAction.");
    }
}

} // namespace devai::strategy