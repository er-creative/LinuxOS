#pragma once
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include <cstddef>
#include <cstdint>

namespace devai::strategy {

struct DevTargetNetworkSynchronizerConfig {
    std::size_t synchronization_interval{1000};
};

struct DevTargetNetworkSynchronizationResult {
    std::uint64_t completed_online_updates{0};
    std::size_t updates_since_synchronization{0};
    bool synchronized{false};
    bool numerical_integrity{false};
};

class DevTargetNetworkSynchronizer {
public:
    explicit DevTargetNetworkSynchronizer(
        DevTargetNetworkSynchronizerConfig config = {});

    [[nodiscard]] DevTargetNetworkSynchronizationResult reset(
        const DevQNetwork& online_network,
        DevQNetwork& target_network);

    [[nodiscard]] DevTargetNetworkSynchronizationResult onOnlineUpdateCompleted(
        const DevQNetwork& online_network,
        DevQNetwork& target_network);

    [[nodiscard]] DevTargetNetworkSynchronizationResult synchronizeNow(
        const DevQNetwork& online_network,
        DevQNetwork& target_network);

    [[nodiscard]] const DevTargetNetworkSynchronizerConfig& config() const noexcept;
    [[nodiscard]] std::uint64_t completedOnlineUpdates() const noexcept;
    [[nodiscard]] std::size_t updatesSinceSynchronization() const noexcept;

private:
    static void validateConfig(const DevTargetNetworkSynchronizerConfig& config);
    [[nodiscard]] DevTargetNetworkSynchronizationResult makeResult(bool synchronized) const;

    DevTargetNetworkSynchronizerConfig config_;
    std::uint64_t completed_online_updates_{0};
    std::size_t updates_since_synchronization_{0};
};

} // namespace devai::strategy
