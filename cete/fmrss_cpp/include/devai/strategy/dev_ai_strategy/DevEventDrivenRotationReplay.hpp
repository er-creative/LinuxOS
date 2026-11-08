#pragma once
#include "devai/strategy/dev_ai_strategy/DevRealMarketReplayAdapter.hpp"
#include <functional>

namespace devai::strategy {
// A frozen-model/DevManager adapter supplies a CAUSAL intent at each boundary.
// No inference or future execution prices are passed into the callback.
enum class DevRotationAction { HOLD, BUY, EXIT, ROTATE };
struct DevRotationIntent {
    DevRotationAction action{DevRotationAction::HOLD};
    std::string symbol;   // BUY or ROTATE target; EXIT may leave empty
    std::string agent_id; // selected global DevAgent_001..005 for BUY/ROTATE
};
using DevRotationPolicy = std::function<DevRotationIntent(std::int64_t)>;
struct DevRotationReplayResult {
    DevReplayResult replay;
    std::size_t decision_count{0};
    std::size_t rotation_count{0};
    std::size_t skipped_no_bar{0};
};
// Exactly one registered manager and at most one held position. Orders fill
// at the first eligible minute OPEN, only when simulation time reaches it.
class DevEventDrivenRotationReplay {
public:
    [[nodiscard]] DevRotationReplayResult run(
        const std::vector<std::int64_t>& decision_times,
        const std::string& manager_id,
        const DevRotationPolicy& policy,
        DevinderjeetSupervisor& supervisor,
        const DevRealMinuteMarket& market,
        double allocation_fraction=0.50,
        bool terminal_settlement=true) const;
};
} // namespace devai::strategy
