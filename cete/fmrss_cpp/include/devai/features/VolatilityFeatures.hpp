#pragma once

#include <cstdint>
#include <limits>
#include <string>


namespace devai::features
{

inline double volatilityFeatureNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// One timeframe's volatility state
// ============================================================================

struct TimeframeVolatilityFeatures
{
    // ------------------------------------------------------------------------
    // Current-bar range
    // ------------------------------------------------------------------------

    double true_range{
        volatilityFeatureNaN()
    };

    double true_range_percent{
        volatilityFeatureNaN()
    };


    // ------------------------------------------------------------------------
    // Wilder ATR(14)
    // ------------------------------------------------------------------------

    double atr14{
        volatilityFeatureNaN()
    };

    double atr14_percent{
        volatilityFeatureNaN()
    };


    // ------------------------------------------------------------------------
    // Rolling close-to-close return standard deviation
    //
    // Simple returns:
    //
    //     r(t) = close(t) / close(t-1) - 1
    // ------------------------------------------------------------------------

    double return_stddev_20{
        volatilityFeatureNaN()
    };

    double return_stddev_50{
        volatilityFeatureNaN()
    };


    // ------------------------------------------------------------------------
    // Rolling realized volatility
    //
    // Log returns:
    //
    //     ln(close(t) / close(t-1))
    //
    // These values are intentionally NOT annualized.
    // ------------------------------------------------------------------------

    double realized_volatility_20{
        volatilityFeatureNaN()
    };

    double realized_volatility_50{
        volatilityFeatureNaN()
    };


    // ------------------------------------------------------------------------
    // Availability
    // ------------------------------------------------------------------------

    bool has_true_range{false};
    bool has_true_range_percent{false};

    bool has_atr14{false};
    bool has_atr14_percent{false};

    bool has_return_stddev_20{false};
    bool has_return_stddev_50{false};

    bool has_realized_volatility_20{false};
    bool has_realized_volatility_50{false};
};


// ============================================================================
// Complete Phase 3.4 output
// ============================================================================

struct VolatilityFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};


    TimeframeVolatilityFeatures one_minute;

    TimeframeVolatilityFeatures five_minute;

    TimeframeVolatilityFeatures fifteen_minute;
};

} // namespace devai::features