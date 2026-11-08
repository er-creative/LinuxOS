#pragma once
#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"
#include "devai/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace devai::strategy {
// Scoring belongs to the configured strategy. No score is invented here.
// Receives only the causal observation at the current decision boundary.
using DevStatisticalScorePolicy = std::function<double(const DevAIObservation&)>;
using DevPortfolioStateProvider = std::function<DevPortfolioState(const std::string&, std::int64_t)>;
using DevSnapshotProvider = std::function<devai::ranking::CrossSectionalUniverseSnapshot(
    const std::vector<std::string>&, std::int64_t)>;
// Mandatory production check against the source candle completion times.
using DevSnapshotCausalityCheck = std::function<bool(
    const devai::ranking::CrossSectionalUniverseSnapshot&, std::int64_t)>;

struct DevStatisticalManagerCycle {
    DevManagerCandidateFrame candidates;
    std::map<std::string, DevAIObservation> observations; // Full universe, not just Top-K
};

class DevStatisticalObservationIntegration final {
public:
    DevStatisticalObservationIntegration(DevConfigurableManagerSpec spec,
        DevSnapshotProvider snapshot_provider,
        DevSnapshotCausalityCheck causal_check,
        DevPortfolioStateProvider portfolio_provider,
        DevStatisticalScorePolicy scoring_policy,
        DevAgentInferenceCallback inference);

    [[nodiscard]] DevStatisticalManagerCycle evaluate(std::int64_t decision_time) const;
private:
    DevConfigurableManagerSpec spec_;
    DevSnapshotProvider snapshot_provider_;
    DevSnapshotCausalityCheck causal_check_;
    DevPortfolioStateProvider portfolio_provider_;
    DevStatisticalScorePolicy scoring_policy_;
    DevAgentInferenceCallback inference_;
};
} // namespace devai::strategy
