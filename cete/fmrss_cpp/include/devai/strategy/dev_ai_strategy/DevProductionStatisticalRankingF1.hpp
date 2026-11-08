#pragma once
// D9F.4C-F1: explicit Forecast-OFF cross-sectional RETURN z-score baseline.
// Not the D8 close-price z-score policy. Not a profitability claim.
#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace devai::strategy {
class DevProductionStatisticalRankingF1 final {
public:
    // Only observed return Z-scores are used. Higher magnitude ranks first.
    // These are Phase-5.2 rank.value fields, NOT rank.percentile or availability counts.
    static DevTopCandidateSelection select(
        const devai::ranking::CrossSectionalUniverseSnapshot& snapshot,
        const DevTopCandidateSelector& selector) {
        if (snapshot.decision_time <= 0 || snapshot.benchmark_symbol.empty() ||
            snapshot.stocks.empty() || snapshot.stock_count != snapshot.stocks.size() ||
            !snapshot.numerical_integrity)
            throw std::invalid_argument("F1 invalid universe snapshot");
        std::unordered_set<std::string> seen;
        std::vector<DevCandidateScore> candidates;
        candidates.reserve(snapshot.stocks.size());
        for (const auto& stock : snapshot.stocks) {
            if (stock.symbol.empty() || stock.symbol == snapshot.benchmark_symbol ||
                !seen.insert(stock.symbol).second ||
                stock.decision_time != snapshot.decision_time ||
                stock.benchmark_symbol != snapshot.benchmark_symbol ||
                !stock.numerical_integrity)
                throw std::invalid_argument("F1 inconsistent stock identity/integrity");
            const auto& a = stock.one_minute.rankings.return_z_score;
            const auto& b = stock.five_minute.rankings.return_z_score;
            const auto& c = stock.fifteen_minute.rankings.return_z_score;
            const bool ready = stock.one_minute_ready && stock.five_minute_ready &&
                stock.fifteen_minute_ready && a.available && b.available && c.available &&
                std::isfinite(a.value) && std::isfinite(b.value) && std::isfinite(c.value) &&
                a.symbol == stock.symbol && b.symbol == stock.symbol && c.symbol == stock.symbol &&
                a.decision_time == snapshot.decision_time &&
                b.decision_time == snapshot.decision_time &&
                c.decision_time == snapshot.decision_time;
            DevCandidateScore item;
            item.symbol = stock.symbol;
            item.decision_time = snapshot.decision_time;
            item.numerical_integrity = true;
            item.eligible = ready;
            item.score = ready ? (0.5 * std::abs(a.value) +
                                  0.3 * std::abs(b.value) +
                                  0.2 * std::abs(c.value)) : 0.0;
            if (ready && !std::isfinite(item.score))
                throw std::runtime_error("F1 nonfinite statistical score");
            candidates.push_back(item);
        }
        const auto selection = selector.select(candidates, snapshot.benchmark_symbol);
        if (selection.decision_time != snapshot.decision_time)
            throw std::runtime_error("F1 selector decision-time mismatch");
        return selection;
    }
};
} // namespace devai::strategy
