#include "devai/strategy/dev_ai_strategy/DevManagerHistoricalStatisticalIntegration.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"
#include <cmath>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
namespace {
struct DecisionContext {
    std::unordered_map<std::string, DevAIObservation> observations;
};
}
DevManagerHistoricalStatisticalIntegration::DevManagerHistoricalStatisticalIntegration(
    devai::DevConfigurableManagerSpec spec, DevHistoricalSnapshotProvider provider,
    DevHistoricalSnapshotValidator validator, DevHistoricalStatisticalScore score,
    DevHistoricalObservationInference inference, DevPortfolioState portfolio)
    : spec_(std::move(spec)), provider_(std::move(provider)), validator_(std::move(validator)),
      score_(std::move(score)), inference_(std::move(inference)), portfolio_(portfolio) {
    if (spec_.strategy != devai::DevStrategyKind::MathematicalStatistical ||
        !provider_ || !validator_ || !score_ || !inference_)
        throw std::invalid_argument("Mathematical manager requires real snapshot, causality, scoring and model callbacks");
}

devai::DevManagerCandidateFrame DevManagerHistoricalStatisticalIntegration::evaluate(
    std::int64_t time) const {
    // Context is scoped to this evaluation, never cached across decision boundaries.
    auto ctx = std::make_shared<DecisionContext>();
    auto ranking = [this, ctx](devai::DevStrategyKind kind,
        const std::vector<std::string>& universe, std::int64_t boundary) {
        if (kind != devai::DevStrategyKind::MathematicalStatistical)
            throw std::invalid_argument("Unexpected strategy");
        auto snapshot = provider_(universe, boundary);
        if (snapshot.decision_time != boundary || !snapshot.numerical_integrity ||
            snapshot.stocks.size() != universe.size() || !validator_(snapshot, boundary))
            throw std::runtime_error("Incomplete, invalid or noncausal historical snapshot");
        const std::unordered_set<std::string> expected(universe.begin(), universe.end());
        if (expected.size() != universe.size() || expected.count(""))
            throw std::runtime_error("Invalid configured universe");
        std::unordered_set<std::string> seen;
        for (const auto& stock : snapshot.stocks) {
            if (!expected.count(stock.symbol) || !seen.insert(stock.symbol).second ||
                stock.decision_time != boundary ||
                stock.benchmark_symbol != snapshot.benchmark_symbol ||
                !stock.numerical_integrity)
                throw std::runtime_error("Unexpected or invalid stock in snapshot");
        }
        if (snapshot.benchmark_symbol.empty() || expected.count(snapshot.benchmark_symbol))
            throw std::runtime_error("Benchmark missing or included in tradable universe");
        ObservationBuilder builder;
        std::vector<devai::DevRankedStock> ranked;
        ranked.reserve(universe.size());
        for (const auto& symbol : universe) {
            auto observation = builder.build(snapshot, symbol, portfolio_);
            if (!observation.numerical_integrity || !observation.ready)
                throw std::runtime_error("Stock observation not ready: " + symbol);
            const double value = score_(observation);
            if (!std::isfinite(value))
                throw std::runtime_error("Strategy score is not finite");
            if (!ctx->observations.emplace(symbol, std::move(observation)).second)
                throw std::runtime_error("Duplicate observation");
            // Actual feature freshness must be validated by validator_.
            // The snapshot's decision boundary is metadata, not a source-candle timestamp.
            ranked.push_back({symbol, value, boundary});
        }
        return ranked;
    };
    auto model = [this, ctx](const std::string& model_id,
        const std::string& manager_id, const std::string& agent_id,
        const devai::DevRankedStock& stock, std::int64_t boundary) {
        const auto it = ctx->observations.find(stock.symbol);
        if (it == ctx->observations.end() || it->second.decision_time != boundary)
            throw std::runtime_error("Inference observation missing or stale");
        return inference_(model_id, manager_id, agent_id, it->second);
    };
    devai::DevConfigurablePhase4Integration manager(spec_, ranking, model);
    return manager.evaluate(time);
}

std::vector<devai::DevManagerCandidateFrame>
DevManagerHistoricalStatisticalIntegration::replay(
    const std::vector<std::int64_t>& boundaries) const {
    std::vector<devai::DevManagerCandidateFrame> frames;
    frames.reserve(boundaries.size());
    std::int64_t previous = 0;
    for (auto boundary : boundaries) {
        if (boundary <= previous)
            throw std::invalid_argument("Historical boundaries must be strictly increasing");
        frames.push_back(evaluate(boundary));
        previous = boundary;
    }
    return frames;
}
} // namespace devai::strategy
