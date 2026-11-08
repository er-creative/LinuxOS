#pragma once
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockFeed.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
// A strategy-owned policy, never inferred from the Phase 4/5 metric set.
// Return eligible=false to exclude a stock. Higher score ranks first.
struct DevManager001Assessment {
    bool eligible{false};
    double ranking_score{0.0};
    std::string reason;
};
using DevManager001StatisticalPolicy = std::function<DevManager001Assessment(
    const DevHistoricalStockFrame&, const DevHistoricalManagerFrame&)>;
struct DevManager001RankedCandidate {
    std::string symbol;
    double ranking_score{0.0};
    DevHistoricalStockFrame observations;
};
struct DevManager001RankingFrame {
    std::string manager_id;
    std::int64_t decision_key{0};
    std::vector<DevManager001RankedCandidate> selected;
    std::size_t evaluated{0};
    std::size_t rejected{0};
};
struct DevManager001RankingConfig {
    std::string manager_id{"DevManager_001"};
    std::size_t top_k{5};
};
class DevManager001HistoricalRanking final {
public:
    DevManager001HistoricalRanking(DevManager001RankingConfig config,
                                  DevManager001StatisticalPolicy strategy_policy);
    DevManager001RankingFrame evaluate(const DevHistoricalManagerFrame& frame) const;
private:
    DevManager001RankingConfig config_;
    DevManager001StatisticalPolicy policy_;
};
// Runs D5 historical feed, then invokes the manager-owned strategy and Top-K policy.
// Does not run RL inference or authorize a trade.
struct DevManager001ReplayReport {
    DevHistoricalFeedReport feed;
    std::size_t manager_frames{0};
    std::size_t frames_with_candidates{0};
    std::size_t candidate_assignments{0};
};
using DevManager001RankingConsumer = std::function<void(const DevManager001RankingFrame&)>;
DevManager001ReplayReport replayDevManager001Ranking(
    const DevRealHistoricalMultiStockFeed& feed,
    const std::vector<DevHistoricalMinuteInput>& inputs,
    const DevManager001HistoricalRanking& manager,
    const DevManager001RankingConsumer& consumer);
} // namespace devai::strategy
