#pragma once

#include "devai/strategy/dev_ai_strategy/DevEpsilonGreedyPolicy.hpp"
#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"
#include "devai/strategy/dev_ai_strategy/DevTrainingEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace devai::strategy
{

// ============================================================================
// DEVAI PHASE 3.16 — SHARED DEVAGENT TRAINING
//
// One stock-level Q-network is shared by every dynamic DevAgent slot.
// DevAgent_001 ... DevAgent_005 do NOT own independent models.
//
// This class owns training infrastructure only. It deliberately does NOT:
//   - construct rewards from ForwardReturn_N
//   - construct counterfactual rewards
//   - calculate CETE brokerage/slippage/P&L/risk
//   - perform DevManager selection/allocation
//   - alter the frozen 84-value observation contract
//   - put future outcomes into an observation
//
// The input is an already-causal, already-rewarded DevTransition. Therefore
// CETE/episode code remains the authority for economic reward construction.
// ============================================================================

struct DevSharedAgentTrainerConfig
{
    std::size_t replay_capacity{100000};
    std::size_t minimum_replay_size{32};
    std::size_t batch_size{32};
    std::uint32_t replay_seed{31601U};

    DevQNetworkConfig online_network{};
    DevTrainingEngineConfig training{};
    DevEpsilonGreedyPolicyConfig exploration{};
};

struct DevSharedAgentTrainingResult
{
    DevTrainingStepResult training{};
    std::size_t replay_size{0};
    std::size_t batch_size{0};
    std::uint64_t completed_training_updates{0};
    bool numerical_integrity{false};
};

class DevSharedAgentTrainer
{
public:
    explicit DevSharedAgentTrainer(
        DevSharedAgentTrainerConfig config = {});

    // Store one causal experience from any dynamically assigned DevAgent.
    void observe(const DevTransition& transition);

    void observe(const std::vector<DevTransition>& transitions);

    [[nodiscard]]
    bool canTrain() const noexcept;

    // Samples one replay mini-batch and performs exactly one frozen
    // DevTrainingEngine update.
    [[nodiscard]]
    DevSharedAgentTrainingResult trainOneUpdate();

    // Shared epsilon-greedy action selection. The supplied observation may
    // belong to any of the five dynamic DevAgent slots.
    [[nodiscard]]
    DevEpsilonGreedyDecision selectAction(
        const DevAIObservation& observation,
        std::int64_t decision_time);

    // Episode reset must NOT reset the global epsilon schedule.
    void resetEpisode() noexcept;

    // Clears replay experience and resets training counters/target network.
    // Network parameters are intentionally preserved.
    void clearExperienceAndResetTraining();

    [[nodiscard]]
    DevModelPersistenceMetadata saveModel(
        const std::filesystem::path& path) const;

    [[nodiscard]]
    const DevSharedAgentTrainerConfig& config() const noexcept;

    [[nodiscard]]
    const DevQNetwork& onlineNetwork() const noexcept;

    [[nodiscard]]
    const DevQNetwork& targetNetwork() const noexcept;

    [[nodiscard]]
    const DevReplayBuffer& replayBuffer() const noexcept;

    [[nodiscard]]
    std::uint64_t completedTrainingUpdates() const noexcept;

    [[nodiscard]]
    std::uint64_t completedActionSelections() const noexcept;

private:
    static void validateConfig(const DevSharedAgentTrainerConfig& config);

    DevSharedAgentTrainerConfig config_{};
    DevQNetwork online_network_;
    DevQNetwork target_network_;
    DevTrainingEngine training_engine_;
    DevReplayBuffer replay_buffer_;
    DevEpsilonGreedyPolicy exploration_policy_;
    DevModelPersistence persistence_{};
};

} // namespace devai::strategy
