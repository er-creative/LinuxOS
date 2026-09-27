#pragma once

#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"

#include "devai/market/MarketSnapshot.hpp"

#include "devai/statistics/RollingStatisticalEngine.hpp"
#include "devai/statistics/StockBenchmarkStatisticalFeatures.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace devai::statistics
{

class StockBenchmarkStatisticalEngine
{
public:

    explicit StockBenchmarkStatisticalEngine(
        std::string stock_symbol,
        std::string benchmark_symbol,
        std::size_t regression_window = 60,
        std::size_t residual_window = 60,
        double extreme_z_threshold = 2.0);


    [[nodiscard]]
    StockBenchmarkStatisticalFeatures update(
        const devai::features::PriceReturnFeatures&
            stock_price_returns,

        const devai::features::PriceReturnFeatures&
            benchmark_price_returns,

        const devai::features::StockBenchmarkFeatures&
            relative_features,

        const devai::market::MarketSnapshot&
            stock_snapshot,

        const devai::market::MarketSnapshot&
            benchmark_snapshot);


    void reset() noexcept;


    [[nodiscard]]
    const std::string&
    stockSymbol() const noexcept;


    [[nodiscard]]
    const std::string&
    benchmarkSymbol() const noexcept;


    [[nodiscard]]
    std::size_t
    regressionWindow() const noexcept;


    [[nodiscard]]
    std::size_t
    residualWindow() const noexcept;


    [[nodiscard]]
    double
    extremeZThreshold() const noexcept;


private:

    // ========================================================================
    // One paired stock/NIFTY return observation
    // ========================================================================

    struct ReturnPair
    {
        double stock_return{0.0};

        double benchmark_return{0.0};
    };


    // ========================================================================
    // Per-timeframe state
    // ========================================================================

    struct HorizonState
    {
        std::deque<ReturnPair>
            return_pairs;


        std::optional<std::int64_t>
            last_stock_source_timestamp;


        std::optional<std::int64_t>
            last_benchmark_source_timestamp;


        RollingStatisticalEngine
            residual_statistics;


        StockBenchmarkHorizonStatistics
            latest;


        HorizonState(
            const std::string& statistical_symbol,
            std::size_t residual_window);
    };


    // ========================================================================
    // Regression calculation
    // ========================================================================

    struct RegressionResult
    {
        double beta{
            stockBenchmarkStatisticalNaN()
        };

        double alpha{
            stockBenchmarkStatisticalNaN()
        };

        double correlation{
            stockBenchmarkStatisticalNaN()
        };

        bool valid{false};
    };


    // ========================================================================
    // Helpers
    // ========================================================================

    [[nodiscard]]
    RegressionResult calculateRegression(
        const std::deque<ReturnPair>& observations)
        const;


    [[nodiscard]]
    RelativeStatisticalContext classifyRelativeContext(
        const StockBenchmarkHorizonStatistics& statistics)
        const noexcept;


    void updateHorizon(
        HorizonState& state,

        bool stock_has_source_candle,
        std::optional<std::int64_t>
            stock_source_timestamp,

        bool benchmark_has_source_candle,
        std::optional<std::int64_t>
            benchmark_source_timestamp,

        std::int64_t decision_time,

        bool has_stock_return,
        double stock_return,

        bool has_benchmark_return,
        double benchmark_return,

        bool has_relative_return,
        double relative_return);


    void validateInputs(
        const devai::features::PriceReturnFeatures&
            stock_price_returns,

        const devai::features::PriceReturnFeatures&
            benchmark_price_returns,

        const devai::features::StockBenchmarkFeatures&
            relative_features,

        const devai::market::MarketSnapshot&
            stock_snapshot,

        const devai::market::MarketSnapshot&
            benchmark_snapshot) const;


    static void copyResidualStatistics(
        const RollingStatisticalFeatures& source,

        StockBenchmarkHorizonStatistics&
            destination);


    static bool finiteObservation(
        bool available,
        double value) noexcept;


    // ========================================================================
    // Configuration
    // ========================================================================

    std::string stock_symbol_;

    std::string benchmark_symbol_;

    std::size_t regression_window_{60};

    std::size_t residual_window_{60};

    double extreme_z_threshold_{2.0};


    // ========================================================================
    // Runtime protection
    // ========================================================================

    std::optional<std::int64_t>
        last_decision_time_;


    // ========================================================================
    // Timeframe states
    // ========================================================================

    HorizonState one_minute_;

    HorizonState five_minute_;

    HorizonState fifteen_minute_;
};

} // namespace devai::statistics