#include "devai/strategy/dev_ai_strategy/DevTargetNetworkSynchronizer.hpp"
#include <limits>
#include <stdexcept>

namespace devai::strategy {

DevTargetNetworkSynchronizer::DevTargetNetworkSynchronizer(
    DevTargetNetworkSynchronizerConfig config)
    : config_(config) {
    validateConfig(config_);
}

DevTargetNetworkSynchronizationResult
DevTargetNetworkSynchronizer::reset(
    const DevQNetwork& online_network,
    DevQNetwork& target_network) {
    completed_online_updates_ = 0;
    updates_since_synchronization_ = 0;
    target_network.copyParametersFrom(online_network);
    return makeResult(true);
}

DevTargetNetworkSynchronizationResult
DevTargetNetworkSynchronizer::onOnlineUpdateCompleted(
    const DevQNetwork& online_network,
    DevQNetwork& target_network) {
    if (completed_online_updates_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("DevTargetNetworkSynchronizer: total update counter overflow.");
    if (updates_since_synchronization_ == std::numeric_limits<std::size_t>::max())
        throw std::overflow_error("DevTargetNetworkSynchronizer: interval counter overflow.");

    ++completed_online_updates_;
    ++updates_since_synchronization_;

    if (updates_since_synchronization_ < config_.synchronization_interval)
        return makeResult(false);

    target_network.copyParametersFrom(online_network);
    updates_since_synchronization_ = 0;
    return makeResult(true);
}

DevTargetNetworkSynchronizationResult
DevTargetNetworkSynchronizer::synchronizeNow(
    const DevQNetwork& online_network,
    DevQNetwork& target_network) {
    target_network.copyParametersFrom(online_network);
    updates_since_synchronization_ = 0;
    return makeResult(true);
}

const DevTargetNetworkSynchronizerConfig&
DevTargetNetworkSynchronizer::config() const noexcept {
    return config_;
}

std::uint64_t DevTargetNetworkSynchronizer::completedOnlineUpdates() const noexcept {
    return completed_online_updates_;
}

std::size_t DevTargetNetworkSynchronizer::updatesSinceSynchronization() const noexcept {
    return updates_since_synchronization_;
}

void DevTargetNetworkSynchronizer::validateConfig(
    const DevTargetNetworkSynchronizerConfig& config) {
    if (config.synchronization_interval == 0)
        throw std::invalid_argument(
            "DevTargetNetworkSynchronizer: synchronization interval must be > 0.");
}

DevTargetNetworkSynchronizationResult
DevTargetNetworkSynchronizer::makeResult(bool synchronized) const {
    DevTargetNetworkSynchronizationResult result{};
    result.completed_online_updates = completed_online_updates_;
    result.updates_since_synchronization = updates_since_synchronization_;
    result.synchronized = synchronized;
    result.numerical_integrity = true;
    return result;
}

} // namespace devai::strategy
