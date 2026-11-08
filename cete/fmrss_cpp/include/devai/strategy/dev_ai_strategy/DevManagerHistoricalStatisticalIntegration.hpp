#pragma once
#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"
#include "devai/strategy/dev_ai_strategy/DevConfigurablePhase4Integration.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
// The manager requests a snapshot from the existing CETE completed-candle pipeline.
// The provider MUST NOT use future candles, labels or forward returns.
using DevHistoricalSnapshotProvider = std::function<devai::ranking::CrossSectionalUniverseSnapshot(
    const std::vector<std::string>& universe, std::int64_t decision_time)>;
// Independently validate actual source candle completion times. A snapshot's
// decision_time alone is NOT proof of causality.
using DevHistoricalSnapshotValidator = std::function<bool(
    const devai::ranking::CrossSectionalUniverseSnapshot&, std::int64_t)>;
// Strategy-defined ordering score; NOT a BUY probability. No implicit formula.
using DevHistoricalStatisticalScore = std::function<double(const DevAIObservation&)>;
// Model inference uses the observation generated from the SAME snapshot.
using DevHistoricalObservationInference = std::function<devai::DevModelAgentReport(
    const std::string& model_id, const std::string& manager_id,
    const std::string& agent_id, const DevAIObservation&)>;

class DevManagerHistoricalStatisticalIntegration final {
public:
    DevManagerHistoricalStatisticalIntegration(
        devai::DevConfigurableManagerSpec spec,
        DevHistoricalSnapshotProvider provider,
        DevHistoricalSnapshotValidator validator,
        DevHistoricalStatisticalScore score,
        DevHistoricalObservationInference inference,
        DevPortfolioState portfolio = {});
    [[nodiscard]] devai::DevManagerCandidateFrame evaluate(std::int64_t completed_boundary_time) const;
    [[nodiscard]] std::vector<devai::DevManagerCandidateFrame> replay(
        const std::vector<std::int64_t>& completed_boundaries) const;
private:
    devai::DevConfigurableManagerSpec spec_;
    DevHistoricalSnapshotProvider provider_;
    DevHistoricalSnapshotValidator validator_;
    DevHistoricalStatisticalScore score_;
    DevHistoricalObservationInference inference_;
    DevPortfolioState portfolio_;
};
} // namespace devai::strategy
