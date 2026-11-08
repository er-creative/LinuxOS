#pragma once

#include "devai/strategy/dev_ai_strategy/DevBellmanTargetEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"

#include <cstddef>
#include <vector>

namespace devai::strategy
{

struct DevMiniBatchTrainerConfig
{
    DevBellmanTargetConfig bellman{};
    DevQTrainingConfig training{};
};

struct DevMiniBatchTrainingResult
{
    std::size_t batch_size{0};

    double mean_bellman_target{0.0};
    double mean_loss{0.0};
    double mean_td_error{0.0};
    double mean_absolute_td_error{0.0};

    double gradient_l2_norm{0.0};
    double clipped_gradient_l2_norm{0.0};

    bool numerical_integrity{false};
};

class DevMiniBatchTrainer
{
public:
    explicit DevMiniBatchTrainer(
        DevMiniBatchTrainerConfig config = {});

    // Phase 3.8C contract:
    //
    // 1. Build every Bellman target using the frozen target network.
    // 2. Do not mutate the target network.
    // 3. Train the online network with one averaged mini-batch gradient.
    [[nodiscard]]
    DevMiniBatchTrainingResult train(
        const std::vector<DevTransition>& transitions,
        DevQNetwork& online_network,
        const DevQNetwork& target_network) const;

    [[nodiscard]]
    const DevMiniBatchTrainerConfig& config() const noexcept;

private:
    DevMiniBatchTrainerConfig config_;
    DevBellmanTargetEngine bellman_target_engine_;
};

} // namespace devai::strategy
