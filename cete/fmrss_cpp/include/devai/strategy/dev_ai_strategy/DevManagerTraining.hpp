#pragma once

#include "devai/strategy/dev_ai_strategy/DevManager.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <string_view>
#include <vector>

namespace devai::strategy
{

inline constexpr std::size_t DEV_MANAGER_INPUT_SIZE = 62;
inline constexpr std::size_t DEV_MANAGER_HIDDEN1_SIZE = 32;
inline constexpr std::size_t DEV_MANAGER_HIDDEN2_SIZE = 16;
inline constexpr std::size_t DEV_MANAGER_ACTION_COUNT = 7;

// Fixed Manager action space:
// 0 NO_TRADE, 1 KEEP_CURRENT, 2..6 SELECT_AGENT slot 0..4.
enum class DevManagerAction : std::size_t
{
    NO_TRADE = 0,
    KEEP_CURRENT = 1,
    SELECT_AGENT_001 = 2,
    SELECT_AGENT_002 = 3,
    SELECT_AGENT_003 = 4,
    SELECT_AGENT_004 = 5,
    SELECT_AGENT_005 = 6
};

[[nodiscard]] constexpr bool isValidManagerAction(DevManagerAction action) noexcept
{
    return static_cast<std::size_t>(action) < DEV_MANAGER_ACTION_COUNT;
}

struct DevManagerEncodedState
{
    std::array<double, DEV_MANAGER_INPUT_SIZE> values{};
    std::array<bool, DEV_MANAGER_ACTION_COUNT> valid_actions{};
    std::int64_t decision_time{0};
    bool numerical_integrity{false};
};

class DevManagerStateEncoder
{
public:
    [[nodiscard]] DevManagerEncodedState encode(const DevManagerState& state) const;
};

struct DevManagerQValues
{
    std::array<double, DEV_MANAGER_ACTION_COUNT> values{};
    bool numerical_integrity{false};

    [[nodiscard]] double value(DevManagerAction action) const;
    [[nodiscard]] DevManagerAction greedyAction(
        const std::array<bool, DEV_MANAGER_ACTION_COUNT>& valid_actions) const;
    [[nodiscard]] double maximumValue(
        const std::array<bool, DEV_MANAGER_ACTION_COUNT>& valid_actions) const;
};

struct DevManagerNetworkConfig
{
    std::uint32_t random_seed{73117U};
    bool zero_initialize{false};
};

struct DevManagerNetworkParameters
{
    std::vector<double> weights1; // [H1][INPUT]
    std::vector<double> biases1;
    std::vector<double> weights2; // [H2][H1]
    std::vector<double> biases2;
    std::vector<double> weights3; // [ACTION][H2]
    std::vector<double> biases3;
};

struct DevManagerTrainingConfig
{
    std::size_t replay_capacity{4096};
    std::size_t minimum_replay_size{32};
    std::size_t batch_size{32};
    double gamma{0.99};
    double learning_rate{0.001};
    double huber_delta{1.0};
    double gradient_clip{10.0};
    std::size_t target_sync_interval{100};

    double epsilon_start{1.0};
    double epsilon_end{0.05};
    std::uint64_t epsilon_decay_selections{10000};
    std::uint32_t random_seed{73118U};
};

struct DevManagerTransition
{
    DevManagerState state{};
    DevManagerAction action{DevManagerAction::NO_TRADE};

    // MUST be a causal CETE economic reward produced after executing the
    // Manager decision. Never substitute ForwardReturn_N here.
    double reward{0.0};

    DevManagerState next_state{};
    bool terminal{false};
    std::int64_t decision_time{0};
    std::int64_t next_decision_time{0};
    bool numerical_integrity{false};
};

struct DevManagerTrainingStepResult
{
    std::uint64_t completed_training_updates{0};
    std::size_t batch_size{0};
    double mean_bellman_target{0.0};
    double mean_loss{0.0};
    double mean_td_error{0.0};
    double gradient_l2_norm{0.0};
    double clipped_gradient_l2_norm{0.0};
    bool target_synchronized{false};
    bool numerical_integrity{false};
};

struct DevManagerPolicyDecision
{
    DevManagerAction action{DevManagerAction::NO_TRADE};
    DevManagerDecision manager_decision{};
    DevManagerQValues q_values{};
    double epsilon{0.0};
    bool exploratory{false};
    bool numerical_integrity{false};
};

class DevManagerQNetwork
{
public:
    explicit DevManagerQNetwork(DevManagerNetworkConfig config = {});

    [[nodiscard]] DevManagerQValues predict(const DevManagerEncodedState& state) const;
    [[nodiscard]] std::size_t parameterCount() const noexcept;
    [[nodiscard]] DevManagerNetworkParameters parameters() const;
    void setParameters(const DevManagerNetworkParameters& parameters);
    void copyParametersFrom(const DevManagerQNetwork& other);

private:
    friend class DevManagerTrainer;
    DevManagerNetworkConfig config_{};
    DevManagerNetworkParameters parameters_{};
};

class DevManagerTrainer
{
public:
    explicit DevManagerTrainer(DevManagerTrainingConfig config = {});

    void addExperience(const DevManagerTransition& transition);
    [[nodiscard]] bool canTrain() const noexcept;
    [[nodiscard]] DevManagerTrainingStepResult trainOneUpdate();

    [[nodiscard]] DevManagerPolicyDecision selectDecision(
        const DevManagerState& state,
        const DevManager& manager);

    // Episode reset intentionally preserves global epsilon/RNG/training state.
    void resetEpisode() noexcept;

    // Deliberate new training lifecycle. Learned online parameters are kept.
    void clearExperienceAndResetTraining();

    [[nodiscard]] const DevManagerQNetwork& onlineNetwork() const noexcept;
    [[nodiscard]] const DevManagerQNetwork& targetNetwork() const noexcept;
    [[nodiscard]] std::size_t replaySize() const noexcept;
    [[nodiscard]] std::uint64_t completedTrainingUpdates() const noexcept;
    [[nodiscard]] std::uint64_t completedActionSelections() const noexcept;
    [[nodiscard]] double currentEpsilon() const noexcept;
    [[nodiscard]] const DevManagerTrainingConfig& config() const noexcept;

    static void validateTransition(const DevManagerTransition& transition);

private:
    [[nodiscard]] static DevManagerDecision materializeDecision(
        DevManagerAction action,
        const DevManagerState& state,
        const DevManager& manager);

    [[nodiscard]] std::vector<std::size_t> validActionIndices(
        const DevManagerEncodedState& state) const;

    [[nodiscard]] double epsilonForSelection(std::uint64_t selection) const noexcept;

    DevManagerTrainingConfig config_{};
    DevManagerStateEncoder encoder_{};
    DevManagerQNetwork online_network_{};
    DevManagerQNetwork target_network_{};
    std::deque<DevManagerTransition> replay_{};
    std::uint64_t completed_training_updates_{0};
    std::uint64_t completed_action_selections_{0};
    std::mt19937 random_engine_{};
    std::uniform_real_distribution<double> unit_distribution_{0.0, 1.0};
};

} // namespace devai::strategy
