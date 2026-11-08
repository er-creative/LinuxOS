#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "devai/ranking/CrossSectionalUniverseSnapshotBuilder.hpp"

namespace devai::strategy
{

// ============================================================================
// DevEpisodeConfig
//
// Defines the structural requirements of one Dev historical episode.
//
// This class does NOT:
//   - calculate CETE features
//   - calculate rankings
//   - execute trades
//   - calculate brokerage
//   - calculate slippage
//   - calculate P&L
//   - calculate reward
//
// Those responsibilities belong to the already-defined CETE / DevAI
// components.
// ============================================================================

struct DevEpisodeConfig
{
    std::size_t minimum_universe_size{2};
};


// ============================================================================
// DevEpisodeState
// ============================================================================

struct DevEpisodeState
{
    std::size_t step_index{0};

    std::size_t total_steps{0};

    std::int64_t start_decision_time{0};

    std::int64_t end_decision_time{0};

    std::int64_t current_decision_time{0};

    bool initialized{false};

    bool terminal{false};
};


// ============================================================================
// DevEpisode
//
// Owns one already-generated chronological sequence of CETE Phase-5
// universe snapshots.
//
// Important:
//
//     DevEpisode NEVER constructs CETE state.
//
// HistoricalCETEReplay is responsible for producing valid historical
// CrossSectionalUniverseSnapshot objects.
//
// DevEpisode only controls:
//
//     start
//     current step
//     chronological advancement
//     terminal state
//     reset
//
// The public API exposes only the CURRENT snapshot. Future snapshots remain
// internal to the episode and are not exposed through the current-state API.
// ============================================================================

class DevEpisode
{
public:

    using Snapshot =
        devai::ranking::CrossSectionalUniverseSnapshot;


    explicit DevEpisode(
        std::vector<Snapshot> snapshots,
        DevEpisodeConfig config = {});


    // ------------------------------------------------------------------------
    // Episode lifecycle
    // ------------------------------------------------------------------------

    void reset();

    void advance();


    // ------------------------------------------------------------------------
    // Current state
    // ------------------------------------------------------------------------

    const Snapshot& currentSnapshot() const;

    std::int64_t currentDecisionTime() const;

    std::size_t stepIndex() const noexcept;

    std::size_t totalSteps() const noexcept;

    bool isInitialized() const noexcept;

    bool isTerminal() const noexcept;


    // ------------------------------------------------------------------------
    // Episode boundaries
    // ------------------------------------------------------------------------

    std::int64_t startDecisionTime() const noexcept;

    std::int64_t endDecisionTime() const noexcept;

    const std::string& benchmarkSymbol() const noexcept;


    // ------------------------------------------------------------------------
    // State summary
    // ------------------------------------------------------------------------

    DevEpisodeState state() const noexcept;


private:

    DevEpisodeConfig config_;

    std::vector<Snapshot> snapshots_;

    std::size_t current_index_{0};

    std::string benchmark_symbol_;

    bool initialized_{false};


    void validateSnapshots() const;
};

} // namespace devai::strategy