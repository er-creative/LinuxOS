#include "devai/strategy/dev_ai_strategy/DevEpsilonGreedyPolicy.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace devai::strategy
{

DevEpsilonGreedyPolicy::DevEpsilonGreedyPolicy(
    const DevQNetwork& online_network,
    DevEpsilonGreedyPolicyConfig config)
    : online_network_(online_network),
      config_(config),
      random_engine_(config_.random_seed)
{
    validateConfig(config_);
}

DevAction
DevEpsilonGreedyPolicy::selectAction(
    const DevAIObservation& observation,
    const std::int64_t decision_time)
{
    return selectActionDetailed(observation, decision_time).action;
}

DevEpsilonGreedyDecision
DevEpsilonGreedyPolicy::selectActionDetailed(
    const DevAIObservation& observation,
    const std::int64_t decision_time)
{
    validateDecisionTime(decision_time);

    if (!observation.numerical_integrity)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: observation failed numerical integrity.");

    if (!observation.ready)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: observation is not ready.");

    if (completed_action_selections_ ==
        std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error(
            "DevEpsilonGreedyPolicy: action-selection counter overflow.");

    const std::uint64_t selection_index =
        completed_action_selections_;

    const double epsilon =
        epsilonForSelection(selection_index);

    const double random_draw =
        unit_distribution_(random_engine_);

    DevEpsilonGreedyDecision result{};
    result.epsilon = epsilon;
    result.random_draw = random_draw;
    result.selection_index = selection_index;
    result.decision_time = decision_time;

    if (random_draw < epsilon)
    {
        result.action = randomAction();
        result.exploratory = true;
        result.greedy = false;
    }
    else
    {
        const DevQValues q_values =
            online_network_.predict(observation);

        if (!q_values.numerical_integrity)
            throw std::runtime_error(
                "DevEpsilonGreedyPolicy: Q prediction failed numerical integrity.");

        result.action = q_values.greedyAction();
        result.exploratory = false;
        result.greedy = true;
    }

    result.numerical_integrity =
        std::isfinite(result.epsilon) &&
        std::isfinite(result.random_draw) &&
        result.epsilon >= 0.0 &&
        result.epsilon <= 1.0 &&
        result.random_draw >= 0.0 &&
        result.random_draw < 1.0;

    if (!result.numerical_integrity)
        throw std::runtime_error(
            "DevEpsilonGreedyPolicy: decision failed numerical integrity.");

    ++completed_action_selections_;
    return result;
}

std::string_view
DevEpsilonGreedyPolicy::name() const noexcept
{
    return "DevEpsilonGreedyPolicy";
}

void
DevEpsilonGreedyPolicy::reset() noexcept
{
    // Episode reset intentionally does nothing.
    //
    // The epsilon schedule and RNG stream are global training state,
    // not episode-local state. Dynamic DevAgent assignments and episode
    // boundaries must therefore continue the same exploration lifecycle.
}

const DevEpsilonGreedyPolicyConfig&
DevEpsilonGreedyPolicy::config() const noexcept
{
    return config_;
}

double
DevEpsilonGreedyPolicy::currentEpsilon() const noexcept
{
    return epsilonForSelection(completed_action_selections_);
}

std::uint64_t
DevEpsilonGreedyPolicy::completedActionSelections() const noexcept
{
    return completed_action_selections_;
}

void
DevEpsilonGreedyPolicy::validateConfig(
    const DevEpsilonGreedyPolicyConfig& config)
{
    if (!std::isfinite(config.epsilon_start) ||
        config.epsilon_start < 0.0 ||
        config.epsilon_start > 1.0)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: epsilon_start must be in [0,1].");

    if (!std::isfinite(config.epsilon_end) ||
        config.epsilon_end < 0.0 ||
        config.epsilon_end > 1.0)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: epsilon_end must be in [0,1].");

    if (config.epsilon_end > config.epsilon_start)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: epsilon_end must not exceed epsilon_start.");

    if (config.decay_steps == 0)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: decay_steps must be > 0.");
}

void
DevEpsilonGreedyPolicy::validateDecisionTime(
    const std::int64_t decision_time)
{
    if (decision_time <= 0)
        throw std::invalid_argument(
            "DevEpsilonGreedyPolicy: decision_time must be positive.");
}

DevAction
DevEpsilonGreedyPolicy::randomAction()
{
    switch (action_distribution_(random_engine_))
    {
        case 0: return DevAction::HOLD;
        case 1: return DevAction::LONG;
        case 2: return DevAction::SHORT;
        case 3: return DevAction::EXIT;
        default:
            throw std::runtime_error(
                "DevEpsilonGreedyPolicy: random action index out of range.");
    }
}

double
DevEpsilonGreedyPolicy::epsilonForSelection(
    const std::uint64_t completed_selections) const noexcept
{
    if (completed_selections >= config_.decay_steps)
        return config_.epsilon_end;

    const double progress =
        static_cast<double>(completed_selections) /
        static_cast<double>(config_.decay_steps);

    const double epsilon =
        config_.epsilon_start +
        progress * (config_.epsilon_end - config_.epsilon_start);

    return std::clamp(
        epsilon,
        config_.epsilon_end,
        config_.epsilon_start);
}

} // namespace devai::strategy
