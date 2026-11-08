#include "devai/strategy/dev_ai_strategy/DevStatisticalObservationIntegration.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
DevStatisticalObservationIntegration::DevStatisticalObservationIntegration(
    DevConfigurableManagerSpec spec, DevSnapshotProvider snapshot_provider,
    DevSnapshotCausalityCheck causal_check, DevPortfolioStateProvider portfolio_provider,
    DevStatisticalScorePolicy scoring_policy, DevAgentInferenceCallback inference)
    : spec_(std::move(spec)), snapshot_provider_(std::move(snapshot_provider)),
      causal_check_(std::move(causal_check)), portfolio_provider_(std::move(portfolio_provider)),
      scoring_policy_(std::move(scoring_policy)), inference_(std::move(inference)) {
    if (!snapshot_provider_ || !causal_check_ || !portfolio_provider_ || !scoring_policy_ || !inference_)
        throw std::invalid_argument("All real integration providers must be supplied");
    if (spec_.strategy != DevStrategyKind::MathematicalStatistical)
        throw std::invalid_argument("Statistical integration requires MathematicalStatistical strategy");
    // Reuse existing configuration validation without running a cycle.
    DevConfigurablePhase4Integration validate(spec_,
        [](DevStrategyKind, const std::vector<std::string>&, std::int64_t) {
            return std::vector<DevRankedStock>{};
        }, inference_);
}

DevStatisticalManagerCycle DevStatisticalObservationIntegration::evaluate(
    std::int64_t decision_time) const {
    if (decision_time <= 0 || decision_time % spec_.interval_seconds != 0)
        throw std::invalid_argument("Decision must occur on a completed interval boundary");
    const auto snapshot = snapshot_provider_(spec_.universe, decision_time);
    if (snapshot.decision_time != decision_time || !snapshot.numerical_integrity ||
        snapshot.benchmark_symbol.empty() || !causal_check_(snapshot, decision_time))
        throw std::runtime_error("Invalid or noncausal statistical snapshot");
    if (snapshot.stocks.size() != spec_.universe.size())
        throw std::runtime_error("Statistical snapshot has incomplete universe");
    const std::unordered_set<std::string> expected(spec_.universe.begin(), spec_.universe.end());
    if (expected.count(snapshot.benchmark_symbol))
        throw std::runtime_error("Benchmark must not appear in tradable universe");
    std::unordered_set<std::string> seen;
    DevStatisticalManagerCycle result;
    std::vector<DevRankedStock> scores;
    scores.reserve(spec_.universe.size());
    ObservationBuilder builder;
    for (const auto& stock : snapshot.stocks) {
        if (!expected.count(stock.symbol) || !seen.insert(stock.symbol).second ||
            stock.decision_time != decision_time ||
            stock.benchmark_symbol != snapshot.benchmark_symbol)
            throw std::runtime_error("Invalid symbol, duplicate, or mismatched stock state");
        const auto portfolio = portfolio_provider_(stock.symbol, decision_time);
        auto observation = builder.build(snapshot, stock.symbol, portfolio);
        if (!observation.numerical_integrity || !observation.ready)
            throw std::runtime_error("Unavailable or invalid statistical observation");
        const double score = scoring_policy_(observation);
        if (!std::isfinite(score))
            throw std::runtime_error("Nonfinite configured strategy score");
        scores.push_back({stock.symbol, score, decision_time});
        result.observations.emplace(stock.symbol, std::move(observation));
    }
    // Manager-owned configurable integration ranks, selects Top-K and invokes agents.
    DevConfigurablePhase4Integration manager(
        spec_, [scores](DevStrategyKind, const std::vector<std::string>&, std::int64_t) {
            return scores;
        }, inference_);
    result.candidates = manager.evaluate(decision_time);
    return result;
}
} // namespace devai::strategy
