#pragma once

#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy
{

using DevHistoricalSnapshot =
    devai::ranking::CrossSectionalUniverseSnapshot;

using DevHistoricalSnapshotSequence =
    std::vector<DevHistoricalSnapshot>;

using DevHistoricalSelectionProvider =
    std::function<DevTopCandidateSelection(
        const DevHistoricalSnapshot&)>;

using DevHistoricalPortfolioProvider =
    std::function<DevPortfolioState(
        const DevHistoricalSnapshot&,
        const std::string&)>;

using DevHistoricalOutcomeProvider =
    std::function<std::vector<DevHistoricalOutcome>(
        const DevHistoricalSnapshot&,
        const std::string&)>;

// Test/adapter seam. Production conversion uses ObservationBuilder internally.
using DevHistoricalObservationProvider =
    std::function<DevAIObservation(
        const DevHistoricalSnapshot&,
        const std::string&,
        const DevPortfolioState&)>;

struct DevRealHistoricalCacheGeneratorStats
{
    std::size_t input_snapshot_count{0};
    std::size_t generated_record_count{0};
    std::size_t generated_candidate_count{0};
    std::size_t empty_candidate_records{0};
    std::size_t outcome_count{0};
    std::size_t available_outcome_count{0};
    bool numerical_integrity{false};
};

struct DevRealHistoricalCacheGenerationResult
{
    DevHistoricalTrainingDataset dataset;
    DevRealHistoricalCacheGeneratorStats stats;
};

class DevRealHistoricalCacheGenerator
{
public:
    // Preferred 3.15B conversion boundary. The caller supplies an already
    // generated chronological CETE snapshot sequence. This prevents the cache
    // layer from owning/repeating CETE mathematics.
    [[nodiscard]]
    DevRealHistoricalCacheGenerationResult generate(
        const DevHistoricalSnapshotSequence& snapshots,
        const DevHistoricalSelectionProvider& selection_provider,
        const DevHistoricalPortfolioProvider& portfolio_provider = {},
        const DevHistoricalOutcomeProvider& outcome_provider = {}) const;

    // Adapter/test seam for validating the record contract without requiring
    // fully populated Phase-5 ranked observations.
    [[nodiscard]]
    DevRealHistoricalCacheGenerationResult generateWithObservationProvider(
        const DevHistoricalSnapshotSequence& snapshots,
        const DevHistoricalSelectionProvider& selection_provider,
        const DevHistoricalObservationProvider& observation_provider,
        const DevHistoricalPortfolioProvider& portfolio_provider = {},
        const DevHistoricalOutcomeProvider& outcome_provider = {}) const;

    // Compatibility helper for the CURRENT frozen HistoricalCETEReplay API.
    // That API exposes replayAt(), not replay()/HistoricalCETEReplayResult.
    // This helper is intentionally bounded and is for smoke/integration runs;
    // it is NOT the final high-performance sequential replay implementation.
    [[nodiscard]]
    DevRealHistoricalCacheGenerationResult generateBoundedWithReplayAt(
        const HistoricalCETEReplay& replay,
        std::int64_t start_decision_time,
        std::int64_t end_decision_time,
        std::int64_t step_seconds,
        const DevHistoricalSelectionProvider& selection_provider,
        const DevHistoricalPortfolioProvider& portfolio_provider = {},
        const DevHistoricalOutcomeProvider& outcome_provider = {}) const;

private:
    [[nodiscard]]
    static DevPortfolioState flatPortfolio() noexcept;

    static void validateSnapshots(
        const DevHistoricalSnapshotSequence& snapshots);

    static void validateSelection(
        const DevTopCandidateSelection& selection,
        const DevHistoricalSnapshot& snapshot);
};

} // namespace devai::strategy
