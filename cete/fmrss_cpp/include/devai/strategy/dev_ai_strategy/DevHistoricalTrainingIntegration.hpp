#pragma once

#include "devai/strategy/dev_ai_strategy/DevSharedAgentTrainer.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace devai::strategy {

// 3.19D-3: Trains only from realized, causally rewarded CETE transitions.
// The caller must construct these using TradingEnvironment (agents) and the
// shared-cash ledger (manager). Forward-return labels are NOT rewards.
struct DevHistoricalTrainingIntegrationConfig {
    DevSharedAgentTrainerConfig agent{};
    DevManagerTrainingConfig manager{};
    std::int64_t training_end_exclusive{0};
    std::size_t agent_updates_per_transition{1};
    std::size_t manager_updates_per_transition{1};
};

struct DevHistoricalTrainingIntegrationReport {
    std::size_t agent_transitions{0};
    std::size_t manager_transitions{0};
    std::uint64_t agent_updates{0};
    std::uint64_t manager_updates{0};
    bool checkpoints_saved{false};
};

class DevHistoricalTrainingIntegration {
public:
    explicit DevHistoricalTrainingIntegration(
        DevHistoricalTrainingIntegrationConfig config);

    // Transitions must be chronological within each stream (agents may share a timestamp).
    // Both endpoints must precede training_end_exclusive; this prevents
    // training targets crossing into validation/OOS.
    [[nodiscard]] DevHistoricalTrainingIntegrationReport train(
        const std::vector<DevTransition>& agent_experiences,
        const std::vector<DevManagerTransition>& manager_experiences);

    // Must only be called after both streams have trained successfully.
    void saveCheckpoints(const std::filesystem::path& agent_path,
                         const std::filesystem::path& manager_path) const;

    [[nodiscard]] const DevSharedAgentTrainer& agentTrainer() const noexcept;
    [[nodiscard]] const DevManagerTrainer& managerTrainer() const noexcept;

private:
    DevHistoricalTrainingIntegrationConfig config_;
    DevSharedAgentTrainer agent_;
    DevManagerTrainer manager_;
    bool trained_{false};
};

} // namespace devai::strategy
