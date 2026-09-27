#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>


namespace devai::features
{

inline double multiTimeframeFeatureNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// Direction
//
// IMPORTANT:
//
// This is a descriptive mathematical state.
//
// It is NOT:
//     BUY
//     SELL
//     LONG
//     SHORT
//
// +1 = positive / upward relationship
//  0 = neutral / unavailable relationship
// -1 = negative / downward relationship
// ============================================================================

enum class FeatureDirection : std::int8_t
{
    NEGATIVE = -1,
    NEUTRAL  = 0,
    POSITIVE = 1
};


// ============================================================================
// Alignment across 1m / 5m / 15m
// ============================================================================

struct DirectionAlignment
{
    FeatureDirection one_minute{
        FeatureDirection::NEUTRAL
    };

    FeatureDirection five_minute{
        FeatureDirection::NEUTRAL
    };

    FeatureDirection fifteen_minute{
        FeatureDirection::NEUTRAL
    };


    std::size_t positive_count{0};

    std::size_t negative_count{0};

    std::size_t neutral_count{0};


    bool all_positive{false};

    bool all_negative{false};

    bool mixed{false};

    bool fully_available{false};
};


// ============================================================================
// Cross-timeframe numeric comparison
//
// Ratios are dimensionless.
//
// Example:
//
//     five_to_one = 5m ATR% / 1m ATR%
//
//     fifteen_to_five = 15m ATR% / 5m ATR%
//
//     fifteen_to_one = 15m ATR% / 1m ATR%
// ============================================================================

struct CrossTimeframeRatios
{
    double five_to_one{
        multiTimeframeFeatureNaN()
    };

    double fifteen_to_five{
        multiTimeframeFeatureNaN()
    };

    double fifteen_to_one{
        multiTimeframeFeatureNaN()
    };


    bool has_five_to_one{false};

    bool has_fifteen_to_five{false};

    bool has_fifteen_to_one{false};
};


// ============================================================================
// RSI relationships
// ============================================================================

struct CrossTimeframeRSI
{
    double one_minute{
        multiTimeframeFeatureNaN()
    };

    double five_minute{
        multiTimeframeFeatureNaN()
    };

    double fifteen_minute{
        multiTimeframeFeatureNaN()
    };


    double five_minus_one{
        multiTimeframeFeatureNaN()
    };

    double fifteen_minus_five{
        multiTimeframeFeatureNaN()
    };

    double fifteen_minus_one{
        multiTimeframeFeatureNaN()
    };


    bool has_one_minute{false};

    bool has_five_minute{false};

    bool has_fifteen_minute{false};

    bool has_five_minus_one{false};

    bool has_fifteen_minus_five{false};

    bool has_fifteen_minus_one{false};
};


// ============================================================================
// Phase 3.5 complete output
// ============================================================================

struct MultiTimeframeFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};


    // ------------------------------------------------------------------------
    // Return direction
    //
    // Based on close-to-close return:
    //
    //     > 0 -> POSITIVE
    //     < 0 -> NEGATIVE
    //     = 0 -> NEUTRAL
    // ------------------------------------------------------------------------

    DirectionAlignment return_alignment;


    // ------------------------------------------------------------------------
    // EMA trend relationship
    //
    //     EMA20 > EMA50 -> POSITIVE
    //     EMA20 < EMA50 -> NEGATIVE
    //     EMA20 = EMA50 -> NEUTRAL
    // ------------------------------------------------------------------------

    DirectionAlignment ema_alignment;


    // ------------------------------------------------------------------------
    // EMA20 slope relationship
    // ------------------------------------------------------------------------

    DirectionAlignment ema20_slope_alignment;


    // ------------------------------------------------------------------------
    // RSI relative to neutral 50
    //
    //     RSI > 50 -> POSITIVE
    //     RSI < 50 -> NEGATIVE
    //     RSI = 50 -> NEUTRAL
    //
    // This is descriptive only.
    // ------------------------------------------------------------------------

    DirectionAlignment rsi_alignment;


    // ------------------------------------------------------------------------
    // RVOL20 cross-timeframe relationships
    // ------------------------------------------------------------------------

    CrossTimeframeRatios rvol20_ratios;


    // ------------------------------------------------------------------------
    // ATR14% cross-timeframe relationships
    // ------------------------------------------------------------------------

    CrossTimeframeRatios atr14_percent_ratios;


    // ------------------------------------------------------------------------
    // Realized volatility 20 cross-timeframe relationships
    // ------------------------------------------------------------------------

    CrossTimeframeRatios realized_volatility_20_ratios;


    // ------------------------------------------------------------------------
    // RSI differences
    // ------------------------------------------------------------------------

    CrossTimeframeRSI rsi_relationship;


    // ------------------------------------------------------------------------
    // Readiness
    //
    // Core readiness requires:
    //
    //     return alignment
    //     EMA alignment
    //     RSI alignment
    //
    // for all three timeframes.
    //
    // It intentionally does NOT require every optional ratio.
    // ------------------------------------------------------------------------

    bool core_features_ready{false};
};

} // namespace devai::features