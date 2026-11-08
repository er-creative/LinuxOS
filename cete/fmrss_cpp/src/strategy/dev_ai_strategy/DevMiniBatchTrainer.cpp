#include "devai/strategy/dev_ai_strategy/DevMiniBatchTrainer.hpp"

#include <cmath>
#include <stdexcept>

namespace devai::strategy
{

DevMiniBatchTrainer::DevMiniBatchTrainer(
    DevMiniBatchTrainerConfig config)
    : config_(config),
      bellman_target_engine_(config_.bellman)
{
    // Reuse DevQNetwork's public validation without mutating a caller's model.
    DevQNetwork validation_network(
        DevQNetworkConfig{
            .random_seed = 0U,
            .zero_initialize = true});

    DevAIObservation observation{};
    observation.numerical_integrity = true;
    observation.ready = true;

    // A zero-target, zero-gradient update validates the training config.
    // The temporary validation network is discarded immediately.
    (void)validation_network.trainSingle(
        observation,
        DevAction::HOLD,
        0.0,
        config_.training);
}

DevMiniBatchTrainingResult
DevMiniBatchTrainer::train(
    const std::vector<DevTransition>& transitions,
    DevQNetwork& online_network,
    const DevQNetwork& target_network) const
{
    if (transitions.empty())
    {
        throw std::invalid_argument(
            "DevMiniBatchTrainer::train: transition batch must not be empty.");
    }

    std::vector<DevQTrainingSample> samples;
    samples.reserve(transitions.size());

    double target_sum = 0.0;

    // IMPORTANT: all Bellman targets are created before the online network
    // changes. The target network is const and is never mutated here.
    for (const DevTransition& transition : transitions)
    {
        const DevBellmanTargetResult target =
            bellman_target_engine_.calculate(
                transition,
                target_network);

        if (!target.numerical_integrity ||
            !std::isfinite(target.target))
        {
            throw std::runtime_error(
                "DevMiniBatchTrainer::train: invalid Bellman target.");
        }

        samples.push_back(
            DevQTrainingSample{
                .observation = transition.observation,
                .action = transition.action,
                .target_q = target.target});

        target_sum += target.target;

        if (!std::isfinite(target_sum))
        {
            throw std::runtime_error(
                "DevMiniBatchTrainer::train: non-finite target sum.");
        }
    }

    const DevQBatchTrainingResult batch_result =
        online_network.trainBatch(
            samples,
            config_.training);

    DevMiniBatchTrainingResult result{};
    result.batch_size = batch_result.batch_size;
    result.mean_bellman_target =
        target_sum /
        static_cast<double>(transitions.size());

    result.mean_loss =
        batch_result.mean_loss;
    result.mean_td_error =
        batch_result.mean_td_error;
    result.mean_absolute_td_error =
        batch_result.mean_absolute_td_error;
    result.gradient_l2_norm =
        batch_result.gradient_l2_norm;
    result.clipped_gradient_l2_norm =
        batch_result.clipped_gradient_l2_norm;

    result.numerical_integrity =
        batch_result.numerical_integrity &&
        result.batch_size == transitions.size() &&
        std::isfinite(result.mean_bellman_target) &&
        std::isfinite(result.mean_loss) &&
        std::isfinite(result.mean_td_error) &&
        std::isfinite(result.mean_absolute_td_error) &&
        std::isfinite(result.gradient_l2_norm) &&
        std::isfinite(result.clipped_gradient_l2_norm);

    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "DevMiniBatchTrainer::train: result failed numerical-integrity validation.");
    }

    return result;
}

const DevMiniBatchTrainerConfig&
DevMiniBatchTrainer::config() const noexcept
{
    return config_;
}

} // namespace devai::strategy
