#pragma once

#include "devai/strategy/dev_ai_strategy/DevMiniBatchTrainer.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevTargetNetworkSynchronizer.hpp"
#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace devai::strategy
{

struct DevTrainingEngineConfig
{
    DevMiniBatchTrainerConfig mini_batch{};
    DevTargetNetworkSynchronizerConfig target_synchronization{};
};

struct DevTrainingStepResult
{
    std::uint64_t completed_training_updates{0};
    std::size_t batch_size{0};

    double mean_bellman_target{0.0};
    double mean_loss{0.0};
    double mean_td_error{0.0};
    double mean_absolute_td_error{0.0};
    double gradient_l2_norm{0.0};
    double clipped_gradient_l2_norm{0.0};

    bool target_synchronized{false};
    std::size_t target_updates_since_synchronization{0};
    bool numerical_integrity{false};
};

class DevTrainingEngine
{
public:
    explicit DevTrainingEngine(DevTrainingEngineConfig config = {});

    void reset(
        const DevQNetwork& online_network,
        DevQNetwork& target_network);

    [[nodiscard]]
    DevTrainingStepResult trainBatch(
        const std::vector<DevTransition>& transitions,
        DevQNetwork& online_network,
        DevQNetwork& target_network);

    [[nodiscard]]
    const DevTrainingEngineConfig& config() const noexcept;

    [[nodiscard]]
    std::uint64_t completedTrainingUpdates() const noexcept;

    [[nodiscard]]
    bool initialized() const noexcept;

private:
    DevTrainingEngineConfig config_{};
    DevMiniBatchTrainer mini_batch_trainer_;
    DevTargetNetworkSynchronizer target_synchronizer_;

    std::uint64_t completed_training_updates_{0};
    bool initialized_{false};
};

} // namespace devai::strategy
