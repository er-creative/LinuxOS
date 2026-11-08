#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingIntegration.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace devai::strategy {
// Explicitly enriched transitions: never infer a manager action, agent action,
// manager state, or per-agent reward from a portfolio-only equity change.
struct DevRewardedTrainingSample {
    DevHistoricalExperience historical;
    DevManagerTransition manager;
    std::vector<DevTransition> agents; // only causally attributable actions
    std::string manager_id{"DevManager_001"};
    bool manager_reward_from_supervisor{true};
};
struct DevRewardedTrainingReport {
    DevHistoricalTrainingIntegrationReport optimizer;
    std::size_t decision_samples{0};
    std::size_t attributed_agent_samples{0};
};
class DevSupervisorRewardedTraining {
public:
    static void validate(const std::vector<DevRewardedTrainingSample>& samples,
                         std::int64_t training_end_exclusive);
    [[nodiscard]] static DevRewardedTrainingReport train(
        const std::vector<DevRewardedTrainingSample>& samples,
        DevHistoricalTrainingIntegration& trainer,
        std::int64_t training_end_exclusive);
    // Both files constitute one logical model version; caller manages atomic
    // publication and metadata/checksums after verification.
    static void save(const DevHistoricalTrainingIntegration& trainer,
                     const std::filesystem::path& agent_path,
                     const std::filesystem::path& manager_path);
};
} // namespace devai::strategy
