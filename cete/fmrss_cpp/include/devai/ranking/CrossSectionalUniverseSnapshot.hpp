#pragma once

#include "devai/ranking/CrossSectionalMathematicalState.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>


namespace devai::ranking
{

// ============================================================================
// Metric-specific universe sizes for one timeframe.
//
// Different mathematical metrics can legitimately have different eligible
// universe sizes.
//
// Example:
//
//     return_z_score universe       = 80
//     residual_z_score universe     = 78
//     rvol20_z_score universe       = 76
//
// Phase 5.4 preserves that information explicitly.
// ============================================================================

struct CrossSectionalMetricUniverseSizes
{
    std::size_t return_value{0};
    std::size_t return_z_score{0};
    std::size_t return_robust_z_score{0};

    std::size_t volume_z_score{0};
    std::size_t rvol20_z_score{0};

    std::size_t relative_return{0};

    std::size_t residual_return{0};
    std::size_t residual_z_score{0};
    std::size_t residual_robust_z_score{0};

    std::size_t atr14_percent{0};
    std::size_t realized_volatility_20{0};

    std::size_t rolling_beta{0};
    std::size_t rolling_correlation{0};
};


// ============================================================================
// Complete Phase-5.4 universe snapshot.
//
// One object represents the complete cross-sectional state for one legal
// decision time.
//
// stocks is deterministically ordered by stock symbol.
//
// IMPORTANT:
//
// NIFTY is benchmark/context only and must never appear in stocks.
// ============================================================================

struct CrossSectionalUniverseSnapshot
{
    std::int64_t decision_time{0};

    std::string benchmark_symbol;


    std::vector<CrossSectionalMathematicalState>
        stocks;


    CrossSectionalMetricUniverseSizes
        one_minute_universe_sizes;

    CrossSectionalMetricUniverseSizes
        five_minute_universe_sizes;

    CrossSectionalMetricUniverseSizes
        fifteen_minute_universe_sizes;


    std::size_t stock_count{0};

    std::size_t fully_ready_stock_count{0};

    std::size_t partially_ready_stock_count{0};


    bool numerical_integrity{false};

    bool fully_ready{false};
};

} // namespace devai::ranking