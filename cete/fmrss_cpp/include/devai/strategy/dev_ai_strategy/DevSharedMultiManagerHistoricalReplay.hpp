#pragma once
#include "devai/strategy/dev_ai_strategy/DevSharedModelMultiManagerRuntime.hpp"
#include <functional>
#include <map>
#include <vector>

namespace devai::strategy {
// All prices must be known at decision_time. For minute candles, a bar with
// timestamp t is NOT completed until t+60 seconds. Do not use forward labels.
using DevReplayCompletedPrice = std::function<double(const std::string&, std::int64_t)>;
struct DevReplayManagerInput {
    std::string manager_id;
    std::vector<DevSharedCandidate> candidates;
};
struct DevReplayTick {
    std::int64_t decision_time{0};
    std::vector<DevReplayManagerInput> managers;
};
struct DevReplayConfig {
    double target_manager_allocation_fraction{0.50};
    bool liquidate_at_end{true};
};
struct DevReplayEvent {
    std::int64_t time{0};
    std::string manager_id;
    std::string symbol;
    std::string action;
    DevOrderReport report{};
};
struct DevReplayResult {
    std::vector<DevReplayEvent> events;
    std::vector<double> equity_curve;
    double initial_equity{0.0};
    double final_equity{0.0};
    double net_pnl{0.0};
    double max_drawdown_fraction{0.0};
    std::size_t filled{0};
    std::size_t rejected{0};
};
// The caller owns the single shared inference runtime and supervisor.
// This is a historical execution runner, not a training loop or live broker.
class DevSharedMultiManagerHistoricalReplay {
public:
    explicit DevSharedMultiManagerHistoricalReplay(DevReplayConfig config = {});
    [[nodiscard]] DevReplayResult run(
        const std::vector<DevReplayTick>& ticks,
        DevSharedModelMultiManagerRuntime& runtime,
        DevinderjeetSupervisor& supervisor,
        const DevReplayCompletedPrice& completed_price) const;
private:
    DevReplayConfig config_;
};
} // namespace devai::strategy
