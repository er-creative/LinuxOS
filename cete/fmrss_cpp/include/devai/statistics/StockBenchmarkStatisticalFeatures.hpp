#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace devai::statistics
{

inline double stockBenchmarkStatisticalNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// Relative statistical context
//
// Descriptive only.
//
// POSITIVE:
//     stock return is above the return predicted from NIFTY.
//
// NEGATIVE:
//     stock return is below the return predicted from NIFTY.
//
// EXTREME states are determined from residual Z-score.
// ============================================================================

enum class RelativeStatisticalContext
{
    UNAVAILABLE,

    EXTREME_NEGATIVE,
    NEGATIVE,
    NEUTRAL,
    POSITIVE,
    EXTREME_POSITIVE
};


// ============================================================================
// Statistics for one timeframe
// ============================================================================

struct StockBenchmarkHorizonStatistics
{
    // ------------------------------------------------------------------------
    // Current paired observations
    // ------------------------------------------------------------------------

    double stock_return{
        stockBenchmarkStatisticalNaN()
    };

    double benchmark_return{
        stockBenchmarkStatisticalNaN()
    };

    double relative_return{
        stockBenchmarkStatisticalNaN()
    };


    // ------------------------------------------------------------------------
    // Rolling linear model
    //
    // stock_return =
    //     alpha +
    //     beta * benchmark_return +
    //     residual
    // ------------------------------------------------------------------------

    double rolling_beta{
        stockBenchmarkStatisticalNaN()
    };

    double rolling_alpha{
        stockBenchmarkStatisticalNaN()
    };

    double rolling_correlation{
        stockBenchmarkStatisticalNaN()
    };


    // ------------------------------------------------------------------------
    // Current residual
    // ------------------------------------------------------------------------

    double residual_return{
        stockBenchmarkStatisticalNaN()
    };


    // ------------------------------------------------------------------------
    // Residual rolling statistics
    // ------------------------------------------------------------------------

    double residual_rolling_mean{
        stockBenchmarkStatisticalNaN()
    };

    double residual_rolling_standard_deviation{
        stockBenchmarkStatisticalNaN()
    };

    double residual_z_score{
        stockBenchmarkStatisticalNaN()
    };

    double residual_rolling_median{
        stockBenchmarkStatisticalNaN()
    };

    double residual_rolling_mad{
        stockBenchmarkStatisticalNaN()
    };

    double residual_robust_z_score{
        stockBenchmarkStatisticalNaN()
    };


    // ------------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------------

    RelativeStatisticalContext relative_context{
        RelativeStatisticalContext::UNAVAILABLE
    };


    // ------------------------------------------------------------------------
    // Observation / readiness state
    // ------------------------------------------------------------------------

    std::size_t paired_observation_count{0};

    std::size_t residual_observation_count{0};


    bool has_observation{false};

    bool regression_ready{false};

    bool residual_statistics_ready{false};

    bool fully_ready{false};
};


// ============================================================================
// Complete Phase 4.4 output
// ============================================================================

struct StockBenchmarkStatisticalFeatures
{
    std::string stock_symbol;

    std::string benchmark_symbol;

    std::int64_t decision_time{0};


    StockBenchmarkHorizonStatistics
        one_minute;

    StockBenchmarkHorizonStatistics
        five_minute;

    StockBenchmarkHorizonStatistics
        fifteen_minute;


    bool one_minute_ready{false};

    bool five_minute_ready{false};

    bool fifteen_minute_ready{false};

    bool fully_ready{false};
};

} // namespace devai::statistics