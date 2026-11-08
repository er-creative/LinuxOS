#include "devai/strategy/dev_ai_strategy/DevTrainingEngine.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace devai::strategy
{

DevTrainingEngine::DevTrainingEngine(DevTrainingEngineConfig config)
    : config_(config),
      mini_batch_trainer_(config_.mini_batch),
      target_synchronizer_(config_.target_synchronization)
{
}

void DevTrainingEngine::reset(
    const DevQNetwork& online_network,
    DevQNetwork& target_network)
{
    const auto synchronization =
        target_synchronizer_.reset(online_network, target_network);

    if (!synchronization.numerical_integrity ||
        !synchronization.synchronized)
    {
        throw std::runtime_error(
            "DevTrainingEngine: initial target synchronization failed.");
    }

    completed_training_updates_ = 0;
    initialized_ = true;
}

DevTrainingStepResult DevTrainingEngine::trainBatch(
    const std::vector<DevTransition>& transitions,
    DevQNetwork& online_network,
    DevQNetwork& target_network)
{
    if (!initialized_)
    {
        throw std::logic_error(
            "DevTrainingEngine: reset() must be called before trainBatch().");
    }

    if (transitions.empty())
    {
        throw std::invalid_argument(
            "DevTrainingEngine: training batch must not be empty.");
    }

    if (completed_training_updates_ ==
        std::numeric_limits<std::uint64_t>::max())
    {
        throw std::overflow_error(
            "DevTrainingEngine: training-update counter overflow.");
    }

    // Bellman targets use the pre-synchronization target network.
    // The online network is updated first; target synchronization follows.
    const DevMiniBatchTrainingResult training =
        mini_batch_trainer_.train(
            transitions,
            online_network,
            target_network);

    if (!training.numerical_integrity)
    {
        throw std::runtime_error(
            "DevTrainingEngine: mini-batch training failed numerical integrity.");
    }

    const DevTargetNetworkSynchronizationResult synchronization =
        target_synchronizer_.onOnlineUpdateCompleted(
            online_network,
            target_network);

    if (!synchronization.numerical_integrity)
    {
        throw std::runtime_error(
            "DevTrainingEngine: target synchronization failed numerical integrity.");
    }

    ++completed_training_updates_;

    if (synchronization.completed_online_updates !=
        completed_training_updates_)
    {
        throw std::runtime_error(
            "DevTrainingEngine: training and synchronization counters diverged.");
    }

    DevTrainingStepResult result{};
    result.completed_training_updates = completed_training_updates_;
    result.batch_size = training.batch_size;
    result.mean_bellman_target = training.mean_bellman_target;
    result.mean_loss = training.mean_loss;
    result.mean_td_error = training.mean_td_error;
    result.mean_absolute_td_error = training.mean_absolute_td_error;
    result.gradient_l2_norm = training.gradient_l2_norm;
    result.clipped_gradient_l2_norm = training.clipped_gradient_l2_norm;
    result.target_synchronized = synchronization.synchronized;
    result.target_updates_since_synchronization =
        synchronization.updates_since_synchronization;

    result.numerical_integrity =
        training.numerical_integrity &&
        synchronization.numerical_integrity &&
        std::isfinite(result.mean_bellman_target) &&
        std::isfinite(result.mean_loss) &&
        std::isfinite(result.mean_td_error) &&
        std::isfinite(result.mean_absolute_td_error) &&
        std::isfinite(result.gradient_l2_norm) &&
        std::isfinite(result.clipped_gradient_l2_norm);

    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "DevTrainingEngine: training result failed numerical integrity.");
    }

    return result;
}

const DevTrainingEngineConfig&
DevTrainingEngine::config() const noexcept
{
    return config_;
}

std::uint64_t
DevTrainingEngine::completedTrainingUpdates() const noexcept
{
    return completed_training_updates_;
}

bool DevTrainingEngine::initialized() const noexcept
{
    return initialized_;
}

} // namespace devai::strategy
