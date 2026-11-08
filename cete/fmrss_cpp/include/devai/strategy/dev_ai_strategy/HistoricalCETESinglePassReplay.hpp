#pragma once
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace devai::strategy {
// Single chronological feature-engine traversal PER STOCK for the entire
// requested interval. Phase-5 cross-sectional snapshots are emitted in time order.
class HistoricalCETESinglePassReplay {
public:
    using Progress = std::function<void(std::size_t, std::size_t, const std::string&)>;
    explicit HistoricalCETESinglePassReplay(HistoricalCETEReplayConfig config = {});
    [[nodiscard]] std::vector<HistoricalCETEReplayPoint> run(
        std::int64_t start_decision_time, std::int64_t end_decision_time,
        std::int64_t step_seconds = 60, const Progress& progress = {}) const;
private:
    HistoricalCETEReplayConfig config_;
};
} // namespace devai::strategy
