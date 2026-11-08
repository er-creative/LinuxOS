#pragma once
#include "devai/strategy/dev_ai_strategy/DevRealMarketReplayAdapter.hpp"
#include <array>
#include <string>
#include <vector>

namespace devai::strategy {
// A full-universe, decision-time ranking snapshot. Scores and observations MUST
// have been computed using completed candles only (no forward-return targets).
struct DevUniverseRankedShare {
    std::string symbol;
    double score{0};
    DevAIObservation observation{};
};
struct DevUniverseRankingTick {
    std::int64_t decision_time{0};
    std::vector<DevUniverseRankedShare> universe;
};
class DevSingleManagerTop5Replay {
public:
    static constexpr const char* managerId = "DevManager_001";
    [[nodiscard]] static std::array<std::string, DEV_AGENT_SLOT_COUNT> agentIds();
    // Reject partial universes, stale observations, duplicate symbols and ties
    // are resolved deterministically by symbol name.
    [[nodiscard]] static std::vector<DevReplayTick> prepare(
        const std::vector<DevUniverseRankingTick>& ranking_ticks,
        const std::vector<std::string>& expected_universe);
    // Caller must register DevManager_001 with the supervisor and add it to
    // runtime, using agentIds(). Caller must load frozen model checkpoints.
    [[nodiscard]] static DevRealReplayResult run(
        const std::vector<DevReplayTick>& ticks,
        DevSharedModelMultiManagerRuntime& runtime,
        DevinderjeetSupervisor& supervisor,
        const DevRealMinuteMarket& market,
        double allocation_fraction=0.50);
};
}
