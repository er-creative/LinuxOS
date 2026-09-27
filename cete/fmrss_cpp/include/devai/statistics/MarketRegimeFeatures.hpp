#pragma once

#include <cstdint>
#include <limits>
#include <string>


namespace devai::statistics
{

inline double marketRegimeNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// Trend state
// ============================================================================

enum class TrendState
{
    UNAVAILABLE,

    STRONG_DOWNTREND,
    DOWNTREND,

    NEUTRAL,

    UPTREND,
    STRONG_UPTREND
};


// ============================================================================
// Volatility state
// ============================================================================

enum class VolatilityState
{
    UNAVAILABLE,

    LOW,
    NORMAL,
    HIGH,
    EXTREME
};


// ============================================================================
// Statistical stability
//
// STABLE:
//     return/residual behaviour is statistically ordinary.
//
// ELEVATED:
//     noticeable statistical displacement exists.
//
// UNSTABLE:
//     extreme return or stock/NIFTY residual behaviour exists.
// ============================================================================

enum class StatisticalStability
{
    UNAVAILABLE,

    STABLE,
    ELEVATED,
    UNSTABLE
};


// ============================================================================
// Final descriptive market regime
//
// This is NOT a trading signal.
// ============================================================================

enum class MarketRegime
{
    UNAVAILABLE,

    TRENDING_UP,
    TRENDING_DOWN,

    RANGE_BOUND,

    HIGH_VOLATILITY,
    UNSTABLE
};


// ============================================================================
// One timeframe
// ============================================================================

struct MarketRegimeHorizon
{
    TrendState trend_state{
        TrendState::UNAVAILABLE
    };

    VolatilityState volatility_state{
        VolatilityState::UNAVAILABLE
    };

    StatisticalStability statistical_stability{
        StatisticalStability::UNAVAILABLE
    };

    MarketRegime regime{
        MarketRegime::UNAVAILABLE
    };


    // ------------------------------------------------------------------------
    // Descriptive numerical evidence
    //
    // trend_score:
    //     -4 ... +4
    //
    // Negative = bearish structure.
    // Positive = bullish structure.
    //
    // This is descriptive evidence, NOT a trading score.
    // ------------------------------------------------------------------------

    int trend_score{0};


    // ------------------------------------------------------------------------
    // Values used by classification
    // ------------------------------------------------------------------------

    double atr14_percent{
        marketRegimeNaN()
    };

    double realized_volatility_20{
        marketRegimeNaN()
    };

    double return_z_score{
        marketRegimeNaN()
    };

    double absolute_return_z_score{
        marketRegimeNaN()
    };

    double residual_z_score{
        marketRegimeNaN()
    };

    double rolling_correlation{
        marketRegimeNaN()
    };


    // ------------------------------------------------------------------------
    // Readiness
    // ------------------------------------------------------------------------

    bool trend_ready{false};

    bool volatility_ready{false};

    bool statistical_stability_ready{false};

    bool regime_ready{false};
};


// ============================================================================
// Complete Phase 4.5 output
// ============================================================================

struct MarketRegimeFeatures
{
    std::string stock_symbol;

    std::string benchmark_symbol;

    std::int64_t decision_time{0};


    MarketRegimeHorizon one_minute;

    MarketRegimeHorizon five_minute;

    MarketRegimeHorizon fifteen_minute;


    bool one_minute_ready{false};

    bool five_minute_ready{false};

    bool fifteen_minute_ready{false};

    bool fully_ready{false};
};

} // namespace devai::statistics