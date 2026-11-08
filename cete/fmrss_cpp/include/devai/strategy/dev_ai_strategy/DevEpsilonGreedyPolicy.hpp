#pragma once

#include "devai/strategy/dev_ai_strategy/DevPolicy.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"

#include <cstdint>
#include <random>
#include <string_view>

namespace devai::strategy
{

struct DevEpsilonGreedyPolicyConfig
{
    double epsilon_start{1.0};
    double epsilon_end{0.05};
    std::uint64_t decay_steps{100000};
    std::uint32_t random_seed{12345U};
};

struct DevEpsilonGreedyDecision
{
    DevAction action{DevAction::HOLD};
    double epsilon{0.0};
    double random_draw{0.0};
    bool exploratory{false};
    bool greedy{false};
    std::uint64_t selection_index{0};
    std::int64_t decision_time{0};
    bool numerical_integrity{false};
};

class DevEpsilonGreedyPolicy final : public DevPolicy
{
public:
    explicit DevEpsilonGreedyPolicy(
        const DevQNetwork& online_network,
        DevEpsilonGreedyPolicyConfig config = {});

    DevAction selectAction(
        const DevAIObservation& observation,
        std::int64_t decision_time) override;

    [[nodiscard]]
    DevEpsilonGreedyDecision selectActionDetailed(
        const DevAIObservation& observation,
        std::int64_t decision_time);

    [[nodiscard]]
    std::string_view name() const noexcept override;

    void reset() noexcept override;

    [[nodiscard]]
    const DevEpsilonGreedyPolicyConfig& config() const noexcept;

    [[nodiscard]]
    double currentEpsilon() const noexcept;

    [[nodiscard]]
    std::uint64_t completedActionSelections() const noexcept;

private:
    static void validateConfig(
        const DevEpsilonGreedyPolicyConfig& config);

    static void validateDecisionTime(
        std::int64_t decision_time);

    [[nodiscard]]
    DevAction randomAction();

    [[nodiscard]]
    double epsilonForSelection(
        std::uint64_t completed_selections) const noexcept;

    const DevQNetwork& online_network_;
    DevEpsilonGreedyPolicyConfig config_;

    std::mt19937 random_engine_;
    std::uniform_real_distribution<double> unit_distribution_{0.0, 1.0};
    std::uniform_int_distribution<int> action_distribution_{0, 3};

    std::uint64_t completed_action_selections_{0};
};

} // namespace devai::strategy
