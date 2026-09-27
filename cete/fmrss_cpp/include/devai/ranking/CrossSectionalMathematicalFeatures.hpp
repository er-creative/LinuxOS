#pragma once

#include "devai/ranking/CrossSectionalRank.hpp"

#include <cstdint>
#include <string>


namespace devai::ranking
{

// ============================================================================
// Cross-sectional mathematical rankings for one timeframe.
//
// IMPORTANT:
//
// These are independent mathematical observations.
//
// No metric is weighted.
// No composite/master score is calculated.
// No metric implies BUY or SELL.
// ============================================================================

struct CrossSectionalMathematicalHorizon
{
    // ------------------------------------------------------------------------
    // Return
    // ------------------------------------------------------------------------

    CrossSectionalRank return_value;
    CrossSectionalRank return_z_score;
    CrossSectionalRank return_robust_z_score;


    // ------------------------------------------------------------------------
    // Volume
    // ------------------------------------------------------------------------

    CrossSectionalRank volume_z_score;
    CrossSectionalRank rvol20_z_score;


    // ------------------------------------------------------------------------
    // Stock / benchmark relationship
    // ------------------------------------------------------------------------

    CrossSectionalRank relative_return;

    CrossSectionalRank residual_return;
    CrossSectionalRank residual_z_score;
    CrossSectionalRank residual_robust_z_score;

    CrossSectionalRank rolling_beta;
    CrossSectionalRank rolling_correlation;


    // ------------------------------------------------------------------------
    // Volatility
    // ------------------------------------------------------------------------

    CrossSectionalRank atr14_percent;
    CrossSectionalRank realized_volatility_20;
};


// ============================================================================
// Complete Phase-5.2 output for one stock.
//
// Phase 5.2 deliberately keeps all three timeframes together so the next
// Phase 5.3 composer can build formal multi-timeframe readiness/state.
// ============================================================================

struct CrossSectionalMathematicalFeatures
{
    std::string symbol;

    std::string benchmark_symbol;

    std::int64_t decision_time{0};


    CrossSectionalMathematicalHorizon
        one_minute;

    CrossSectionalMathematicalHorizon
        five_minute;

    CrossSectionalMathematicalHorizon
        fifteen_minute;
};

} // namespace devai::ranking