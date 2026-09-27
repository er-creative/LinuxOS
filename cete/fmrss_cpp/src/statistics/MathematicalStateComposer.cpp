#include "devai/statistics/MathematicalStateComposer.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::statistics
{

// ============================================================================
// Constructor
// ============================================================================

MathematicalStateComposer::MathematicalStateComposer(
    std::string stock_symbol,
    std::string benchmark_symbol)
    :
    stock_symbol_(
        std::move(stock_symbol)),

    benchmark_symbol_(
        std::move(benchmark_symbol))
{
    if (stock_symbol_.empty())
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "stock symbol cannot be empty.");
    }


    if (benchmark_symbol_.empty())
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "benchmark symbol cannot be empty.");
    }


    if (stock_symbol_ ==
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "stock and benchmark symbols must be different.");
    }
}


// ============================================================================
// Accessors
// ============================================================================

const std::string&
MathematicalStateComposer::stockSymbol() const noexcept
{
    return
        stock_symbol_;
}


const std::string&
MathematicalStateComposer::benchmarkSymbol() const noexcept
{
    return
        benchmark_symbol_;
}


// ============================================================================
// Input validation
//
// Phase 4.6 is a point-in-time composer.
//
// Every input must therefore describe:
//
//     - the expected symbol
//     - the same decision time
//
// No stale Phase-4 object is allowed to be mixed with a newer object.
// ============================================================================

void MathematicalStateComposer::validateInputs(
    const ReturnStatisticalFeatures&
        return_statistics,

    const VolumeStatisticalFeatures&
        volume_statistics,

    const StockBenchmarkStatisticalFeatures&
        stock_benchmark_statistics,

    const MarketRegimeFeatures&
        market_regime) const
{
    // ------------------------------------------------------------------------
    // Stock symbol
    // ------------------------------------------------------------------------

    if (return_statistics.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "ReturnStatisticalFeatures symbol mismatch.");
    }


    if (volume_statistics.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "VolumeStatisticalFeatures symbol mismatch.");
    }


    if (stock_benchmark_statistics.stock_symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "StockBenchmarkStatisticalFeatures stock symbol mismatch.");
    }


    if (market_regime.stock_symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "MarketRegimeFeatures stock symbol mismatch.");
    }


    // ------------------------------------------------------------------------
    // Benchmark symbol
    // ------------------------------------------------------------------------

    if (stock_benchmark_statistics.benchmark_symbol !=
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "StockBenchmarkStatisticalFeatures benchmark symbol mismatch.");
    }


    if (market_regime.benchmark_symbol !=
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "MarketRegimeFeatures benchmark symbol mismatch.");
    }


    // ------------------------------------------------------------------------
    // Decision time
    // ------------------------------------------------------------------------

    const std::int64_t decision_time =
        return_statistics.decision_time;


    if (volume_statistics.decision_time !=
            decision_time ||

        stock_benchmark_statistics.decision_time !=
            decision_time ||

        market_regime.decision_time !=
            decision_time)
    {
        throw std::invalid_argument(
            "MathematicalStateComposer: "
            "Phase-4 decision times do not match.");
    }
}


// ============================================================================
// Finite-value helper
//
// Unavailable values are permitted to remain NaN.
//
// Once a value is declared available, however, it must be finite.
// ============================================================================

bool MathematicalStateComposer::finiteIfAvailable(
    bool available,
    double value) noexcept
{
    if (!available)
    {
        return
            true;
    }


    return
        std::isfinite(value);
}


// ============================================================================
// Phase 4.2 numerical integrity
// ============================================================================

bool MathematicalStateComposer::validateReturnStatistics(
    const ReturnHorizonStatistics&
        statistics) noexcept
{
    if (!statistics.has_observation)
    {
        // No current statistical observation is a legitimate warm-up state.
        return
            true;
    }


    if (!std::isfinite(
            statistics.return_value) ||

        !std::isfinite(
            statistics.absolute_return))
    {
        return
            false;
    }


    if (statistics.absolute_return <
        0.0)
    {
        return
            false;
    }


    if (!std::isfinite(
            statistics.rolling_mean) ||

        !std::isfinite(
            statistics.rolling_standard_deviation) ||

        !std::isfinite(
            statistics.z_score) ||

        !std::isfinite(
            statistics.rolling_median) ||

        !std::isfinite(
            statistics.rolling_mad) ||

        !std::isfinite(
            statistics.robust_z_score) ||

        !std::isfinite(
            statistics.absolute_return_mean) ||

        !std::isfinite(
            statistics.absolute_return_standard_deviation) ||

        !std::isfinite(
            statistics.absolute_return_z_score) ||

        !std::isfinite(
            statistics.absolute_return_median) ||

        !std::isfinite(
            statistics.absolute_return_mad) ||

        !std::isfinite(
            statistics.absolute_return_robust_z_score))
    {
        return
            false;
    }


    if (statistics.rolling_standard_deviation <
            0.0 ||

        statistics.rolling_mad <
            0.0 ||

        statistics.absolute_return_standard_deviation <
            0.0 ||

        statistics.absolute_return_mad <
            0.0)
    {
        return
            false;
    }


    if (statistics.ready &&
        statistics.observation_count == 0)
    {
        return
            false;
    }


    return
        true;
}


// ============================================================================
// Phase 4.3 metric numerical integrity
// ============================================================================

bool MathematicalStateComposer::validateVolumeMetric(
    const VolumeMetricStatistics&
        statistics) noexcept
{
    if (!statistics.has_observation)
    {
        return
            true;
    }


    if (!std::isfinite(
            statistics.value) ||

        !std::isfinite(
            statistics.rolling_mean) ||

        !std::isfinite(
            statistics.rolling_standard_deviation) ||

        !std::isfinite(
            statistics.z_score) ||

        !std::isfinite(
            statistics.rolling_median) ||

        !std::isfinite(
            statistics.rolling_mad) ||

        !std::isfinite(
            statistics.robust_z_score))
    {
        return
            false;
    }


    if (statistics.rolling_standard_deviation <
            0.0 ||

        statistics.rolling_mad <
            0.0)
    {
        return
            false;
    }


    if (statistics.ready &&
        statistics.observation_count == 0)
    {
        return
            false;
    }


    return
        true;
}


// ============================================================================
// Phase 4.3 horizon numerical integrity
// ============================================================================

bool MathematicalStateComposer::validateVolumeStatistics(
    const VolumeHorizonStatistics&
        statistics) noexcept
{
    if (!validateVolumeMetric(
            statistics.volume))
    {
        return
            false;
    }


    if (!validateVolumeMetric(
            statistics.rvol20))
    {
        return
            false;
    }


    if (!validateVolumeMetric(
            statistics.rvol50))
    {
        return
            false;
    }


    // ------------------------------------------------------------------------
    // Readiness must agree with the underlying Phase 4.1-derived metrics.
    // ------------------------------------------------------------------------

    if (statistics.volume_ready !=
        statistics.volume.ready)
    {
        return
            false;
    }


    if (statistics.rvol20_ready !=
        statistics.rvol20.ready)
    {
        return
            false;
    }


    if (statistics.rvol50_ready !=
        statistics.rvol50.ready)
    {
        return
            false;
    }


    const bool expected_fully_ready =
        statistics.volume_ready &&
        statistics.rvol20_ready &&
        statistics.rvol50_ready;


    if (statistics.fully_ready !=
        expected_fully_ready)
    {
        return
            false;
    }


    return
        true;
}


// ============================================================================
// Phase 4.4 numerical integrity
// ============================================================================

bool MathematicalStateComposer::
validateStockBenchmarkStatistics(
    const StockBenchmarkHorizonStatistics&
        statistics) noexcept
{
    // ------------------------------------------------------------------------
    // No paired observation yet.
    // ------------------------------------------------------------------------

    if (!statistics.has_observation)
    {
        return
            true;
    }


    if (!std::isfinite(
            statistics.stock_return) ||

        !std::isfinite(
            statistics.benchmark_return) ||

        !std::isfinite(
            statistics.relative_return))
    {
        return
            false;
    }


    // ------------------------------------------------------------------------
    // Regression values become mandatory only after regression readiness.
    // ------------------------------------------------------------------------

    if (statistics.regression_ready)
    {
        if (!std::isfinite(
                statistics.rolling_beta) ||

            !std::isfinite(
                statistics.rolling_alpha) ||

            !std::isfinite(
                statistics.rolling_correlation) ||

            !std::isfinite(
                statistics.residual_return))
        {
            return
                false;
        }


        if (statistics.rolling_correlation <
                -1.000000001 ||

            statistics.rolling_correlation >
                1.000000001)
        {
            return
                false;
        }
    }


    // ------------------------------------------------------------------------
    // Residual rolling statistics become mandatory only when ready.
    // ------------------------------------------------------------------------

    if (statistics.residual_statistics_ready)
    {
        if (!std::isfinite(
                statistics.residual_rolling_mean) ||

            !std::isfinite(
                statistics.residual_rolling_standard_deviation) ||

            !std::isfinite(
                statistics.residual_z_score) ||

            !std::isfinite(
                statistics.residual_rolling_median) ||

            !std::isfinite(
                statistics.residual_rolling_mad) ||

            !std::isfinite(
                statistics.residual_robust_z_score))
        {
            return
                false;
        }


        if (statistics.residual_rolling_standard_deviation <
                0.0 ||

            statistics.residual_rolling_mad <
                0.0)
        {
            return
                false;
        }
    }


    // ------------------------------------------------------------------------
    // Structural readiness consistency
    // ------------------------------------------------------------------------

    const bool expected_fully_ready =
        statistics.has_observation &&
        statistics.regression_ready &&
        statistics.residual_statistics_ready;


    if (statistics.fully_ready !=
        expected_fully_ready)
    {
        return
            false;
    }


    return
        true;
}


// ============================================================================
// Phase 4.5 numerical / structural integrity
// ============================================================================

bool MathematicalStateComposer::validateMarketRegime(
    const MarketRegimeHorizon&
        regime) noexcept
{
    // ------------------------------------------------------------------------
    // Trend score is built from four signs.
    // ------------------------------------------------------------------------

    if (regime.trend_score < -4 ||
        regime.trend_score > 4)
    {
        return
            false;
    }


    // ------------------------------------------------------------------------
    // Readiness must agree with enum availability.
    // ------------------------------------------------------------------------

    const bool expected_trend_ready =
        regime.trend_state !=
        TrendState::UNAVAILABLE;


    const bool expected_volatility_ready =
        regime.volatility_state !=
        VolatilityState::UNAVAILABLE;


    const bool expected_stability_ready =
        regime.statistical_stability !=
        StatisticalStability::UNAVAILABLE;


    const bool expected_regime_ready =
        regime.regime !=
        MarketRegime::UNAVAILABLE;


    if (regime.trend_ready !=
            expected_trend_ready ||

        regime.volatility_ready !=
            expected_volatility_ready ||

        regime.statistical_stability_ready !=
            expected_stability_ready ||

        regime.regime_ready !=
            expected_regime_ready)
    {
        return
            false;
    }


    // ------------------------------------------------------------------------
    // Evidence is required only when its corresponding component is ready.
    // ------------------------------------------------------------------------

    if (regime.volatility_ready)
    {
        if (!std::isfinite(
                regime.atr14_percent) ||

            !std::isfinite(
                regime.realized_volatility_20) ||

            !std::isfinite(
                regime.absolute_return_z_score))
        {
            return
                false;
        }


        if (regime.atr14_percent <
                0.0 ||

            regime.realized_volatility_20 <
                0.0)
        {
            return
                false;
        }
    }


    if (regime.statistical_stability_ready)
    {
        if (!std::isfinite(
                regime.return_z_score) ||

            !std::isfinite(
                regime.residual_z_score))
        {
            return
                false;
        }
    }


    if (std::isfinite(
            regime.rolling_correlation))
    {
        if (regime.rolling_correlation <
                -1.000000001 ||

            regime.rolling_correlation >
                1.000000001)
        {
            return
                false;
        }
    }


    return
        true;
}


// ============================================================================
// Compose one timeframe
// ============================================================================

MathematicalHorizonState
MathematicalStateComposer::composeHorizon(
    const ReturnHorizonStatistics&
        return_statistics,

    const VolumeHorizonStatistics&
        volume_statistics,

    const StockBenchmarkHorizonStatistics&
        stock_benchmark_statistics,

    const MarketRegimeHorizon&
        market_regime)
{
    MathematicalHorizonState output;


    // ------------------------------------------------------------------------
    // Lossless Phase-4 composition
    // ------------------------------------------------------------------------

    output.return_statistics =
        return_statistics;


    output.volume_statistics =
        volume_statistics;


    output.stock_benchmark_statistics =
        stock_benchmark_statistics;


    output.market_regime =
        market_regime;


    // ------------------------------------------------------------------------
    // Source readiness
    // ------------------------------------------------------------------------

    output.readiness.return_statistics_ready =
        return_statistics.ready;


    output.readiness.volume_statistics_ready =
        volume_statistics.fully_ready;


    output.readiness.stock_benchmark_statistics_ready =
        stock_benchmark_statistics.fully_ready;


    output.readiness.market_regime_ready =
        market_regime.regime_ready;


    // ------------------------------------------------------------------------
    // Numerical integrity
    // ------------------------------------------------------------------------

    output.readiness.numerical_integrity =
        validateReturnStatistics(
            return_statistics) &&

        validateVolumeStatistics(
            volume_statistics) &&

        validateStockBenchmarkStatistics(
            stock_benchmark_statistics) &&

        validateMarketRegime(
            market_regime);


    // ------------------------------------------------------------------------
    // Final horizon readiness
    // ------------------------------------------------------------------------

    output.readiness.fully_ready =
        output.readiness.return_statistics_ready &&
        output.readiness.volume_statistics_ready &&
        output.readiness.stock_benchmark_statistics_ready &&
        output.readiness.market_regime_ready &&
        output.readiness.numerical_integrity;


    return
        output;
}


// ============================================================================
// Compose final Phase-4 state
// ============================================================================

MathematicalStateFeatures
MathematicalStateComposer::compose(
    const ReturnStatisticalFeatures&
        return_statistics,

    const VolumeStatisticalFeatures&
        volume_statistics,

    const StockBenchmarkStatisticalFeatures&
        stock_benchmark_statistics,

    const MarketRegimeFeatures&
        market_regime) const
{
    // ------------------------------------------------------------------------
    // Strict point-in-time validation
    // ------------------------------------------------------------------------

    validateInputs(
        return_statistics,
        volume_statistics,
        stock_benchmark_statistics,
        market_regime);


    MathematicalStateFeatures output;


    // ------------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------------

    output.stock_symbol =
        stock_symbol_;


    output.benchmark_symbol =
        benchmark_symbol_;


    output.decision_time =
        return_statistics.decision_time;


    // ========================================================================
    // 1 MINUTE
    // ========================================================================

    output.one_minute =
        composeHorizon(
            return_statistics.one_minute,
            volume_statistics.one_minute,
            stock_benchmark_statistics.one_minute,
            market_regime.one_minute);


    // ========================================================================
    // 5 MINUTE
    // ========================================================================

    output.five_minute =
        composeHorizon(
            return_statistics.five_minute,
            volume_statistics.five_minute,
            stock_benchmark_statistics.five_minute,
            market_regime.five_minute);


    // ========================================================================
    // 15 MINUTE
    // ========================================================================

    output.fifteen_minute =
        composeHorizon(
            return_statistics.fifteen_minute,
            volume_statistics.fifteen_minute,
            stock_benchmark_statistics.fifteen_minute,
            market_regime.fifteen_minute);


    // ------------------------------------------------------------------------
    // Per-timeframe readiness
    // ------------------------------------------------------------------------

    output.one_minute_ready =
        output.one_minute.
            readiness.fully_ready;


    output.five_minute_ready =
        output.five_minute.
            readiness.fully_ready;


    output.fifteen_minute_ready =
        output.fifteen_minute.
            readiness.fully_ready;


    // ------------------------------------------------------------------------
    // Global numerical integrity
    // ------------------------------------------------------------------------

    output.numerical_integrity =
        output.one_minute.
            readiness.numerical_integrity &&

        output.five_minute.
            readiness.numerical_integrity &&

        output.fifteen_minute.
            readiness.numerical_integrity;


    // ------------------------------------------------------------------------
    // Final Phase-4 readiness
    // ------------------------------------------------------------------------

    output.fully_ready =
        output.one_minute_ready &&
        output.five_minute_ready &&
        output.fifteen_minute_ready &&
        output.numerical_integrity;


    return
        output;
}

} // namespace devai::statistics