#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/Timeframe.hpp"

#include "devai/statistics/StockBenchmarkStatisticalEngine.hpp"
#include "devai/statistics/StockBenchmarkStatisticalFeatures.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using namespace devai::features;
using namespace devai::market;
using namespace devai::statistics;


// ============================================================================
// Configuration
// ============================================================================

constexpr const char* DEFAULT_DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

constexpr const char* DEFAULT_STOCK_SYMBOL =
    "RELIANCE";

constexpr const char* DEFAULT_BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::size_t REGRESSION_WINDOW =
    60;

constexpr std::size_t RESIDUAL_WINDOW =
    60;

constexpr double EXTREME_Z_THRESHOLD =
    2.0;

constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr std::int64_t FIVE_MINUTE_SECONDS =
    300;

constexpr std::int64_t FIFTEEN_MINUTE_SECONDS =
    900;

constexpr double NUMERIC_TOLERANCE =
    1.0e-9;

constexpr double VARIANCE_EPSILON =
    1.0e-18;


// ============================================================================
// Validation counters
// ============================================================================

struct MetricValidation
{
    std::size_t observations{0};

    std::size_t beta_mismatches{0};
    std::size_t alpha_mismatches{0};
    std::size_t correlation_mismatches{0};
    std::size_t residual_mismatches{0};

    std::size_t residual_mean_mismatches{0};
    std::size_t residual_stddev_mismatches{0};
    std::size_t residual_z_mismatches{0};
    std::size_t residual_median_mismatches{0};
    std::size_t residual_mad_mismatches{0};
    std::size_t residual_robust_z_mismatches{0};

    std::size_t observation_count_mismatches{0};
    std::size_t readiness_mismatches{0};
    std::size_t context_mismatches{0};

    std::size_t repeated_source_snapshots{0};
};


struct ValidationStats
{
    std::size_t decision_points{0};
    std::size_t active_session_points{0};
    std::size_t synchronized_points{0};
    std::size_t incomplete_pairs{0};

    std::size_t new_sessions{0};

    std::size_t lookahead_violations{0};
    std::size_t decision_time_violations{0};
    std::size_t symbol_violations{0};
    std::size_t timestamp_alignment_violations{0};
    std::size_t cross_session_continuity_violations{0};
    std::size_t invalid_numeric_values{0};

    std::size_t feature_exceptions{0};
    std::size_t statistical_exceptions{0};

    MetricValidation one_minute;
    MetricValidation five_minute;
    MetricValidation fifteen_minute;
};


// ============================================================================
// Numeric helpers
// ============================================================================

bool approximatelyEqual(
    double left,
    double right,
    double tolerance = NUMERIC_TOLERANCE)
{
    if (std::isnan(left) &&
        std::isnan(right))
    {
        return true;
    }

    if (!std::isfinite(left) ||
        !std::isfinite(right))
    {
        return false;
    }

    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            });

    return
        std::fabs(left - right) <=
        tolerance * scale;
}


// ============================================================================
// Look-ahead
// ============================================================================

bool hasLookahead(
    const std::optional<Candle>& candle,
    std::int64_t timeframe_seconds,
    std::int64_t decision_time)
{
    if (!candle)
    {
        return false;
    }

    return
        candle->timestamp +
        timeframe_seconds >
        decision_time;
}


// ============================================================================
// Phase 3.6 relative-return construction
//
// Phase 3.6 defines:
//     relative return = stock return - benchmark return
//
// Only relative-return fields are required by Phase 4.4.
// ============================================================================

StockBenchmarkFeatures makeRelativeFeatures(
    const PriceReturnFeatures& stock,
    const PriceReturnFeatures& benchmark)
{
    if (stock.decision_time !=
        benchmark.decision_time)
    {
        throw std::runtime_error(
            "Price-return decision-time mismatch.");
    }

    StockBenchmarkFeatures output;

    output.stock_symbol =
        stock.symbol;

    output.benchmark_symbol =
        benchmark.symbol;

    output.decision_time =
        stock.decision_time;


    if (stock.has_one_minute &&
        benchmark.has_one_minute &&
        std::isfinite(stock.one_minute_return) &&
        std::isfinite(benchmark.one_minute_return))
    {
        output.relative_return.one_minute =
            stock.one_minute_return -
            benchmark.one_minute_return;

        output.relative_return.has_one_minute =
            true;
    }


    if (stock.has_five_minute &&
        benchmark.has_five_minute &&
        std::isfinite(stock.five_minute_return) &&
        std::isfinite(benchmark.five_minute_return))
    {
        output.relative_return.five_minute =
            stock.five_minute_return -
            benchmark.five_minute_return;

        output.relative_return.has_five_minute =
            true;
    }


    if (stock.has_fifteen_minute &&
        benchmark.has_fifteen_minute &&
        std::isfinite(stock.fifteen_minute_return) &&
        std::isfinite(benchmark.fifteen_minute_return))
    {
        output.relative_return.fifteen_minute =
            stock.fifteen_minute_return -
            benchmark.fifteen_minute_return;

        output.relative_return.has_fifteen_minute =
            true;
    }

    return output;
}


// ============================================================================
// Independent reference model
// ============================================================================

struct ReturnPair
{
    double stock{0.0};
    double benchmark{0.0};
};


struct ReferenceRegression
{
    double beta{
        std::numeric_limits<double>::quiet_NaN()
    };

    double alpha{
        std::numeric_limits<double>::quiet_NaN()
    };

    double correlation{
        std::numeric_limits<double>::quiet_NaN()
    };

    bool valid{false};
};


ReferenceRegression calculateRegression(
    const std::deque<ReturnPair>& observations)
{
    ReferenceRegression output;

    if (observations.size() <
        REGRESSION_WINDOW)
    {
        return output;
    }


    long double stock_sum =
        0.0L;

    long double benchmark_sum =
        0.0L;


    for (const auto& observation :
         observations)
    {
        stock_sum +=
            static_cast<long double>(
                observation.stock);

        benchmark_sum +=
            static_cast<long double>(
                observation.benchmark);
    }


    const long double count =
        static_cast<long double>(
            observations.size());


    const long double stock_mean =
        stock_sum / count;

    const long double benchmark_mean =
        benchmark_sum / count;


    long double stock_variance_sum =
        0.0L;

    long double benchmark_variance_sum =
        0.0L;

    long double covariance_sum =
        0.0L;


    for (const auto& observation :
         observations)
    {
        const long double stock_difference =
            static_cast<long double>(
                observation.stock) -
            stock_mean;

        const long double benchmark_difference =
            static_cast<long double>(
                observation.benchmark) -
            benchmark_mean;

        stock_variance_sum +=
            stock_difference *
            stock_difference;

        benchmark_variance_sum +=
            benchmark_difference *
            benchmark_difference;

        covariance_sum +=
            stock_difference *
            benchmark_difference;
    }


    const long double stock_variance =
        stock_variance_sum /
        count;

    const long double benchmark_variance =
        benchmark_variance_sum /
        count;

    const long double covariance =
        covariance_sum /
        count;


    if (benchmark_variance <=
        VARIANCE_EPSILON)
    {
        return output;
    }


    const long double beta =
        covariance /
        benchmark_variance;


    const long double alpha =
        stock_mean -
        beta *
        benchmark_mean;


    long double correlation =
        0.0L;


    if (stock_variance >
            VARIANCE_EPSILON &&
        benchmark_variance >
            VARIANCE_EPSILON)
    {
        const long double denominator =
            std::sqrt(
                stock_variance *
                benchmark_variance);

        if (denominator > 0.0L)
        {
            correlation =
                covariance /
                denominator;

            correlation =
                std::clamp(
                    correlation,
                    -1.0L,
                    1.0L);
        }
    }


    output.beta =
        static_cast<double>(beta);

    output.alpha =
        static_cast<double>(alpha);

    output.correlation =
        static_cast<double>(correlation);

    output.valid =
        std::isfinite(output.beta) &&
        std::isfinite(output.alpha) &&
        std::isfinite(output.correlation);

    return output;
}


// ============================================================================
// Independent rolling residual statistics
// ============================================================================

struct ReferenceResidualStatistics
{
    double mean{
        std::numeric_limits<double>::quiet_NaN()
    };

    double standard_deviation{
        std::numeric_limits<double>::quiet_NaN()
    };

    double z_score{
        std::numeric_limits<double>::quiet_NaN()
    };

    double median{
        std::numeric_limits<double>::quiet_NaN()
    };

    double mad{
        std::numeric_limits<double>::quiet_NaN()
    };

    double robust_z_score{
        std::numeric_limits<double>::quiet_NaN()
    };

    bool ready{false};
};


double medianOf(
    std::vector<double> values)
{
    if (values.empty())
    {
        return
            std::numeric_limits<double>::
                quiet_NaN();
    }

    std::sort(
        values.begin(),
        values.end());

    const std::size_t size =
        values.size();

    if ((size % 2) != 0)
    {
        return
            values[size / 2];
    }

    return
        (
            values[(size / 2) - 1] +
            values[size / 2]
        ) /
        2.0;
}


ReferenceResidualStatistics
calculateResidualStatistics(
    const std::deque<double>& residuals)
{
    ReferenceResidualStatistics output;

    if (residuals.empty())
    {
        return output;
    }


    long double sum =
        0.0L;

    for (const double value :
         residuals)
    {
        sum +=
            static_cast<long double>(
                value);
    }


    const long double count =
        static_cast<long double>(
            residuals.size());


    const long double mean =
        sum /
        count;


    long double variance_sum =
        0.0L;

    for (const double value :
         residuals)
    {
        const long double difference =
            static_cast<long double>(
                value) -
            mean;

        variance_sum +=
            difference *
            difference;
    }


    const long double variance =
        variance_sum /
        count;


    const double standard_deviation =
        std::sqrt(
            static_cast<double>(
                variance));


    output.mean =
        static_cast<double>(
            mean);

    output.standard_deviation =
        standard_deviation;


    const double current =
        residuals.back();


    if (standard_deviation <=
        1.0e-12)
    {
        output.z_score =
            0.0;
    }
    else
    {
        output.z_score =
            (
                current -
                output.mean
            ) /
            standard_deviation;
    }


    std::vector<double> values(
        residuals.begin(),
        residuals.end());


    output.median =
        medianOf(values);


    std::vector<double>
        absolute_deviations;

    absolute_deviations.reserve(
        values.size());


    for (const double value :
         values)
    {
        absolute_deviations.push_back(
            std::fabs(
                value -
                output.median));
    }


    output.mad =
        medianOf(
            absolute_deviations);


    if (output.mad <=
        1.0e-12)
    {
        output.robust_z_score =
            0.0;
    }
    else
    {
        output.robust_z_score =
            0.6744897501960817 *
            (
                current -
                output.median
            ) /
            output.mad;
    }


    output.ready =
        residuals.size() >=
        RESIDUAL_WINDOW;


    return output;
}


// ============================================================================
// Reference horizon
// ============================================================================

struct ReferenceHorizon
{
    std::deque<ReturnPair>
        return_pairs;

    std::deque<double>
        residuals;

    std::optional<std::int64_t>
        last_source_timestamp;

    StockBenchmarkHorizonStatistics
        latest;
};


// ============================================================================
// Expected context
// ============================================================================

RelativeStatisticalContext expectedContext(
    const StockBenchmarkHorizonStatistics& value)
{
    if (!value.has_observation ||
        !value.regression_ready ||
        !std::isfinite(
            value.residual_return) ||
        !std::isfinite(
            value.residual_z_score))
    {
        return
            RelativeStatisticalContext::
                UNAVAILABLE;
    }


    if (value.residual_z_score >=
        EXTREME_Z_THRESHOLD)
    {
        return
            RelativeStatisticalContext::
                EXTREME_POSITIVE;
    }


    if (value.residual_z_score <=
        -EXTREME_Z_THRESHOLD)
    {
        return
            RelativeStatisticalContext::
                EXTREME_NEGATIVE;
    }


    if (value.residual_return > 0.0)
    {
        return
            RelativeStatisticalContext::
                POSITIVE;
    }


    if (value.residual_return < 0.0)
    {
        return
            RelativeStatisticalContext::
                NEGATIVE;
    }


    return
        RelativeStatisticalContext::
            NEUTRAL;
}


// ============================================================================
// Update independent reference
// ============================================================================

void updateReference(
    ReferenceHorizon& state,
    std::optional<std::int64_t>
        source_timestamp,
    bool has_stock_return,
    double stock_return,
    bool has_benchmark_return,
    double benchmark_return)
{
    if (!source_timestamp)
    {
        return;
    }


    if (state.last_source_timestamp &&
        *source_timestamp ==
            *state.last_source_timestamp)
    {
        return;
    }


    if (state.last_source_timestamp &&
        *source_timestamp <
            *state.last_source_timestamp)
    {
        throw std::runtime_error(
            "Reference source timestamp moved backwards.");
    }


    state.last_source_timestamp =
        *source_timestamp;


    if (!has_stock_return ||
        !has_benchmark_return ||
        !std::isfinite(stock_return) ||
        !std::isfinite(benchmark_return))
    {
        return;
    }


    state.return_pairs.push_back(
        ReturnPair{
            stock_return,
            benchmark_return
        });


    while (state.return_pairs.size() >
           REGRESSION_WINDOW)
    {
        state.return_pairs.pop_front();
    }


    state.latest =
        StockBenchmarkHorizonStatistics{};


    state.latest.stock_return =
        stock_return;

    state.latest.benchmark_return =
        benchmark_return;

    state.latest.relative_return =
        stock_return -
        benchmark_return;

    state.latest.has_observation =
        true;

    state.latest.paired_observation_count =
        state.return_pairs.size();


    if (state.return_pairs.size() <
        REGRESSION_WINDOW)
    {
        return;
    }


    const auto regression =
        calculateRegression(
            state.return_pairs);


    if (!regression.valid)
    {
        return;
    }


    state.latest.rolling_beta =
        regression.beta;

    state.latest.rolling_alpha =
        regression.alpha;

    state.latest.rolling_correlation =
        regression.correlation;

    state.latest.regression_ready =
        true;


    const double residual =
        stock_return -
        (
            regression.alpha +
            regression.beta *
            benchmark_return
        );


    state.latest.residual_return =
        residual;


    state.residuals.push_back(
        residual);


    while (state.residuals.size() >
           RESIDUAL_WINDOW)
    {
        state.residuals.pop_front();
    }


    const auto residual_statistics =
        calculateResidualStatistics(
            state.residuals);


    state.latest.residual_rolling_mean =
        residual_statistics.mean;

    state.latest.residual_rolling_standard_deviation =
        residual_statistics.standard_deviation;

    state.latest.residual_z_score =
        residual_statistics.z_score;

    state.latest.residual_rolling_median =
        residual_statistics.median;

    state.latest.residual_rolling_mad =
        residual_statistics.mad;

    state.latest.residual_robust_z_score =
        residual_statistics.robust_z_score;

    state.latest.residual_observation_count =
        state.residuals.size();

    state.latest.residual_statistics_ready =
        residual_statistics.ready;

    state.latest.fully_ready =
        state.latest.regression_ready &&
        state.latest.residual_statistics_ready;

    state.latest.relative_context =
        expectedContext(
            state.latest);
}


// ============================================================================
// Comparison
// ============================================================================

void compareHorizon(
    const StockBenchmarkHorizonStatistics& actual,
    const StockBenchmarkHorizonStatistics& expected,
    MetricValidation& stats)
{
    if (!expected.has_observation)
    {
        return;
    }


    ++stats.observations;


    if (actual.paired_observation_count !=
        expected.paired_observation_count)
    {
        ++stats.observation_count_mismatches;
    }


    if (actual.regression_ready !=
            expected.regression_ready ||
        actual.residual_statistics_ready !=
            expected.residual_statistics_ready ||
        actual.fully_ready !=
            expected.fully_ready)
    {
        ++stats.readiness_mismatches;
    }


    if (expected.regression_ready)
    {
        if (!approximatelyEqual(
                actual.rolling_beta,
                expected.rolling_beta))
        {
            ++stats.beta_mismatches;
        }


        if (!approximatelyEqual(
                actual.rolling_alpha,
                expected.rolling_alpha))
        {
            ++stats.alpha_mismatches;
        }


        if (!approximatelyEqual(
                actual.rolling_correlation,
                expected.rolling_correlation))
        {
            ++stats.correlation_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_return,
                expected.residual_return))
        {
            ++stats.residual_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_rolling_mean,
                expected.residual_rolling_mean))
        {
            ++stats.residual_mean_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_rolling_standard_deviation,
                expected.residual_rolling_standard_deviation))
        {
            ++stats.residual_stddev_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_z_score,
                expected.residual_z_score))
        {
            ++stats.residual_z_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_rolling_median,
                expected.residual_rolling_median))
        {
            ++stats.residual_median_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_rolling_mad,
                expected.residual_rolling_mad))
        {
            ++stats.residual_mad_mismatches;
        }


        if (!approximatelyEqual(
                actual.residual_robust_z_score,
                expected.residual_robust_z_score))
        {
            ++stats.residual_robust_z_mismatches;
        }


        if (actual.relative_context !=
            expected.relative_context)
        {
            ++stats.context_mismatches;
        }
    }
}


// ============================================================================
// Numeric integrity
// ============================================================================

bool invalidFiniteOutput(
    const StockBenchmarkHorizonStatistics& value)
{
    if (!value.has_observation)
    {
        return false;
    }


    if (value.regression_ready)
    {
        if (!std::isfinite(value.rolling_beta) ||
            !std::isfinite(value.rolling_alpha) ||
            !std::isfinite(value.rolling_correlation) ||
            !std::isfinite(value.residual_return) ||
            !std::isfinite(value.residual_rolling_mean) ||
            !std::isfinite(
                value.residual_rolling_standard_deviation) ||
            !std::isfinite(value.residual_z_score) ||
            !std::isfinite(value.residual_rolling_median) ||
            !std::isfinite(value.residual_rolling_mad) ||
            !std::isfinite(value.residual_robust_z_score))
        {
            return true;
        }
    }


    return false;
}


// ============================================================================
// Print metric report
// ============================================================================

void printMetricReport(
    const std::string& name,
    const MetricValidation& stats)
{
    std::cout
        << "\n"
        << name
        << "\n"
        << "------------------------------------------------------------\n"
        << "Source observations             : "
        << stats.observations
        << "\n"
        << "Observation-count mismatches    : "
        << stats.observation_count_mismatches
        << "\n"
        << "Readiness mismatches            : "
        << stats.readiness_mismatches
        << "\n"
        << "Beta mismatches                 : "
        << stats.beta_mismatches
        << "\n"
        << "Alpha mismatches                : "
        << stats.alpha_mismatches
        << "\n"
        << "Correlation mismatches          : "
        << stats.correlation_mismatches
        << "\n"
        << "Residual mismatches             : "
        << stats.residual_mismatches
        << "\n"
        << "Residual mean mismatches        : "
        << stats.residual_mean_mismatches
        << "\n"
        << "Residual StdDev mismatches      : "
        << stats.residual_stddev_mismatches
        << "\n"
        << "Residual Z mismatches           : "
        << stats.residual_z_mismatches
        << "\n"
        << "Residual median mismatches      : "
        << stats.residual_median_mismatches
        << "\n"
        << "Residual MAD mismatches         : "
        << stats.residual_mad_mismatches
        << "\n"
        << "Robust residual Z mismatches    : "
        << stats.residual_robust_z_mismatches
        << "\n"
        << "Relative-context mismatches     : "
        << stats.context_mismatches
        << "\n"
        << "Repeated source snapshots       : "
        << stats.repeated_source_snapshots
        << "\n";
}


// ============================================================================
// Determine metric pass
// ============================================================================

bool metricPassed(
    const MetricValidation& stats)
{
    return
        stats.observations > 0 &&
        stats.beta_mismatches == 0 &&
        stats.alpha_mismatches == 0 &&
        stats.correlation_mismatches == 0 &&
        stats.residual_mismatches == 0 &&
        stats.residual_mean_mismatches == 0 &&
        stats.residual_stddev_mismatches == 0 &&
        stats.residual_z_mismatches == 0 &&
        stats.residual_median_mismatches == 0 &&
        stats.residual_mad_mismatches == 0 &&
        stats.residual_robust_z_mismatches == 0 &&
        stats.observation_count_mismatches == 0 &&
        stats.readiness_mismatches == 0 &&
        stats.context_mismatches == 0;
}

} // namespace


// ============================================================================
// MAIN
// ============================================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        std::filesystem::path data_folder =
            DEFAULT_DATA_FOLDER;

        std::string stock_symbol =
            DEFAULT_STOCK_SYMBOL;

        std::string benchmark_symbol =
            DEFAULT_BENCHMARK_SYMBOL;


        if (argc >= 2)
        {
            data_folder =
                argv[1];
        }

        if (argc >= 3)
        {
            stock_symbol =
                argv[2];
        }

        if (argc >= 4)
        {
            benchmark_symbol =
                argv[3];
        }


        const auto stock_file =
            data_folder /
            (
                stock_symbol +
                "_1min.txt"
            );


        const auto benchmark_file =
            data_folder /
            (
                benchmark_symbol +
                "_1min.txt"
            );


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 4.4 — REAL STOCK / NIFTY STATISTICAL VALIDATION\n"
            << "============================================================\n"
            << "Data folder        : "
            << data_folder
            << "\n"
            << "Stock              : "
            << stock_symbol
            << "\n"
            << "Benchmark          : "
            << benchmark_symbol
            << "\n"
            << "Regression window  : "
            << REGRESSION_WINDOW
            << "\n"
            << "Residual window    : "
            << RESIDUAL_WINDOW
            << "\n"
            << "Extreme Z          : "
            << EXTREME_Z_THRESHOLD
            << "\n"
            << "============================================================\n";


        if (!std::filesystem::exists(stock_file))
        {
            throw std::runtime_error(
                "Stock file not found: " +
                stock_file.string());
        }


        if (!std::filesystem::exists(
                benchmark_file))
        {
            throw std::runtime_error(
                "Benchmark file not found: " +
                benchmark_file.string());
        }


        // ====================================================================
        // Load real stock + NIFTY data
        // ====================================================================

        MarketDataLoader loader(
            data_folder);


        const auto stock_load =
            loader.loadPath(
                stock_file);


        const auto benchmark_load =
            loader.loadPath(
                benchmark_file);


        if (stock_load.candles.empty() ||
            benchmark_load.candles.empty())
        {
            throw std::runtime_error(
                "Stock or benchmark data is empty.");
        }


        // ====================================================================
        // Aggregate
        // ====================================================================

        CandleAggregator aggregator;


        const auto stock_five =
            aggregator.aggregate(
                stock_load.candles,
                Timeframe::FIVE_MINUTES);


        const auto stock_fifteen =
            aggregator.aggregate(
                stock_load.candles,
                Timeframe::FIFTEEN_MINUTES);


        const auto benchmark_five =
            aggregator.aggregate(
                benchmark_load.candles,
                Timeframe::FIVE_MINUTES);


        const auto benchmark_fifteen =
            aggregator.aggregate(
                benchmark_load.candles,
                Timeframe::FIFTEEN_MINUTES);


        std::cout
            << "\nMarket data\n"
            << "------------------------------------------------------------\n"
            << "Stock 1m            : "
            << stock_load.candles.size()
            << "\n"
            << "Stock 5m            : "
            << stock_five.candles.size()
            << "\n"
            << "Stock 15m           : "
            << stock_fifteen.candles.size()
            << "\n"
            << "Stock incomplete 5m : "
            << stock_five.incomplete_buckets
            << "\n"
            << "Stock incomplete15m : "
            << stock_fifteen.incomplete_buckets
            << "\n"
            << "NIFTY 1m            : "
            << benchmark_load.candles.size()
            << "\n"
            << "NIFTY 5m            : "
            << benchmark_five.candles.size()
            << "\n"
            << "NIFTY 15m           : "
            << benchmark_fifteen.candles.size()
            << "\n"
            << "NIFTY incomplete 5m : "
            << benchmark_five.incomplete_buckets
            << "\n"
            << "NIFTY incomplete15m : "
            << benchmark_fifteen.incomplete_buckets
            << "\n";


        // ====================================================================
        // Historical availability
        // ====================================================================

        MultiTimeframeSynchronizerConfig config;

        config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer synchronizer(
            config);


        const auto stock_timed_one =
            synchronizer.prepareOneMinute(
                stock_load.candles);

        const auto stock_timed_five =
            synchronizer.prepareFiveMinute(
                stock_five.candles);

        const auto stock_timed_fifteen =
            synchronizer.prepareFifteenMinute(
                stock_fifteen.candles);


        const auto benchmark_timed_one =
            synchronizer.prepareOneMinute(
                benchmark_load.candles);

        const auto benchmark_timed_five =
            synchronizer.prepareFiveMinute(
                benchmark_five.candles);

        const auto benchmark_timed_fifteen =
            synchronizer.prepareFifteenMinute(
                benchmark_fifteen.candles);


        // ====================================================================
        // Phase 2 runtime
        // ====================================================================

        MultiTimeframeCursor stock_cursor(
            stock_timed_one,
            stock_timed_five,
            stock_timed_fifteen);


        MultiTimeframeCursor benchmark_cursor(
            benchmark_timed_one,
            benchmark_timed_five,
            benchmark_timed_fifteen);


        SessionState stock_session;
        SessionState benchmark_session;


        MarketSnapshotBuilder stock_builder(
            stock_symbol);


        MarketSnapshotBuilder benchmark_builder(
            benchmark_symbol);


        // ====================================================================
        // Phase 3.1
        //
        // Frozen engine. History survives session boundaries.
        // ====================================================================

        PriceReturnFeatureEngine
            stock_return_engine(
                stock_symbol,
                64);


        PriceReturnFeatureEngine
            benchmark_return_engine(
                benchmark_symbol,
                64);


        // ====================================================================
        // Phase 4.4 production engine
        // ====================================================================

        StockBenchmarkStatisticalEngine
            statistical_engine(
                stock_symbol,
                benchmark_symbol,
                REGRESSION_WINDOW,
                RESIDUAL_WINDOW,
                EXTREME_Z_THRESHOLD);


        // ====================================================================
        // Independent reference state
        // ====================================================================

        ReferenceHorizon reference_one;
        ReferenceHorizon reference_five;
        ReferenceHorizon reference_fifteen;


        ValidationStats stats;


        std::optional<std::int64_t>
            previous_one_timestamp;

        std::optional<std::int64_t>
            previous_five_timestamp;

        std::optional<std::int64_t>
            previous_fifteen_timestamp;


        std::optional<std::size_t>
            previous_one_count;

        std::optional<std::size_t>
            previous_five_count;

        std::optional<std::size_t>
            previous_fifteen_count;


        // ====================================================================
        // Real runtime loop
        //
        // Use each real stock 1-minute completion as decision point.
        // ====================================================================

        for (const Candle& source :
             stock_load.candles)
        {
            const std::int64_t decision_time =
                source.timestamp +
                ONE_MINUTE_SECONDS;


            ++stats.decision_points;


            MarketSnapshot stock_snapshot =
                stock_builder.build(
                    decision_time,
                    stock_cursor,
                    stock_session);


            MarketSnapshot benchmark_snapshot =
                benchmark_builder.build(
                    decision_time,
                    benchmark_cursor,
                    benchmark_session);


            if (stock_snapshot.sessionActive())
            {
                ++stats.active_session_points;
            }


            if (stock_snapshot.new_session)
            {
                ++stats.new_sessions;
            }


            // ================================================================
            // Runtime integrity
            // ================================================================

            if (stock_snapshot.decision_time !=
                    decision_time ||
                benchmark_snapshot.decision_time !=
                    decision_time)
            {
                ++stats.decision_time_violations;
            }


            if (stock_snapshot.symbol !=
                    stock_symbol ||
                benchmark_snapshot.symbol !=
                    benchmark_symbol)
            {
                ++stats.symbol_violations;
            }


            if (hasLookahead(
                    stock_snapshot.one_minute,
                    ONE_MINUTE_SECONDS,
                    decision_time) ||
                hasLookahead(
                    stock_snapshot.five_minute,
                    FIVE_MINUTE_SECONDS,
                    decision_time) ||
                hasLookahead(
                    stock_snapshot.fifteen_minute,
                    FIFTEEN_MINUTE_SECONDS,
                    decision_time) ||
                hasLookahead(
                    benchmark_snapshot.one_minute,
                    ONE_MINUTE_SECONDS,
                    decision_time) ||
                hasLookahead(
                    benchmark_snapshot.five_minute,
                    FIVE_MINUTE_SECONDS,
                    decision_time) ||
                hasLookahead(
                    benchmark_snapshot.fifteen_minute,
                    FIFTEEN_MINUTE_SECONDS,
                    decision_time))
            {
                ++stats.lookahead_violations;
            }


            // ================================================================
            // Stock/NIFTY pair completeness
            //
            // Phase 4.4 requires aligned source periods.
            // ================================================================

            if (!stock_snapshot.one_minute ||
                !benchmark_snapshot.one_minute)
            {
                ++stats.incomplete_pairs;
                continue;
            }


            if (stock_snapshot.one_minute->timestamp !=
                benchmark_snapshot.one_minute->timestamp)
            {
                ++stats.timestamp_alignment_violations;
                continue;
            }


            ++stats.synchronized_points;


            // ================================================================
            // Phase 3.1
            // ================================================================

            PriceReturnFeatures stock_returns;
            PriceReturnFeatures benchmark_returns;


            try
            {
                stock_returns =
                    stock_return_engine.update(
                        stock_snapshot);


                benchmark_returns =
                    benchmark_return_engine.update(
                        benchmark_snapshot);
            }
            catch (const std::exception&)
            {
                ++stats.feature_exceptions;
                continue;
            }


            // ================================================================
            // Phase 3.6 relative-return portion
            // ================================================================

            const auto relative_features =
                makeRelativeFeatures(
                    stock_returns,
                    benchmark_returns);


            // ================================================================
            // Determine aligned timeframe source timestamps
            // ================================================================

            auto alignedTimestamp =
                [&](const std::optional<Candle>& stock_candle,
                    const std::optional<Candle>& benchmark_candle)
                -> std::optional<std::int64_t>
            {
                if (!stock_candle ||
                    !benchmark_candle)
                {
                    return std::nullopt;
                }


                if (stock_candle->timestamp !=
                    benchmark_candle->timestamp)
                {
                    return std::nullopt;
                }


                return
                    stock_candle->timestamp;
            };


            const auto one_timestamp =
                alignedTimestamp(
                    stock_snapshot.one_minute,
                    benchmark_snapshot.one_minute);


            const auto five_timestamp =
                alignedTimestamp(
                    stock_snapshot.five_minute,
                    benchmark_snapshot.five_minute);


            const auto fifteen_timestamp =
                alignedTimestamp(
                    stock_snapshot.fifteen_minute,
                    benchmark_snapshot.fifteen_minute);


            // ================================================================
            // Repeated-source counters
            // ================================================================

            if (one_timestamp &&
                previous_one_timestamp &&
                *one_timestamp ==
                    *previous_one_timestamp)
            {
                ++stats.one_minute.
                    repeated_source_snapshots;
            }


            if (five_timestamp &&
                previous_five_timestamp &&
                *five_timestamp ==
                    *previous_five_timestamp)
            {
                ++stats.five_minute.
                    repeated_source_snapshots;
            }


            if (fifteen_timestamp &&
                previous_fifteen_timestamp &&
                *fifteen_timestamp ==
                    *previous_fifteen_timestamp)
            {
                ++stats.fifteen_minute.
                    repeated_source_snapshots;
            }


            if (one_timestamp)
            {
                previous_one_timestamp =
                    one_timestamp;
            }

            if (five_timestamp)
            {
                previous_five_timestamp =
                    five_timestamp;
            }

            if (fifteen_timestamp)
            {
                previous_fifteen_timestamp =
                    fifteen_timestamp;
            }


            // ================================================================
            // Independent reference update
            // ================================================================

            updateReference(
                reference_one,
                one_timestamp,
                stock_returns.has_one_minute,
                stock_returns.one_minute_return,
                benchmark_returns.has_one_minute,
                benchmark_returns.one_minute_return);


            updateReference(
                reference_five,
                five_timestamp,
                stock_returns.has_five_minute,
                stock_returns.five_minute_return,
                benchmark_returns.has_five_minute,
                benchmark_returns.five_minute_return);


            updateReference(
                reference_fifteen,
                fifteen_timestamp,
                stock_returns.has_fifteen_minute,
                stock_returns.fifteen_minute_return,
                benchmark_returns.has_fifteen_minute,
                benchmark_returns.fifteen_minute_return);


            // ================================================================
            // Production Phase 4.4
            // ================================================================

            StockBenchmarkStatisticalFeatures actual;


            try
            {
                actual =
                    statistical_engine.update(
                        stock_returns,
                        benchmark_returns,
                        relative_features,
                        stock_snapshot,
                        benchmark_snapshot);
            }
            catch (const std::exception&)
            {
                ++stats.statistical_exceptions;
                continue;
            }


            // ================================================================
            // Compare independent mathematics
            // ================================================================

            compareHorizon(
                actual.one_minute,
                reference_one.latest,
                stats.one_minute);


            compareHorizon(
                actual.five_minute,
                reference_five.latest,
                stats.five_minute);


            compareHorizon(
                actual.fifteen_minute,
                reference_fifteen.latest,
                stats.fifteen_minute);


            // ================================================================
            // Cross-session continuity
            //
            // Mathematical history must not reset on new session.
            // ================================================================

            if (stock_snapshot.new_session)
            {
                if (previous_one_count &&
                    actual.one_minute.
                        paired_observation_count <
                        *previous_one_count)
                {
                    ++stats.
                        cross_session_continuity_violations;
                }


                if (previous_five_count &&
                    actual.five_minute.
                        paired_observation_count <
                        *previous_five_count)
                {
                    ++stats.
                        cross_session_continuity_violations;
                }


                if (previous_fifteen_count &&
                    actual.fifteen_minute.
                        paired_observation_count <
                        *previous_fifteen_count)
                {
                    ++stats.
                        cross_session_continuity_violations;
                }
            }


            previous_one_count =
                actual.one_minute.
                    paired_observation_count;

            previous_five_count =
                actual.five_minute.
                    paired_observation_count;

            previous_fifteen_count =
                actual.fifteen_minute.
                    paired_observation_count;


            // ================================================================
            // Numeric integrity
            // ================================================================

            if (invalidFiniteOutput(
                    actual.one_minute))
            {
                ++stats.invalid_numeric_values;
            }


            if (invalidFiniteOutput(
                    actual.five_minute))
            {
                ++stats.invalid_numeric_values;
            }


            if (invalidFiniteOutput(
                    actual.fifteen_minute))
            {
                ++stats.invalid_numeric_values;
            }
        }


        // ====================================================================
        // Report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "1-MINUTE VALIDATION\n"
            << "============================================================\n";

        printMetricReport(
            "1-MINUTE STOCK / NIFTY STATISTICS",
            stats.one_minute);


        std::cout
            << "\n"
            << "============================================================\n"
            << "5-MINUTE VALIDATION\n"
            << "============================================================\n";

        printMetricReport(
            "5-MINUTE STOCK / NIFTY STATISTICS",
            stats.five_minute);


        std::cout
            << "\n"
            << "============================================================\n"
            << "15-MINUTE VALIDATION\n"
            << "============================================================\n";

        printMetricReport(
            "15-MINUTE STOCK / NIFTY STATISTICS",
            stats.fifteen_minute);


        std::cout
            << "\n"
            << "============================================================\n"
            << "RUNTIME / SAFETY VALIDATION\n"
            << "============================================================\n"
            << "Decision points                     : "
            << stats.decision_points
            << "\n"
            << "Active-session points               : "
            << stats.active_session_points
            << "\n"
            << "Synchronized points                 : "
            << stats.synchronized_points
            << "\n"
            << "Incomplete pairs                    : "
            << stats.incomplete_pairs
            << "\n"
            << "New sessions                        : "
            << stats.new_sessions
            << "\n"
            << "Look-ahead violations               : "
            << stats.lookahead_violations
            << "\n"
            << "Decision-time violations            : "
            << stats.decision_time_violations
            << "\n"
            << "Symbol violations                   : "
            << stats.symbol_violations
            << "\n"
            << "Timestamp-alignment violations      : "
            << stats.timestamp_alignment_violations
            << "\n"
            << "Cross-session continuity violations : "
            << stats.cross_session_continuity_violations
            << "\n"
            << "Invalid numeric values              : "
            << stats.invalid_numeric_values
            << "\n"
            << "Phase 3.1 feature exceptions        : "
            << stats.feature_exceptions
            << "\n"
            << "Phase 4.4 statistical exceptions    : "
            << stats.statistical_exceptions
            << "\n";


        const bool runtime_passed =
            stats.lookahead_violations == 0 &&
            stats.decision_time_violations == 0 &&
            stats.symbol_violations == 0 &&
            stats.cross_session_continuity_violations == 0 &&
            stats.invalid_numeric_values == 0 &&
            stats.feature_exceptions == 0 &&
            stats.statistical_exceptions == 0;


        const bool one_passed =
            metricPassed(
                stats.one_minute);

        const bool five_passed =
            metricPassed(
                stats.five_minute);

        const bool fifteen_passed =
            metricPassed(
                stats.fifteen_minute);


        const bool all_passed =
            one_passed &&
            five_passed &&
            fifteen_passed &&
            runtime_passed;


        std::cout
            << "\n"
            << "============================================================\n";


        if (!all_passed)
        {
            std::cout
                << "PHASE 4.4 REAL STOCK / NIFTY STATISTICAL VALIDATION FAILED\n"
                << "============================================================\n"
                << "1m Beta / Alpha / Correlation : "
                << (one_passed ? "PASSED" : "FAILED")
                << "\n"
                << "5m Beta / Alpha / Correlation : "
                << (five_passed ? "PASSED" : "FAILED")
                << "\n"
                << "15m Beta / Alpha / Correlation: "
                << (fifteen_passed ? "PASSED" : "FAILED")
                << "\n"
                << "Runtime / Safety             : "
                << (runtime_passed ? "PASSED" : "FAILED")
                << "\n"
                << "============================================================\n";

            return 1;
        }


        std::cout
            << "PHASE 4.4 REAL STOCK / NIFTY STATISTICAL VALIDATION PASSED\n"
            << "============================================================\n"
            << "1m Rolling Beta                 : PASSED\n"
            << "1m Alpha / Intercept            : PASSED\n"
            << "1m Rolling Correlation          : PASSED\n"
            << "1m Residual Return              : PASSED\n"
            << "1m Residual Statistics          : PASSED\n"
            << "5m Rolling Beta                 : PASSED\n"
            << "5m Alpha / Intercept            : PASSED\n"
            << "5m Rolling Correlation          : PASSED\n"
            << "5m Residual Return              : PASSED\n"
            << "5m Residual Statistics          : PASSED\n"
            << "15m Rolling Beta                : PASSED\n"
            << "15m Alpha / Intercept           : PASSED\n"
            << "15m Rolling Correlation         : PASSED\n"
            << "15m Residual Return             : PASSED\n"
            << "15m Residual Statistics         : PASSED\n"
            << "Relative Statistical Context   : PASSED\n"
            << "Repeated Candle Protection     : PASSED\n"
            << "Window Readiness               : PASSED\n"
            << "Cross-Session Continuity       : PASSED\n"
            << "Look-Ahead Protection          : PASSED\n"
            << "Decision-Time Integrity        : PASSED\n"
            << "Numeric Integrity              : PASSED\n"
            << "============================================================\n";


        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nFATAL ERROR: "
            << exception.what()
            << "\n";

        return 1;
    }
}