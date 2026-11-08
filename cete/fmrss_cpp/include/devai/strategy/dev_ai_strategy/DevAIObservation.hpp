#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>


namespace devai::strategy
{

// ============================================================================
// DevAI Observation Contract — Version 1
//
// MARKET FEATURES
//
// For each CETE timeframe:
//
//   1m
//   5m
//   15m
//
// Dev observes the centered cross-sectional percentile for:
//
//   01 Return Value
//   02 Return Z-Score
//   03 Return Robust Z-Score
//   04 Volume Z-Score
//   05 RVOL20 Z-Score
//   06 Relative Return
//   07 Residual Return
//   08 Residual Z-Score
//   09 Residual Robust Z-Score
//   10 ATR14 %
//   11 Realized Volatility 20
//   12 Rolling Beta
//   13 Rolling Correlation
//
// Centered percentile is already bounded:
//
//      [-1, +1]
//
// Availability is represented separately by a binary mask.
//
// FORBIDDEN:
//
//   Forward return
//   Future price
//   Future reward
//   Future P&L
//   Future CETE state
//
// ============================================================================

inline constexpr std::size_t DEV_METRICS_PER_TIMEFRAME = 13;
inline constexpr std::size_t DEV_TIMEFRAME_COUNT = 3;

inline constexpr std::size_t DEV_MARKET_FEATURE_COUNT =
    DEV_METRICS_PER_TIMEFRAME *
    DEV_TIMEFRAME_COUNT;                 // 39

inline constexpr std::size_t DEV_AVAILABILITY_COUNT =
    DEV_MARKET_FEATURE_COUNT;            // 39

inline constexpr std::size_t DEV_PORTFOLIO_FEATURE_COUNT =
    6;

inline constexpr std::size_t DEV_OBSERVATION_SIZE =
    DEV_MARKET_FEATURE_COUNT +
    DEV_AVAILABILITY_COUNT +
    DEV_PORTFOLIO_FEATURE_COUNT;         // 84


enum class DevMetric : std::size_t
{
    RETURN_VALUE = 0,
    RETURN_Z_SCORE,
    RETURN_ROBUST_Z_SCORE,
    VOLUME_Z_SCORE,
    RVOL20_Z_SCORE,
    RELATIVE_RETURN,
    RESIDUAL_RETURN,
    RESIDUAL_Z_SCORE,
    RESIDUAL_ROBUST_Z_SCORE,
    ATR14_PERCENT,
    REALIZED_VOLATILITY_20,
    ROLLING_BETA,
    ROLLING_CORRELATION
};


enum class DevTimeframe : std::size_t
{
    ONE_MINUTE = 0,
    FIVE_MINUTE = 1,
    FIFTEEN_MINUTE = 2
};


struct DevPortfolioState
{
    // -1 = short
    //  0 = flat
    // +1 = long
    double position{0.0};

    // Return relative to current position entry.
    double unrealized_return{0.0};

    // Episode cumulative realized return.
    double realized_return{0.0};

    // Current equity / initial equity - 1.
    double equity_return{0.0};

    // Current drawdown in [0, 1].
    double drawdown{0.0};

    // Number of bars current position has been held,
    // normalized later by ObservationNormalizer.
    double holding_period{0.0};
};


struct DevAIObservation
{
    std::string agent_name{"Dev"};

    std::string symbol;
    std::string benchmark_symbol;

    std::int64_t decision_time{0};

    // 39 centered cross-sectional percentiles.
    std::array<double, DEV_MARKET_FEATURE_COUNT>
        market{};

    // 39 binary availability indicators.
    //
    // 1.0 = feature legally available
    // 0.0 = unavailable
    std::array<double, DEV_AVAILABILITY_COUNT>
        availability{};

    DevPortfolioState portfolio;

    bool numerical_integrity{false};
    bool ready{false};
};


// ============================================================================
// Deterministic feature index
// ============================================================================

constexpr std::size_t marketIndex(
    DevTimeframe timeframe,
    DevMetric metric) noexcept
{
    return
        static_cast<std::size_t>(timeframe) *
            DEV_METRICS_PER_TIMEFRAME +
        static_cast<std::size_t>(metric);
}

} // namespace devai::strategy