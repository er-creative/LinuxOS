#pragma once

#include "devai/statistics/MarketRegimeFeatures.hpp"
#include "devai/statistics/ReturnStatisticalFeatures.hpp"
#include "devai/statistics/StockBenchmarkStatisticalFeatures.hpp"
#include "devai/statistics/VolumeStatisticalFeatures.hpp"

#include <cstdint>
#include <string>

namespace devai::statistics
{

// ============================================================================
// Phase-4 readiness
//
// This is intentionally descriptive.
//
// It tells downstream CETE components exactly which mathematical layers are
// usable at the current decision time.
//
// It is NOT:
//     - a trading score
//     - a confidence score
//     - a BUY/SELL decision
// ============================================================================

struct MathematicalReadinessState
{
    // ------------------------------------------------------------------------
    // Individual Phase-4 components
    // ------------------------------------------------------------------------

    bool return_statistics_ready{false};          // Phase 4.2
    bool volume_statistics_ready{false};          // Phase 4.3
    bool stock_benchmark_statistics_ready{false}; // Phase 4.4
    bool market_regime_ready{false};              // Phase 4.5


    // ------------------------------------------------------------------------
    // Numerical integrity
    // ------------------------------------------------------------------------

    bool numerical_integrity{false};


    // ------------------------------------------------------------------------
    // Complete horizon readiness
    //
    // True only when every required mathematical/statistical component for
    // this timeframe is ready and numerically valid.
    // ------------------------------------------------------------------------

    bool fully_ready{false};
};


// ============================================================================
// One timeframe's final Phase-4 observation
//
// IMPORTANT:
//
// This structure deliberately keeps the original Phase 4.2-4.5 structures
// intact.
//
// No information is recalculated here.
// No information is compressed into a score.
// No directional trading action is generated.
//
// Phase 4.1 rolling-statistical evidence is preserved transitively inside
// Phase 4.2, Phase 4.3 and Phase 4.4 outputs.
// ============================================================================

struct MathematicalHorizonState
{
    // ------------------------------------------------------------------------
    // Phase 4.2
    // ------------------------------------------------------------------------

    ReturnHorizonStatistics
        return_statistics;


    // ------------------------------------------------------------------------
    // Phase 4.3
    // ------------------------------------------------------------------------

    VolumeHorizonStatistics
        volume_statistics;


    // ------------------------------------------------------------------------
    // Phase 4.4
    // ------------------------------------------------------------------------

    StockBenchmarkHorizonStatistics
        stock_benchmark_statistics;


    // ------------------------------------------------------------------------
    // Phase 4.5
    // ------------------------------------------------------------------------

    MarketRegimeHorizon
        market_regime;


    // ------------------------------------------------------------------------
    // Final readiness for this timeframe
    // ------------------------------------------------------------------------

    MathematicalReadinessState
        readiness;
};


// ============================================================================
// Final Phase-4 observation
//
// This is the single object exported by Phase 4.
//
// Phase 5 and later DevAI-facing composition can consume this object instead
// of independently wiring Phase 4.2, 4.3, 4.4 and 4.5.
//
// Phase 4.1 is represented by the rolling-statistical evidence embedded in
// those higher-level Phase-4 outputs.
// ============================================================================

struct MathematicalStateFeatures
{
    // ------------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------------

    std::string stock_symbol;
    std::string benchmark_symbol;

    std::int64_t decision_time{0};


    // ------------------------------------------------------------------------
    // Multi-timeframe mathematical observation
    // ------------------------------------------------------------------------

    MathematicalHorizonState
        one_minute;

    MathematicalHorizonState
        five_minute;

    MathematicalHorizonState
        fifteen_minute;


    // ------------------------------------------------------------------------
    // Per-timeframe readiness
    // ------------------------------------------------------------------------

    bool one_minute_ready{false};
    bool five_minute_ready{false};
    bool fifteen_minute_ready{false};


    // ------------------------------------------------------------------------
    // Global numerical integrity
    //
    // True only when every available numerical value used by the final
    // Phase-4 state is finite and structurally valid.
    // ------------------------------------------------------------------------

    bool numerical_integrity{false};


    // ------------------------------------------------------------------------
    // Complete Phase-4 readiness
    //
    // True only when all three timeframes are completely ready and numerical
    // integrity has passed.
    // ------------------------------------------------------------------------

    bool fully_ready{false};
};

} // namespace devai::statistics