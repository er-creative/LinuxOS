#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalManagerStrategyAdapter.hpp"
#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"
#include "devai/ranking/CrossSectionalMathematicalFeatures.hpp"
#include "devai/statistics/MathematicalStateFeatures.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
// Source returns exactly one real, causal mathematical state for each symbol.
// The caller must obtain these from CETE's completed-candle feature pipeline.
using DevMathematicalStateProvider = std::function<std::vector<devai::statistics::MathematicalStateFeatures>(
    const std::vector<std::string>&, std::int64_t)>;

// Map the real Phase-4 ranking output to its symbol and established scalar score.
// No scoring formula is invented by this bridge.
using DevMathematicalRankProjection = std::function<DevRankedStock(
    const devai::ranking::CrossSectionalMathematicalFeatures&, std::int64_t)>;

// Verify the actual source-candle completion times and absence of future labels.
// This validation is mandatory because the supplied headers do not expose those fields.
using DevMathematicalStateValidator = std::function<bool(
    const devai::statistics::MathematicalStateFeatures&, std::int64_t)>;

class DevRealStatisticalStrategyBridge final {
public:
    DevRealStatisticalStrategyBridge(
        DevMathematicalStateProvider state_provider,
        DevMathematicalStateValidator state_validator,
        DevMathematicalRankProjection rank_projection,
        std::size_t minimum_universe_size = 2);

    // This callback can be passed to DevConfigurablePhase4Integration directly.
    // It invokes the REAL CrossSectionalMathematicalRanker::rank(states).
    [[nodiscard]] DevStrategyRankingCallback rankingCallback() const;

private:
    DevMathematicalStateProvider provider_;
    DevMathematicalStateValidator validator_;
    DevMathematicalRankProjection projection_;
    std::size_t minimum_universe_size_;
};
} // namespace devai::strategy
