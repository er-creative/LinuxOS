#pragma once
#include "devai/strategy/dev_ai_strategy/DevManager001StatisticalStrategyD7.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace devai::strategy {
// D7B integrates D5 historical observations with the D7 strategy under
// DevManager_001 control. It never reads candles beyond the decision boundary.
struct DevD7BConfig {
    std::string manager_id{"DevManager_001"};
    std::size_t top_k{5};
    std::int64_t decision_interval_minutes{1};
};
struct DevD7BReport {
    DevHistoricalFeedReport feed;
    std::size_t observed_frames{0}, decision_frames{0};
    std::size_t evaluated{0}, rejected{0}, selected{0};
};
using DevD7BConsumer = std::function<void(const DevManager001RankingFrame&)>;
class DevManager001HistoricalStrategyD7B {
public:
    DevManager001HistoricalStrategyD7B(
        DevD7BConfig config, const DevManager001StatisticalStrategyD7& strategy);
    [[nodiscard]] DevD7BReport replay(
        const DevRealHistoricalMultiStockFeed& feed,
        const std::vector<DevHistoricalMinuteInput>& inputs,
        const DevD7BConsumer& consumer) const;
private:
    DevD7BConfig config_;
    DevManager001HistoricalRanking manager_;
};

// Past-only diagnostic forecast: last COMPLETED 5m candle close/open return (carried forward up to 4 minutes).
// NOT a trained forward-return forecast, NOT production strategy.
[[nodiscard]] DevD7Forecast diagnosticPastOnlyForecast(
    const DevHistoricalStockFrame& stock,
    const DevHistoricalManagerFrame& frame);
} // namespace devai::strategy
