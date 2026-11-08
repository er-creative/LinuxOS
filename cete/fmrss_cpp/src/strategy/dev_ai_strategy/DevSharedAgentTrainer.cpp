#include "devai/strategy/dev_ai_strategy/DevSharedAgentTrainer.hpp"

#include <stdexcept>

namespace devai::strategy
{

void DevSharedAgentTrainer::validateConfig(
    const DevSharedAgentTrainerConfig& config)
{
    if (config.replay_capacity == 0)
        throw std::invalid_argument("DevSharedAgentTrainer: replay_capacity must be > 0.");

    if (config.minimum_replay_size == 0)
        throw std::invalid_argument("DevSharedAgentTrainer: minimum_replay_size must be > 0.");

    if (config.batch_size == 0)
        throw std::invalid_argument("DevSharedAgentTrainer: batch_size must be > 0.");

    if (config.minimum_replay_size > config.replay_capacity)
        throw std::invalid_argument("DevSharedAgentTrainer: minimum_replay_size exceeds replay_capacity.");

    if (config.batch_size > config.replay_capacity)
        throw std::invalid_argument("DevSharedAgentTrainer: batch_size exceeds replay_capacity.");

    if (config.batch_size > config.minimum_replay_size)
        throw std::invalid_argument("DevSharedAgentTrainer: batch_size must not exceed minimum_replay_size.");
}

DevSharedAgentTrainer::DevSharedAgentTrainer(
    DevSharedAgentTrainerConfig config)
    : config_(config),
      online_network_(config_.online_network),
      target_network_(config_.online_network),
      training_engine_(config_.training),
      replay_buffer_(config_.replay_capacity, config_.replay_seed),
      exploration_policy_(online_network_, config_.exploration)
{
    validateConfig(config_);

    // Frozen Phase 3.8F lifecycle: reset hard-synchronizes target from online.
    training_engine_.reset(online_network_, target_network_);
}

void DevSharedAgentTrainer::observe(
    const DevTransition& transition)
{
    replay_buffer_.push(transition);
}

void DevSharedAgentTrainer::observe(
    const std::vector<DevTransition>& transitions)
{
    for (const DevTransition& transition : transitions)
        observe(transition);
}

bool DevSharedAgentTrainer::canTrain() const noexcept
{
    return replay_buffer_.size() >= config_.minimum_replay_size &&
           replay_buffer_.size() >= config_.batch_size;
}

DevSharedAgentTrainingResult
DevSharedAgentTrainer::trainOneUpdate()
{
    if (!canTrain())
        throw std::runtime_error("DevSharedAgentTrainer::trainOneUpdate: insufficient replay experience.");

    const std::vector<DevTransition> batch =
        replay_buffer_.sample(config_.batch_size);

    const DevTrainingStepResult step =
        training_engine_.trainBatch(batch, online_network_, target_network_);

    DevSharedAgentTrainingResult result{};
    result.training = step;
    result.replay_size = replay_buffer_.size();
    result.batch_size = batch.size();
    result.completed_training_updates = training_engine_.completedTrainingUpdates();
    result.numerical_integrity =
        step.numerical_integrity &&
        result.batch_size == config_.batch_size &&
        result.completed_training_updates > 0;

    if (!result.numerical_integrity)
        throw std::runtime_error("DevSharedAgentTrainer::trainOneUpdate: training result failed integrity validation.");

    return result;
}

DevEpsilonGreedyDecision
DevSharedAgentTrainer::selectAction(
    const DevAIObservation& observation,
    std::int64_t decision_time)
{
    return exploration_policy_.selectActionDetailed(observation, decision_time);
}

void DevSharedAgentTrainer::resetEpisode() noexcept
{
    // Frozen Phase 3.8E semantics: episode reset preserves the global epsilon
    // schedule and RNG progression.
    exploration_policy_.reset();
}

void DevSharedAgentTrainer::clearExperienceAndResetTraining()
{
    replay_buffer_.clear();
    training_engine_.reset(online_network_, target_network_);
}

DevModelPersistenceMetadata
DevSharedAgentTrainer::saveModel(
    const std::filesystem::path& path) const
{
    return persistence_.save(online_network_, path);
}

const DevSharedAgentTrainerConfig&
DevSharedAgentTrainer::config() const noexcept
{
    return config_;
}

const DevQNetwork&
DevSharedAgentTrainer::onlineNetwork() const noexcept
{
    return online_network_;
}

const DevQNetwork&
DevSharedAgentTrainer::targetNetwork() const noexcept
{
    return target_network_;
}

const DevReplayBuffer&
DevSharedAgentTrainer::replayBuffer() const noexcept
{
    return replay_buffer_;
}

std::uint64_t
DevSharedAgentTrainer::completedTrainingUpdates() const noexcept
{
    return training_engine_.completedTrainingUpdates();
}

std::uint64_t
DevSharedAgentTrainer::completedActionSelections() const noexcept
{
    return exploration_policy_.completedActionSelections();
}

} // namespace devai::strategy
