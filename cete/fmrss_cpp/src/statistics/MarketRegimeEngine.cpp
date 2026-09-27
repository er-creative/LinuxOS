#include "devai/statistics/MarketRegimeEngine.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>


namespace devai::statistics
{

// ============================================================================
// Constructor
// ============================================================================

MarketRegimeEngine::MarketRegimeEngine(
    std::string stock_symbol,
    std::string benchmark_symbol,
    double elevated_z_threshold,
    double extreme_z_threshold)
    :
    stock_symbol_(
        std::move(stock_symbol)
    ),
    benchmark_symbol_(
        std::move(benchmark_symbol)
    ),
    elevated_z_threshold_(
        elevated_z_threshold
    ),
    extreme_z_threshold_(
        extreme_z_threshold
    )
{
    if (stock_symbol_.empty())
    {
        throw std::invalid_argument(
            "MarketRegimeEngine stock symbol cannot be empty."
        );
    }


    if (benchmark_symbol_.empty())
    {
        throw std::invalid_argument(
            "MarketRegimeEngine benchmark symbol cannot be empty."
        );
    }


    if (stock_symbol_ ==
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine stock and benchmark symbols must differ."
        );
    }


    if (!std::isfinite(
            elevated_z_threshold_) ||
        elevated_z_threshold_ <= 0.0)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine elevated Z threshold must be positive."
        );
    }


    if (!std::isfinite(
            extreme_z_threshold_) ||
        extreme_z_threshold_ <=
            elevated_z_threshold_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine extreme Z threshold must be greater "
            "than elevated Z threshold."
        );
    }
}


// ============================================================================
// Finite helper
// ============================================================================

bool MarketRegimeEngine::finite(
    double value) noexcept
{
    return
        std::isfinite(value);
}


// ============================================================================
// Input validation
// ============================================================================

void MarketRegimeEngine::validateInputs(
    const devai::features::TrendMomentumFeatures&
        trend_features,

    const devai::features::VolatilityFeatures&
        volatility_features,

    const ReturnStatisticalFeatures&
        return_statistics,

    const StockBenchmarkStatisticalFeatures&
        stock_benchmark_statistics) const
{
    // ------------------------------------------------------------------------
    // Symbol integrity
    // ------------------------------------------------------------------------

    if (trend_features.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine trend symbol does not match stock."
        );
    }


    if (volatility_features.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine volatility symbol does not match stock."
        );
    }


    if (return_statistics.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine return-statistics symbol does not "
            "match stock."
        );
    }


    if (stock_benchmark_statistics.stock_symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine Phase 4.4 stock symbol does not match."
        );
    }


    if (stock_benchmark_statistics.benchmark_symbol !=
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine Phase 4.4 benchmark symbol does not match."
        );
    }


    // ------------------------------------------------------------------------
    // Decision-time integrity
    // ------------------------------------------------------------------------

    const std::int64_t decision_time =
        trend_features.decision_time;


    if (volatility_features.decision_time !=
            decision_time ||
        return_statistics.decision_time !=
            decision_time ||
        stock_benchmark_statistics.decision_time !=
            decision_time)
    {
        throw std::invalid_argument(
            "MarketRegimeEngine input decision times are not aligned."
        );
    }
}


// ============================================================================
// Trend classification
//
// Evidence:
//
//     EMA20 > EMA50       +1
//     EMA20 < EMA50       -1
//
//     Price > EMA20       +1
//     Price < EMA20       -1
//
//     EMA20 slope > 0     +1
//     EMA20 slope < 0     -1
//
//     EMA50 slope > 0     +1
//     EMA50 slope < 0     -1
//
// Score:
//
//     +3 / +4 = STRONG_UPTREND
//     +1 / +2 = UPTREND
//      0      = NEUTRAL
//     -1 / -2 = DOWNTREND
//     -3 / -4 = STRONG_DOWNTREND
//
// RSI is deliberately NOT used to define trend direction.
// RSI remains momentum context and can become overbought/oversold during a
// valid trend.
// ============================================================================

TrendState MarketRegimeEngine::classifyTrend(
    const devai::features::TimeframeTrendMomentumFeatures&
        trend,
    int& trend_score) const
{
    trend_score =
        0;


    const bool ready =
        trend.has_ema20 &&
        trend.has_ema50 &&
        trend.has_ema20_slope &&
        trend.has_ema50_slope &&
        finite(trend.ema20) &&
        finite(trend.ema50) &&
        finite(trend.price_vs_ema20) &&
        finite(trend.ema20_vs_ema50) &&
        finite(trend.ema20_slope) &&
        finite(trend.ema50_slope);


    if (!ready)
    {
        return
            TrendState::UNAVAILABLE;
    }


    // ------------------------------------------------------------------------
    // EMA relationship
    // ------------------------------------------------------------------------

    if (trend.ema20_vs_ema50 > 0.0)
    {
        ++trend_score;
    }
    else if (trend.ema20_vs_ema50 < 0.0)
    {
        --trend_score;
    }


    // ------------------------------------------------------------------------
    // Price relative to EMA20
    // ------------------------------------------------------------------------

    if (trend.price_vs_ema20 > 0.0)
    {
        ++trend_score;
    }
    else if (trend.price_vs_ema20 < 0.0)
    {
        --trend_score;
    }


    // ------------------------------------------------------------------------
    // EMA20 slope
    // ------------------------------------------------------------------------

    if (trend.ema20_slope > 0.0)
    {
        ++trend_score;
    }
    else if (trend.ema20_slope < 0.0)
    {
        --trend_score;
    }


    // ------------------------------------------------------------------------
    // EMA50 slope
    // ------------------------------------------------------------------------

    if (trend.ema50_slope > 0.0)
    {
        ++trend_score;
    }
    else if (trend.ema50_slope < 0.0)
    {
        --trend_score;
    }


    if (trend_score >= 3)
    {
        return
            TrendState::STRONG_UPTREND;
    }


    if (trend_score >= 1)
    {
        return
            TrendState::UPTREND;
    }


    if (trend_score <= -3)
    {
        return
            TrendState::STRONG_DOWNTREND;
    }


    if (trend_score <= -1)
    {
        return
            TrendState::DOWNTREND;
    }


    return
        TrendState::NEUTRAL;
}


// ============================================================================
// Volatility classification
//
// We deliberately avoid fixed absolute ATR thresholds.
//
// Different instruments naturally have different volatility scales.
//
// Phase 4.2 already gives a standardized current absolute-return displacement:
//
//     absolute_return_z_score
//
// ATR14% and realized volatility are required as structural volatility
// evidence/readiness, while the standardized absolute-return Z-score provides
// the regime classification.
//
//     Z <= -1          LOW
//     -1 < Z < +1     NORMAL
//     +1 <= Z < +2    HIGH
//     Z >= +2         EXTREME
//
// Absolute-return Z can legitimately be negative:
// it means current absolute movement is below its rolling average.
// ============================================================================

VolatilityState MarketRegimeEngine::classifyVolatility(
    const devai::features::TimeframeVolatilityFeatures&
        volatility,

    const ReturnHorizonStatistics&
        return_statistics) const
{
    const bool volatility_ready =
        volatility.has_atr14_percent &&
        volatility.has_realized_volatility_20 &&
        finite(volatility.atr14_percent) &&
        finite(volatility.realized_volatility_20);


    const bool statistics_ready =
        return_statistics.ready &&
        return_statistics.has_observation &&
        finite(
            return_statistics.absolute_return_z_score);


    if (!volatility_ready ||
        !statistics_ready)
    {
        return
            VolatilityState::UNAVAILABLE;
    }


    const double z =
        return_statistics.absolute_return_z_score;


    if (z >=
        extreme_z_threshold_)
    {
        return
            VolatilityState::EXTREME;
    }


    if (z >=
        elevated_z_threshold_)
    {
        return
            VolatilityState::HIGH;
    }


    if (z <=
        -elevated_z_threshold_)
    {
        return
            VolatilityState::LOW;
    }


    return
        VolatilityState::NORMAL;
}


// ============================================================================
// Statistical stability
//
// This uses BOTH:
//
//     Phase 4.2 stock-return statistics
//     Phase 4.4 stock-vs-NIFTY residual statistics
//
// UNSTABLE:
//     either ordinary return or residual is >= extreme threshold.
//
// ELEVATED:
//     either is >= elevated threshold.
//
// STABLE:
//     both remain inside elevated threshold.
//
// Robust Z-scores are also considered. This prevents one unusual observation
// from being hidden merely because ordinary standard deviation was distorted.
// ============================================================================

StatisticalStability
MarketRegimeEngine::classifyStability(
    const ReturnHorizonStatistics&
        return_statistics,

    const StockBenchmarkHorizonStatistics&
        stock_benchmark_statistics) const
{
    const bool return_ready =
        return_statistics.ready &&
        return_statistics.has_observation &&
        finite(return_statistics.z_score) &&
        finite(return_statistics.robust_z_score);


    const bool residual_ready =
        stock_benchmark_statistics.
            residual_statistics_ready &&
        stock_benchmark_statistics.
            regression_ready &&
        finite(
            stock_benchmark_statistics.
                residual_z_score) &&
        finite(
            stock_benchmark_statistics.
                residual_robust_z_score);


    if (!return_ready ||
        !residual_ready)
    {
        return
            StatisticalStability::UNAVAILABLE;
    }


    const double maximum_return_displacement =
        std::max(
            std::fabs(
                return_statistics.z_score),
            std::fabs(
                return_statistics.robust_z_score));


    const double maximum_residual_displacement =
        std::max(
            std::fabs(
                stock_benchmark_statistics.
                    residual_z_score),
            std::fabs(
                stock_benchmark_statistics.
                    residual_robust_z_score));


    const double maximum_displacement =
        std::max(
            maximum_return_displacement,
            maximum_residual_displacement);


    if (maximum_displacement >=
        extreme_z_threshold_)
    {
        return
            StatisticalStability::UNSTABLE;
    }


    if (maximum_displacement >=
        elevated_z_threshold_)
    {
        return
            StatisticalStability::ELEVATED;
    }


    return
        StatisticalStability::STABLE;
}


// ============================================================================
// Final market-regime classification
//
// Priority is deliberate:
//
// 1. Statistical instability
// 2. Extreme/high volatility
// 3. Trend
// 4. Range-bound
//
// A statistically extreme market is therefore not hidden by an EMA trend.
//
// HIGH_VOLATILITY is descriptive. It does not mean BUY or SELL.
// ============================================================================

MarketRegime MarketRegimeEngine::classifyRegime(
    TrendState trend_state,
    VolatilityState volatility_state,
    StatisticalStability stability) const
{
    if (trend_state ==
            TrendState::UNAVAILABLE ||
        volatility_state ==
            VolatilityState::UNAVAILABLE ||
        stability ==
            StatisticalStability::UNAVAILABLE)
    {
        return
            MarketRegime::UNAVAILABLE;
    }


    if (stability ==
        StatisticalStability::UNSTABLE)
    {
        return
            MarketRegime::UNSTABLE;
    }


    if (volatility_state ==
            VolatilityState::EXTREME ||
        volatility_state ==
            VolatilityState::HIGH)
    {
        return
            MarketRegime::HIGH_VOLATILITY;
    }


    if (trend_state ==
            TrendState::STRONG_UPTREND ||
        trend_state ==
            TrendState::UPTREND)
    {
        return
            MarketRegime::TRENDING_UP;
    }


    if (trend_state ==
            TrendState::STRONG_DOWNTREND ||
        trend_state ==
            TrendState::DOWNTREND)
    {
        return
            MarketRegime::TRENDING_DOWN;
    }


    return
        MarketRegime::RANGE_BOUND;
}


// ============================================================================
// One timeframe
// ============================================================================

MarketRegimeHorizon
MarketRegimeEngine::classifyHorizon(
    const devai::features::TimeframeTrendMomentumFeatures&
        trend,

    const devai::features::TimeframeVolatilityFeatures&
        volatility,

    const ReturnHorizonStatistics&
        return_statistics,

    const StockBenchmarkHorizonStatistics&
        stock_benchmark_statistics) const
{
    MarketRegimeHorizon output;


    // ------------------------------------------------------------------------
    // Preserve numerical evidence
    // ------------------------------------------------------------------------

    if (volatility.has_atr14_percent &&
        finite(volatility.atr14_percent))
    {
        output.atr14_percent =
            volatility.atr14_percent;
    }


    if (volatility.has_realized_volatility_20 &&
        finite(
            volatility.realized_volatility_20))
    {
        output.realized_volatility_20 =
            volatility.realized_volatility_20;
    }


    if (return_statistics.has_observation &&
        finite(return_statistics.z_score))
    {
        output.return_z_score =
            return_statistics.z_score;
    }


    if (return_statistics.has_observation &&
        finite(
            return_statistics.
                absolute_return_z_score))
    {
        output.absolute_return_z_score =
            return_statistics.
                absolute_return_z_score;
    }


    if (stock_benchmark_statistics.
            regression_ready &&
        finite(
            stock_benchmark_statistics.
                residual_z_score))
    {
        output.residual_z_score =
            stock_benchmark_statistics.
                residual_z_score;
    }


    if (stock_benchmark_statistics.
            regression_ready &&
        finite(
            stock_benchmark_statistics.
                rolling_correlation))
    {
        output.rolling_correlation =
            stock_benchmark_statistics.
                rolling_correlation;
    }


    // ------------------------------------------------------------------------
    // Trend
    // ------------------------------------------------------------------------

    output.trend_state =
        classifyTrend(
            trend,
            output.trend_score);


    output.trend_ready =
        output.trend_state !=
        TrendState::UNAVAILABLE;


    // ------------------------------------------------------------------------
    // Volatility
    // ------------------------------------------------------------------------

    output.volatility_state =
        classifyVolatility(
            volatility,
            return_statistics);


    output.volatility_ready =
        output.volatility_state !=
        VolatilityState::UNAVAILABLE;


    // ------------------------------------------------------------------------
    // Statistical stability
    // ------------------------------------------------------------------------

    output.statistical_stability =
        classifyStability(
            return_statistics,
            stock_benchmark_statistics);


    output.statistical_stability_ready =
        output.statistical_stability !=
        StatisticalStability::UNAVAILABLE;


    // ------------------------------------------------------------------------
    // Final regime
    // ------------------------------------------------------------------------

    output.regime =
        classifyRegime(
            output.trend_state,
            output.volatility_state,
            output.statistical_stability);


    output.regime_ready =
        output.regime !=
        MarketRegime::UNAVAILABLE;


    return output;
}


// ============================================================================
// Main update
// ============================================================================

MarketRegimeFeatures
MarketRegimeEngine::update(
    const devai::features::TrendMomentumFeatures&
        trend_features,

    const devai::features::VolatilityFeatures&
        volatility_features,

    const ReturnStatisticalFeatures&
        return_statistics,

    const StockBenchmarkStatisticalFeatures&
        stock_benchmark_statistics) const
{
    validateInputs(
        trend_features,
        volatility_features,
        return_statistics,
        stock_benchmark_statistics);


    MarketRegimeFeatures output;


    output.stock_symbol =
        stock_symbol_;

    output.benchmark_symbol =
        benchmark_symbol_;

    output.decision_time =
        trend_features.decision_time;


    // ------------------------------------------------------------------------
    // 1 minute
    // ------------------------------------------------------------------------

    output.one_minute =
        classifyHorizon(
            trend_features.one_minute,
            volatility_features.one_minute,
            return_statistics.one_minute,
            stock_benchmark_statistics.one_minute);


    // ------------------------------------------------------------------------
    // 5 minute
    // ------------------------------------------------------------------------

    output.five_minute =
        classifyHorizon(
            trend_features.five_minute,
            volatility_features.five_minute,
            return_statistics.five_minute,
            stock_benchmark_statistics.five_minute);


    // ------------------------------------------------------------------------
    // 15 minute
    // ------------------------------------------------------------------------

    output.fifteen_minute =
        classifyHorizon(
            trend_features.fifteen_minute,
            volatility_features.fifteen_minute,
            return_statistics.fifteen_minute,
            stock_benchmark_statistics.fifteen_minute);


    // ------------------------------------------------------------------------
    // Readiness
    // ------------------------------------------------------------------------

    output.one_minute_ready =
        output.one_minute.regime_ready;

    output.five_minute_ready =
        output.five_minute.regime_ready;

    output.fifteen_minute_ready =
        output.fifteen_minute.regime_ready;


    output.fully_ready =
        output.one_minute_ready &&
        output.five_minute_ready &&
        output.fifteen_minute_ready;


    return output;
}

} // namespace devai::statistics