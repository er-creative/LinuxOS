#include "devai/strategy/dev_ai_strategy/DevPolicy.hpp"

#include <stdexcept>


namespace devai::strategy
{

// ============================================================================
// isValidAction
// ============================================================================

bool DevPolicyValidator::isValidAction(
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
// validateAction
// ============================================================================

void DevPolicyValidator::validateAction(
    const DevAction action)
{
    if (!isValidAction(action))
    {
        throw std::invalid_argument(
            "DevPolicy produced an invalid DevAction.");
    }
}


// ============================================================================
// validateDecisionTime
// ============================================================================

void DevPolicyValidator::validateDecisionTime(
    const std::int64_t decision_time)
{
    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevPolicy decision time must be greater than zero.");
    }
}

} // namespace devai::strategy