#pragma once
#include "devai/strategy/dev_ai_strategy/DevSingleManagerTop5Replay.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
// Phase 3.19D-3B-4C-2B: fail-closed, deterministic integrity checks.
// Each run must construct FRESH runtime, supervisor, and accounting state.
// Frozen model objects are shared by const reference, never trained here.
struct DevSharedReplayIntegrityReport {
    bool passed{false};
    std::size_t ticks{0};
    std::size_t events{0};
    std::string reason;
};
using DevFreshReplayRun = std::function<DevRealReplayResult(const std::vector<DevReplayTick>&)>;
class DevSharedReplayIntegrity {
public:
    [[nodiscard]] static DevSharedReplayIntegrityReport verify(
        const std::vector<DevUniverseRankingTick>& ranking,
        const std::vector<std::string>& universe,
        const DevQNetwork& agent_model,
        const DevManagerQNetwork& manager_model,
        const DevFreshReplayRun& run_fresh);
};
} // namespace devai::strategy
