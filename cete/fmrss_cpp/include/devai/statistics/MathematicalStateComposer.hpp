#pragma once

#include "devai/statistics/MathematicalStateFeatures.hpp"
#include "devai/statistics/MarketRegimeFeatures.hpp"
#include "devai/statistics/ReturnStatisticalFeatures.hpp"
#include "devai/statistics/StockBenchmarkStatisticalFeatures.hpp"
#include "devai/statistics/VolumeStatisticalFeatures.hpp"

#include <string>

namespace devai::statistics
{

// ============================================================================
// Phase 4.6 — Mathematical State Composer
//
// Responsibility:
//
//     Phase 4.2 Return Statistical Engine
//                 │
//     Phase 4.3 Volume Statistical Engine
//                 │
//     Phase 4.4 Stock / NIFTY Statistical Engine
//                 │
//     Phase 4.5 Market Regime Engine
//                 │
//                 ▼
//         MathematicalStateComposer
//                 │
//                 ▼
//       MathematicalStateFeatures
//
// Phase 4.1 Rolling Statistical Engine is already embedded in the statistical
// outputs of 4.2-4.4.
//
// The composer:
//
//     - validates identity
//     - validates decision-time alignment
//     - validates numerical integrity
//     - composes 1m / 5m / 15m mathematical state
//     - calculates readiness
//
// It does NOT:
//
//     - calculate new statistics
//     - maintain rolling history
//     - calculate a trading score
//     - rank symbols
//     - generate BUY/SELL
//     - perform PCA
//     - perform reinforcement learning
//
// Therefore this class is stateless.
// ============================================================================

class MathematicalStateComposer
{
public:

    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------

    explicit MathematicalStateComposer(
        std::string stock_symbol,
        std::string benchmark_symbol);


    // ------------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------------

    [[nodiscard]]
    const std::string&
    stockSymbol() const noexcept;


    [[nodiscard]]
    const std::string&
    benchmarkSymbol() const noexcept;


    // ------------------------------------------------------------------------
    // Compose final Phase-4 observation
    // ------------------------------------------------------------------------

    [[nodiscard]]
    MathematicalStateFeatures compose(
        const ReturnStatisticalFeatures&
            return_statistics,

        const VolumeStatisticalFeatures&
            volume_statistics,

        const StockBenchmarkStatisticalFeatures&
            stock_benchmark_statistics,

        const MarketRegimeFeatures&
            market_regime) const;


private:

    // ------------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------------

    std::string stock_symbol_;
    std::string benchmark_symbol_;


    // ------------------------------------------------------------------------
    // Input validation
    // ------------------------------------------------------------------------

    void validateInputs(
        const ReturnStatisticalFeatures&
            return_statistics,

        const VolumeStatisticalFeatures&
            volume_statistics,

        const StockBenchmarkStatisticalFeatures&
            stock_benchmark_statistics,

        const MarketRegimeFeatures&
            market_regime) const;


    // ------------------------------------------------------------------------
    // Numerical-integrity validation
    // ------------------------------------------------------------------------

    [[nodiscard]]
    static bool finiteIfAvailable(
        bool available,
        double value) noexcept;


    [[nodiscard]]
    static bool validateReturnStatistics(
        const ReturnHorizonStatistics&
            statistics) noexcept;


    [[nodiscard]]
    static bool validateVolumeMetric(
        const VolumeMetricStatistics&
            statistics) noexcept;


    [[nodiscard]]
    static bool validateVolumeStatistics(
        const VolumeHorizonStatistics&
            statistics) noexcept;


    [[nodiscard]]
    static bool validateStockBenchmarkStatistics(
        const StockBenchmarkHorizonStatistics&
            statistics) noexcept;


    [[nodiscard]]
    static bool validateMarketRegime(
        const MarketRegimeHorizon&
            regime) noexcept;


    // ------------------------------------------------------------------------
    // Compose one timeframe
    // ------------------------------------------------------------------------

    [[nodiscard]]
    static MathematicalHorizonState composeHorizon(
        const ReturnHorizonStatistics&
            return_statistics,

        const VolumeHorizonStatistics&
            volume_statistics,

        const StockBenchmarkHorizonStatistics&
            stock_benchmark_statistics,

        const MarketRegimeHorizon&
            market_regime);
};

} // namespace devai::statistics