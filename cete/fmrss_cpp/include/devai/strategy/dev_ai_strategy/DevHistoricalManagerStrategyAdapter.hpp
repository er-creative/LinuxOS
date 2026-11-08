#pragma once
#include "devai/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.hpp"
#include "devai/strategy/dev_ai_strategy/DevSingleManagerTop5Replay.hpp"
#include <functional>
#include <vector>

namespace devai::strategy {

// Historical Phase-4 input produced by the REAL manager-selected strategy.
// The strategy producer, not this adapter, computes the scores.
using DevHistoricalRankingProvider = std::function<DevUniverseRankingTick(
    const DevConfigurableManagerSpec&, std::int64_t decision_time)>;

// Mandatory validation of observation timestamps and feature causality.
// Must check each observation's actual source-candle completion time.
// Returning true without validating is NOT safe for production.
using DevObservationCausalityValidator = std::function<bool(
    const DevUniverseRankedShare&, std::int64_t decision_time)>;

struct DevHistoricalManagerFrame {
    DevManagerCandidateFrame candidate_frame;
    DevUniverseRankingTick complete_universe;
};

class DevHistoricalManagerStrategyAdapter final {
public:
    DevHistoricalManagerStrategyAdapter(
        DevConfigurableManagerSpec spec,
        DevHistoricalRankingProvider ranking_provider,
        DevObservationCausalityValidator causal_validator,
        DevAgentInferenceCallback model_inference);

    [[nodiscard]] DevHistoricalManagerFrame evaluate(
        std::int64_t completed_boundary_time) const;

    [[nodiscard]] std::vector<DevHistoricalManagerFrame> replay(
        const std::vector<std::int64_t>& completed_boundary_times) const;

private:
    DevConfigurableManagerSpec spec_;
    DevHistoricalRankingProvider provider_;
    DevObservationCausalityValidator validator_;
    DevAgentInferenceCallback model_;
};
} // namespace devai::strategy
