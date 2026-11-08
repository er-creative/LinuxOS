#include "devai/strategy/dev_ai_strategy/TerminalConditionEngine.hpp"

#include <cmath>
#include <stdexcept>

namespace devai::strategy
{

// ============================================================================
// Constructor
// ============================================================================

TerminalConditionEngine::TerminalConditionEngine(
    TerminalConditionConfig config)
    : config_(config)
{
    validateConfig(config_);
}


// ============================================================================
// finite
// ============================================================================

bool
TerminalConditionEngine::finite(
    double value) noexcept
{
    return std::isfinite(value);
}


// ============================================================================
// validateConfig
// ============================================================================

void
TerminalConditionEngine::validateConfig(
    const TerminalConditionConfig& config)
{
    if (!finite(config.maximum_drawdown) ||
        config.maximum_drawdown < 0.0 ||
        config.maximum_drawdown >= 1.0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine maximum_drawdown "
            "must be finite and in [0, 1).");
    }

    if (!finite(config.minimum_equity_fraction) ||
        config.minimum_equity_fraction < 0.0 ||
        config.minimum_equity_fraction > 1.0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine minimum_equity_fraction "
            "must be finite and in [0, 1].");
    }
}


// ============================================================================
// validateInput
// ============================================================================

void
TerminalConditionEngine::validateInput(
    const TerminalConditionInput& input)
{
    if (input.decision_time <= 0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine decision_time "
            "must be positive.");
    }

    if (!finite(input.initial_equity) ||
        input.initial_equity <= 0.0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine initial_equity "
            "must be positive and finite.");
    }

    if (!finite(input.current_equity) ||
        input.current_equity <= 0.0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine current_equity "
            "must be positive and finite.");
    }

    if (!finite(input.current_drawdown) ||
        input.current_drawdown < 0.0 ||
        input.current_drawdown >= 1.0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine current_drawdown "
            "must be finite and in [0, 1).");
    }

    if (!finite(input.maximum_drawdown) ||
        input.maximum_drawdown < 0.0 ||
        input.maximum_drawdown >= 1.0)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine maximum_drawdown "
            "must be finite and in [0, 1).");
    }

    if (input.maximum_drawdown <
        input.current_drawdown)
    {
        throw std::invalid_argument(
            "TerminalConditionEngine maximum_drawdown "
            "cannot be below current_drawdown.");
    }
}


// ============================================================================
// makeResult
// ============================================================================

TerminalConditionResult
TerminalConditionEngine::makeResult(
    const TerminalConditionInput& input,
    bool terminal,
    TerminalReason reason)
{
    TerminalConditionResult result;

    result.terminal =
        terminal;

    result.reason =
        reason;

    result.decision_time =
        input.decision_time;

    result.completed_steps =
        input.completed_steps;

    result.current_equity =
        input.current_equity;

    result.current_drawdown =
        input.current_drawdown;

    result.numerical_integrity =
        true;

    return result;
}


// ============================================================================
// evaluate
// ============================================================================

TerminalConditionResult
TerminalConditionEngine::evaluate(
    const TerminalConditionInput& input) const
{
    validateInput(input);


    // ========================================================================
    // 1. End of historical episode data
    // ========================================================================

    if (!input.has_next_state)
    {
        return makeResult(
            input,
            true,
            TerminalReason::END_OF_DATA);
    }


    // ========================================================================
    // 2. Maximum drawdown policy
    //
    // A configured value of 0 disables this condition.
    // ========================================================================

    if (config_.maximum_drawdown > 0.0 &&
        input.current_drawdown >=
            config_.maximum_drawdown)
    {
        return makeResult(
            input,
            true,
            TerminalReason::MAXIMUM_DRAWDOWN);
    }


    // ========================================================================
    // 3. Minimum-equity policy
    //
    // A configured value of 0 disables this condition.
    // ========================================================================

    if (config_.minimum_equity_fraction > 0.0)
    {
        const double minimum_equity =
            input.initial_equity *
            config_.minimum_equity_fraction;

        if (!finite(minimum_equity) ||
            minimum_equity <= 0.0)
        {
            throw std::runtime_error(
                "TerminalConditionEngine generated invalid "
                "minimum equity.");
        }

        if (input.current_equity <=
            minimum_equity)
        {
            return makeResult(
                input,
                true,
                TerminalReason::MINIMUM_EQUITY);
        }
    }


    // ========================================================================
    // 4. Maximum episode steps
    //
    // A configured value of 0 disables this condition.
    // ========================================================================

    if (config_.maximum_episode_steps > 0 &&
        input.completed_steps >=
            config_.maximum_episode_steps)
    {
        return makeResult(
            input,
            true,
            TerminalReason::MAXIMUM_STEPS);
    }


    // ========================================================================
    // Continue episode
    // ========================================================================

    return makeResult(
        input,
        false,
        TerminalReason::NONE);
}


// ============================================================================
// config
// ============================================================================

const TerminalConditionConfig&
TerminalConditionEngine::config() const noexcept
{
    return config_;
}


// ============================================================================
// reasonName
// ============================================================================

const char*
TerminalConditionEngine::reasonName(
    TerminalReason reason) noexcept
{
    switch (reason)
    {
        case TerminalReason::NONE:
            return "NONE";

        case TerminalReason::END_OF_DATA:
            return "END_OF_DATA";

        case TerminalReason::MAXIMUM_STEPS:
            return "MAXIMUM_STEPS";

        case TerminalReason::MAXIMUM_DRAWDOWN:
            return "MAXIMUM_DRAWDOWN";

        case TerminalReason::MINIMUM_EQUITY:
            return "MINIMUM_EQUITY";
    }

    return "UNKNOWN";
}

} // namespace devai::strategy