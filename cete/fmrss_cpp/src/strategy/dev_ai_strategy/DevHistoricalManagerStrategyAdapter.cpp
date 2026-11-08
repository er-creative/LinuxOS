#include "devai/strategy/dev_ai_strategy/DevHistoricalManagerStrategyAdapter.hpp"
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
DevHistoricalManagerStrategyAdapter::DevHistoricalManagerStrategyAdapter(
    DevConfigurableManagerSpec spec,
    DevHistoricalRankingProvider ranking_provider,
    DevObservationCausalityValidator causal_validator,
    DevAgentInferenceCallback model_inference)
    : spec_(std::move(spec)), provider_(std::move(ranking_provider)),
      validator_(std::move(causal_validator)), model_(std::move(model_inference)) {
    if (!provider_ || !validator_ || !model_)
        throw std::invalid_argument("Ranking provider, causal validator and model required");
    // Config validation is performed by the existing configurable integration.
    DevConfigurablePhase4Integration check(spec_,
        [](DevStrategyKind, const std::vector<std::string>&, std::int64_t) {
            return std::vector<DevRankedStock>{};
        }, model_);
}

DevHistoricalManagerFrame DevHistoricalManagerStrategyAdapter::evaluate(
    std::int64_t completed_boundary_time) const {
    // Provider is invoked ONCE for the full universe at this decision time.
    const auto tick = provider_(spec_, completed_boundary_time);
    if (tick.decision_time != completed_boundary_time)
        throw std::runtime_error("Historical ranking tick time mismatch");
    if (tick.universe.size() != spec_.universe.size())
        throw std::runtime_error("Incomplete historical universe");
    const std::unordered_set<std::string> expected(
        spec_.universe.begin(), spec_.universe.end());
    std::unordered_set<std::string> seen;
    std::vector<DevRankedStock> ranks;
    ranks.reserve(tick.universe.size());
    for (const auto& row : tick.universe) {
        if (!expected.count(row.symbol) || !seen.insert(row.symbol).second)
            throw std::runtime_error("Unknown or duplicate historical symbol");
        if (!validator_(row, completed_boundary_time))
            throw std::runtime_error("Noncausal historical observation");
        // Historical score comes from the existing strategy; no invented score.
        // Observation timestamps are validated by validator_ above.
        ranks.push_back({row.symbol, row.score, completed_boundary_time});
    }
    // The manager-owned integration performs ranking, Top-K and K model calls.
    DevConfigurablePhase4Integration integration(
        spec_,
        [ranks](DevStrategyKind, const std::vector<std::string>&, std::int64_t) {
            return ranks;
        }, model_);
    return {integration.evaluate(completed_boundary_time), tick};
}

std::vector<DevHistoricalManagerFrame>
DevHistoricalManagerStrategyAdapter::replay(
    const std::vector<std::int64_t>& times) const {
    std::vector<DevHistoricalManagerFrame> result;
    result.reserve(times.size());
    std::int64_t previous = 0;
    for (const auto time : times) {
        if (time <= previous)
            throw std::invalid_argument("Historical decision times must increase strictly");
        if (previous && time - previous < spec_.interval_seconds)
            throw std::invalid_argument("Decision times closer than configured interval");
        result.push_back(evaluate(time));
        previous = time;
    }
    return result;
}
} // namespace devai::strategy
