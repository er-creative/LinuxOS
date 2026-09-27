#pragma once

#include "devai/ranking/CrossSectionalMathematicalFeatures.hpp"

#include <cstddef>
#include <cstdint>
#include <string>


namespace devai::ranking
{

// ============================================================================
// Metric readiness
//
// A metric is considered ready when Phase 5.2 produced a valid
// cross-sectional rank for the stock.
//
// universe_size is preserved explicitly because different mathematical
// metrics may have different eligible universes.
// ============================================================================

struct CrossSectionalMetricReadiness
{
    bool available{false};

    std::size_t universe_size{0};
};


// ============================================================================
// One-timeframe readiness
//
// These flags correspond exactly to the 13 independent Phase-5.2 metrics.
//
// IMPORTANT:
//
// No weighting or composite score is calculated here.
// ============================================================================

struct CrossSectionalHorizonReadiness
{
    // ------------------------------------------------------------------------
    // Return
    // ------------------------------------------------------------------------

    CrossSectionalMetricReadiness
        return_value;

    CrossSectionalMetricReadiness
        return_z_score;

    CrossSectionalMetricReadiness
        return_robust_z_score;


    // ------------------------------------------------------------------------
    // Volume
    // ------------------------------------------------------------------------

    CrossSectionalMetricReadiness
        volume_z_score;

    CrossSectionalMetricReadiness
        rvol20_z_score;


    // ------------------------------------------------------------------------
    // Stock / benchmark relationship
    // ------------------------------------------------------------------------

    CrossSectionalMetricReadiness
        relative_return;

    CrossSectionalMetricReadiness
        residual_return;

    CrossSectionalMetricReadiness
        residual_z_score;

    CrossSectionalMetricReadiness
        residual_robust_z_score;


    // ------------------------------------------------------------------------
    // Volatility
    // ------------------------------------------------------------------------

    CrossSectionalMetricReadiness
        atr14_percent;

    CrossSectionalMetricReadiness
        realized_volatility_20;


    // ------------------------------------------------------------------------
    // Regression / relationship
    // ------------------------------------------------------------------------

    CrossSectionalMetricReadiness
        rolling_beta;

    CrossSectionalMetricReadiness
        rolling_correlation;


    // ------------------------------------------------------------------------
    // Horizon-level state
    // ------------------------------------------------------------------------

    std::size_t available_metric_count{0};

    std::size_t total_metric_count{13};

    bool numerical_integrity{false};

    bool fully_ready{false};
};


// ============================================================================
// One timeframe of the final Phase-5.3 state.
//
// rankings is copied losslessly from Phase 5.2.
// readiness describes those exact rankings.
// ============================================================================

struct CrossSectionalHorizonState
{
    CrossSectionalMathematicalHorizon
        rankings;

    CrossSectionalHorizonReadiness
        readiness;
};


// ============================================================================
// Complete Phase-5.3 state for one stock.
//
// This is a per-stock multi-timeframe cross-sectional state.
//
// Phase 5.4 will later package multiple instances of this structure into one
// complete cross-sectional universe snapshot.
// ============================================================================

struct CrossSectionalMathematicalState
{
    std::string symbol;

    std::string benchmark_symbol;

    std::int64_t decision_time{0};


    CrossSectionalHorizonState
        one_minute;

    CrossSectionalHorizonState
        five_minute;

    CrossSectionalHorizonState
        fifteen_minute;


    bool one_minute_ready{false};

    bool five_minute_ready{false};

    bool fifteen_minute_ready{false};


    bool numerical_integrity{false};

    bool fully_ready{false};
};

} // namespace devai::ranking