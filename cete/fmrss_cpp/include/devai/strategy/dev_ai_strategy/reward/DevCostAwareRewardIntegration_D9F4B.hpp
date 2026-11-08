#pragma once
#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include "devai/strategy/dev_ai_strategy/DevPortfolioEnvironment.hpp"
#include <vector>

namespace devai::d9f4b {
class DevCostAwareRewardIntegration final {
public:
    // Historical replay equity is already net of execution costs.
    // No cost estimate is added or subtracted by this adapter.
    static RewardResult fromHistorical(
        const devai::strategy::DevHistoricalExperience& sample,
        double initial_equity);
    static void validateHistorical(
        const devai::strategy::DevHistoricalExperienceResult& result);
    // Environment's step.reward is already a net-equity reward.
    static RewardResult fromPortfolioStep(
        const devai::strategy::DevPortfolioStep& step,
        double initial_cash);
};
}
