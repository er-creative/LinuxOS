#pragma once

#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"

#include <cstddef>
#include <vector>


namespace devai::ranking
{

// ============================================================================
// CrossSectionalUniverseSnapshotBuilder
//
// Phase 5.4 — Cross-Sectional Universe Snapshot
//
// Responsibilities:
//
// - consume Phase-5.3 per-stock states
// - enforce common decision time
// - enforce common benchmark
// - enforce unique stock identity
// - exclude benchmark from ranked stock universe
// - deterministically order stocks
// - preserve all Phase-5.3 state
// - verify metric-specific universe sizes
// - derive universe-level readiness/integrity
//
// It does NOT:
//
// - recalculate mathematical statistics
// - rerank stocks
// - calculate a master score
// - select Top-N stocks
// - generate BUY / SELL
// - forecast returns
// - perform reinforcement learning
// ============================================================================

class CrossSectionalUniverseSnapshotBuilder
{
public:

    explicit CrossSectionalUniverseSnapshotBuilder(
        std::size_t minimum_universe_size = 2);


    [[nodiscard]]
    std::size_t minimumUniverseSize() const noexcept;


    [[nodiscard]]
    CrossSectionalUniverseSnapshot
    build(
        const std::vector<
            CrossSectionalMathematicalState>&
            states) const;


private:

    std::size_t minimum_universe_size_;


    static void validateIdentity(
        const std::vector<
            CrossSectionalMathematicalState>&
            states);


    [[nodiscard]]
    static bool validateMetricRank(
        const CrossSectionalRank& rank,
        const std::string& expected_symbol,
        std::int64_t expected_decision_time,
        std::size_t total_stock_count);


    [[nodiscard]]
    static bool validateHorizon(
        const CrossSectionalHorizonState& horizon,
        const std::string& expected_symbol,
        std::int64_t expected_decision_time,
        std::size_t total_stock_count);


    [[nodiscard]]
    static bool validateState(
        const CrossSectionalMathematicalState& state,
        std::size_t total_stock_count);


    [[nodiscard]]
    static std::size_t determineUniverseSize(
        const std::vector<
            CrossSectionalMathematicalState>&
            states,
        const CrossSectionalRank
            CrossSectionalMathematicalHorizon::* member,
        const CrossSectionalHorizonState
            CrossSectionalMathematicalState::* horizon_member);


    [[nodiscard]]
    static CrossSectionalMetricUniverseSizes
    determineHorizonUniverseSizes(
        const std::vector<
            CrossSectionalMathematicalState>&
            states,
        const CrossSectionalHorizonState
            CrossSectionalMathematicalState::* horizon_member);


    [[nodiscard]]
    bool validateUniverseSize(
        std::size_t universe_size,
        std::size_t stock_count) const noexcept;
};

} // namespace devai::ranking