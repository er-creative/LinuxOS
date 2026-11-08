#include "devai/strategy/dev_ai_strategy/DevBellmanTargetEngine.hpp"

#include <cmath>
#include <stdexcept>

namespace devai::strategy
{

namespace
{

bool finiteValue(double value) noexcept
{
    return std::isfinite(value);
}

} // namespace

// ============================================================================
// Constructor
// ============================================================================

DevBellmanTargetEngine::DevBellmanTargetEngine(
    DevBellmanTargetConfig config)
    : config_(config)
{
    validateConfig(config_);
}

// ============================================================================
// Main transition-based Bellman calculation
// ============================================================================

DevBellmanTargetResult
DevBellmanTargetEngine::calculate(
    const DevTransition& transition,
    const DevQNetwork& target_network) const
{
    validateTransition(transition);

    // ------------------------------------------------------------------------
    // Terminal transition
    //
    // Bellman target:
    //
    //     y_t = R_t
    //
    // IMPORTANT:
    // Do not evaluate the target network for terminal transitions.
    // There is no future bootstrap contribution.
    // ------------------------------------------------------------------------

    if (transition.terminal)
    {
        return calculate(
            transition.reward,
            0.0,
            true,
            transition.decision_time,
            transition.next_decision_time);
    }

    // ------------------------------------------------------------------------
    // Non-terminal transition
    //
    //     y_t = R_t + gamma * max_a Q_target(S_{t+1}, a)
    // ------------------------------------------------------------------------

    const DevQValues next_q_values =
        target_network.predict(
            transition.next_observation);

    if (!next_q_values.numerical_integrity)
    {
        throw std::runtime_error(
            "DevBellmanTargetEngine: target-network inference "
            "failed numerical-integrity validation.");
    }

    const double next_max_q =
        next_q_values.maximumValue();

    if (!finiteValue(next_max_q))
    {
        throw std::runtime_error(
            "DevBellmanTargetEngine: target network produced "
            "non-finite maximum Q-value.");
    }

    return calculate(
        transition.reward,
        next_max_q,
        false,
        transition.decision_time,
        transition.next_decision_time);
}

// ============================================================================
// Scalar Bellman calculation
// ============================================================================

DevBellmanTargetResult
DevBellmanTargetEngine::calculate(
    double reward,
    double next_max_q,
    bool terminal,
    std::int64_t decision_time,
    std::int64_t next_decision_time) const
{
    validateChronology(
        decision_time,
        next_decision_time);

    if (!finiteValue(reward))
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: reward must be finite.");
    }

    // For terminal transitions, next_max_q is intentionally irrelevant.
    //
    // We still reject non-finite scalar input because accepting NaN/Inf into
    // this API would hide upstream numerical corruption.
    if (!finiteValue(next_max_q))
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: next maximum Q-value must be finite.");
    }

    DevBellmanTargetResult result{};

    result.reward = reward;
    result.discount_factor = config_.discount_factor;
    result.terminal = terminal;
    result.decision_time = decision_time;
    result.next_decision_time = next_decision_time;

    if (terminal)
    {
        result.next_max_q = 0.0;
        result.discounted_bootstrap = 0.0;
        result.target = reward;
    }
    else
    {
        result.next_max_q = next_max_q;

        result.discounted_bootstrap =
            config_.discount_factor *
            next_max_q;

        result.target =
            reward +
            result.discounted_bootstrap;
    }

    result.numerical_integrity =
        finiteValue(result.reward) &&
        finiteValue(result.discount_factor) &&
        finiteValue(result.next_max_q) &&
        finiteValue(result.discounted_bootstrap) &&
        finiteValue(result.target);

    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "DevBellmanTargetEngine: Bellman calculation "
            "produced a non-finite result.");
    }

    return result;
}

// ============================================================================
// Config
// ============================================================================

const DevBellmanTargetConfig&
DevBellmanTargetEngine::config() const noexcept
{
    return config_;
}

// ============================================================================
// Validation
// ============================================================================

void DevBellmanTargetEngine::validateConfig(
    const DevBellmanTargetConfig& config)
{
    if (!finiteValue(config.discount_factor))
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: discount factor must be finite.");
    }

    if (config.discount_factor < 0.0 ||
        config.discount_factor > 1.0)
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: discount factor must be "
            "within [0, 1].");
    }
}

void DevBellmanTargetEngine::validateTransition(
    const DevTransition& transition)
{
    if (!finiteValue(transition.reward))
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: transition reward must be finite.");
    }

    validateChronology(
        transition.decision_time,
        transition.next_decision_time);

    // Current observation is not mathematically needed for the Bellman target.
    //
    // That is intentional:
    //
    //     target =
    //         R_t
    //         + gamma * max Q_target(S_{t+1}, a)
    //
    // S_t is used later by the training/backpropagation stage to obtain
    // Q_online(S_t, A_t).
    //
    // For a non-terminal transition, S_{t+1} must be a valid network input.
    if (!transition.terminal)
    {
        if (!transition.next_observation.numerical_integrity)
        {
            throw std::invalid_argument(
                "DevBellmanTargetEngine: non-terminal next observation "
                "has invalid numerical integrity.");
        }

        if (!transition.next_observation.ready)
        {
            throw std::invalid_argument(
                "DevBellmanTargetEngine: non-terminal next observation "
                "is not ready.");
        }
    }
}

void DevBellmanTargetEngine::validateChronology(
    std::int64_t decision_time,
    std::int64_t next_decision_time)
{
    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: decision time must be positive.");
    }

    if (next_decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: next decision time must be positive.");
    }

    if (next_decision_time <= decision_time)
    {
        throw std::invalid_argument(
            "DevBellmanTargetEngine: next decision time must be "
            "strictly later than decision time.");
    }
}

} // namespace devai::strategy