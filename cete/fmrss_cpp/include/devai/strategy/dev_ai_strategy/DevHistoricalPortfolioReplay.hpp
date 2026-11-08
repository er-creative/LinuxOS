#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevPortfolioEnvironment.hpp"
#include <filesystem>
#include <cstddef>
#include <string>

namespace devai::strategy {
struct DevHistoricalPortfolioReplayConfig {
    std::filesystem::path minute_folder;
    std::filesystem::path model_checkpoint; // empty => deterministic plumbing-test policy
    std::filesystem::path audit_csv;
    std::size_t maximum_holding_bars{5};
    double minimum_q_advantage{0.0};
    DevPortfolioConfig portfolio{};
};
struct DevHistoricalPortfolioReplayStats {
    std::size_t steps{}, opened{}, exited{}, rejected{}, hold_steps{};
    double ending_equity{}, total_reward{};
};
class DevHistoricalPortfolioReplay {
public:
    explicit DevHistoricalPortfolioReplay(DevHistoricalPortfolioReplayConfig config);
    DevHistoricalPortfolioReplayStats run(const DevHistoricalTrainingDataset& dataset) const;
private:
    DevHistoricalPortfolioReplayConfig config_;
};
}
