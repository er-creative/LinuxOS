#pragma once

#include "devai/ranking/CrossSectionalMathematicalFeatures.hpp"
#include "devai/ranking/CrossSectionalMathematicalState.hpp"

#include <vector>


namespace devai::ranking
{

// ============================================================================
// CrossSectionalMathematicalStateComposer
//
// Phase 5.3 — Multi-Timeframe Cross-Sectional State
//
// Responsibilities:
//
// - consume Phase-5.2 output
// - preserve every individual ranking
// - validate rank numerical integrity
// - derive metric readiness
// - derive timeframe readiness
// - derive complete multi-timeframe readiness
//
// It does NOT:
//
// - rerank values
// - calculate new mathematical statistics
// - calculate a master score
// - select Top-N stocks
// - generate BUY / SELL
// - perform forecasting
// - perform reinforcement learning
// ============================================================================

class CrossSectionalMathematicalStateComposer
{
public:

    CrossSectionalMathematicalStateComposer() = default;


    [[nodiscard]]
    std::vector<CrossSectionalMathematicalState>
    compose(
        const std::vector<
            CrossSectionalMathematicalFeatures>&
            features) const;


private:

    static constexpr std::size_t
        METRIC_COUNT = 13;


    static void validateInput(
        const std::vector<
            CrossSectionalMathematicalFeatures>&
            features);


    [[nodiscard]]
    static bool validateRank(
        const CrossSectionalRank& rank,
        const std::string& expected_symbol,
        std::int64_t expected_decision_time);


    [[nodiscard]]
    static CrossSectionalMetricReadiness
    composeMetricReadiness(
        const CrossSectionalRank& rank);


    [[nodiscard]]
    static CrossSectionalHorizonState
    composeHorizon(
        const CrossSectionalMathematicalHorizon&
            rankings,
        const std::string& expected_symbol,
        std::int64_t expected_decision_time);
};

} // namespace devai::ranking