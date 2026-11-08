#include "devai/strategy/dev_ai_strategy/DevRealStatisticalStrategyBridge.hpp"
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
DevRealStatisticalStrategyBridge::DevRealStatisticalStrategyBridge(
    DevMathematicalStateProvider state_provider,
    DevMathematicalStateValidator state_validator,
    DevMathematicalRankProjection rank_projection,
    std::size_t minimum_universe_size)
    : provider_(std::move(state_provider)), validator_(std::move(state_validator)),
      projection_(std::move(rank_projection)), minimum_universe_size_(minimum_universe_size) {
    if (!provider_ || !validator_ || !projection_ || minimum_universe_size_ < 2)
        throw std::invalid_argument("Valid real state provider, validator, projection and min universe >=2 required");
}

DevStrategyRankingCallback DevRealStatisticalStrategyBridge::rankingCallback() const {
    return [provider = provider_, validator = validator_, projection = projection_,
            minimum = minimum_universe_size_](
        DevStrategyKind kind, const std::vector<std::string>& universe,
        std::int64_t decision_time) -> std::vector<DevRankedStock> {
        if (kind != DevStrategyKind::MathematicalStatistical)
            throw std::invalid_argument("Real statistical bridge only supports MathematicalStatistical strategy");
        if (decision_time <= 0 || universe.size() < minimum)
            throw std::invalid_argument("Invalid decision time or universe size");
        std::unordered_set<std::string> expected(universe.begin(), universe.end());
        if (expected.size() != universe.size() || expected.count(""))
            throw std::invalid_argument("Duplicate or empty configured symbols");
        auto states = provider(universe, decision_time);
        if (states.size() != universe.size())
            throw std::runtime_error("Missing real mathematical states for universe");
        for (const auto& state : states)
            if (!validator(state, decision_time))
                throw std::runtime_error("Noncausal or invalid mathematical state");
        const devai::ranking::CrossSectionalMathematicalRanker ranker(minimum);
        const auto ranked = ranker.rank(states);
        if (ranked.size() != universe.size())
            throw std::runtime_error("Ranker did not return complete universe");
        std::vector<DevRankedStock> output;
        output.reserve(ranked.size());
        std::unordered_set<std::string> seen;
        for (const auto& entry : ranked) {
            auto row = projection(entry, decision_time);
            if (!expected.count(row.symbol) || !seen.insert(row.symbol).second ||
                !std::isfinite(row.ranking_score) ||
                row.latest_observation_time <= 0 ||
                row.latest_observation_time > decision_time)
                throw std::runtime_error("Invalid projected real ranking row");
            output.push_back(std::move(row));
        }
        return output;
    };
}
} // namespace devai::strategy
