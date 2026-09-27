#pragma once

#include "devai/features/TrendMomentumFeatures.hpp"
#include "devai/features/VolatilityFeatures.hpp"

#include "devai/statistics/MarketRegimeFeatures.hpp"
#include "devai/statistics/ReturnStatisticalFeatures.hpp"
#include "devai/statistics/StockBenchmarkStatisticalFeatures.hpp"

#include <string>


namespace devai::statistics
{

class MarketRegimeEngine
{
public:

    explicit MarketRegimeEngine(
        std::string stock_symbol,
        std::string benchmark_symbol,
        double elevated_z_threshold = 1.0,
        double extreme_z_threshold = 2.0);


    [[nodiscard]]
    MarketRegimeFeatures update(
        const devai::features::TrendMomentumFeatures&
            trend_features,

        const devai::features::VolatilityFeatures&
            volatility_features,

        const ReturnStatisticalFeatures&
            return_statistics,

        const StockBenchmarkStatisticalFeatures&
            stock_benchmark_statistics) const;


    [[nodiscard]]
    const std::string&
    stockSymbol() const noexcept
    {
        return stock_symbol_;
    }


    [[nodiscard]]
    const std::string&
    benchmarkSymbol() const noexcept
    {
        return benchmark_symbol_;
    }


private:

    std::string stock_symbol_;

    std::string benchmark_symbol_;

    double elevated_z_threshold_;

    double extreme_z_threshold_;


    // ========================================================================
    // Classification
    // ========================================================================

    [[nodiscard]]
    MarketRegimeHorizon classifyHorizon(
        const devai::features::TimeframeTrendMomentumFeatures&
            trend,

        const devai::features::TimeframeVolatilityFeatures&
            volatility,

        const ReturnHorizonStatistics&
            return_statistics,

        const StockBenchmarkHorizonStatistics&
            stock_benchmark_statistics) const;


    [[nodiscard]]
    TrendState classifyTrend(
        const devai::features::TimeframeTrendMomentumFeatures&
            trend,
        int& trend_score) const;


    [[nodiscard]]
    VolatilityState classifyVolatility(
        const devai::features::TimeframeVolatilityFeatures&
            volatility,

        const ReturnHorizonStatistics&
            return_statistics) const;


    [[nodiscard]]
    StatisticalStability classifyStability(
        const ReturnHorizonStatistics&
            return_statistics,

        const StockBenchmarkHorizonStatistics&
            stock_benchmark_statistics) const;


    [[nodiscard]]
    MarketRegime classifyRegime(
        TrendState trend_state,
        VolatilityState volatility_state,
        StatisticalStability stability) const;


    // ========================================================================
    // Validation
    // ========================================================================

    void validateInputs(
        const devai::features::TrendMomentumFeatures&
            trend_features,

        const devai::features::VolatilityFeatures&
            volatility_features,

        const ReturnStatisticalFeatures&
            return_statistics,

        const StockBenchmarkStatisticalFeatures&
            stock_benchmark_statistics) const;


    [[nodiscard]]
    static bool finite(
        double value) noexcept;
};

} // namespace devai::statistics