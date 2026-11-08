#pragma once
#include "devai/strategy/dev_ai_strategy/DevSingleManagerTop5Replay.hpp"
#include "devai/strategy/dev_ai_strategy/DevEventDrivenRotationReplay.hpp"
#include <functional>
#include <vector>

namespace devai::strategy {
// Training samples are constructed AFTER the event-driven simulation has run.
// No next state or execution-bar price is exposed to the decision callback.
struct DevHistoricalExperience {
    std::int64_t decision_time{0};
    std::int64_t next_decision_time{0};
    std::vector<DevSharedCandidate> candidates;
    std::vector<DevSharedCandidate> next_candidates;
    DevRotationIntent intent;
    double equity_before{0};
    double equity_after{0};
    double portfolio_reward{0}; // equity delta / initial equity; includes supervisor costs
    bool terminal{false};
};
struct DevHistoricalExperienceResult {
    DevRotationReplayResult execution;
    std::vector<DevHistoricalExperience> experiences;
};
// A deterministic policy adapter may be used for testing; in production it
// must call the frozen shared model/DevManager only on current candidates.
using DevHistoricalDecision = std::function<DevRotationIntent(
    const DevReplayTick&, DevSharedModelMultiManagerRuntime&)>;
class DevHistoricalExperienceIntegration {
public:
    [[nodiscard]] static DevRotationIntent frozenModelDecision(
        const DevReplayTick& tick, DevSharedModelMultiManagerRuntime& runtime,
        const DevinderjeetSupervisor& supervisor);
    [[nodiscard]] static DevHistoricalExperienceResult run(
        const std::vector<DevUniverseRankingTick>& ranking,
        const std::vector<std::string>& universe,
        DevSharedModelMultiManagerRuntime& runtime,
        DevinderjeetSupervisor& supervisor,
        const DevRealMinuteMarket& market,
        const DevHistoricalDecision& decision,
        double allocation_fraction=0.50,
        bool terminal_settlement=true);
};
} // namespace devai::strategy
