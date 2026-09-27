#include "devai/statistics/StockBenchmarkStatisticalEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace devai::statistics
{

namespace
{

constexpr double VARIANCE_EPSILON =
    1.0e-18;

constexpr double STANDARD_DEVIATION_EPSILON =
    1.0e-12;


double nanValue() noexcept
{
    return
        std::numeric_limits<double>::
            quiet_NaN();
}

} // namespace


// ============================================================================
// HorizonState
// ============================================================================

StockBenchmarkStatisticalEngine::
HorizonState::HorizonState(
    const std::string& statistical_symbol,
    std::size_t residual_window)
    :
    residual_statistics(
        statistical_symbol,
        residual_window)
{
}


// ============================================================================
// Constructor
// ============================================================================

StockBenchmarkStatisticalEngine::
StockBenchmarkStatisticalEngine(
    std::string stock_symbol,
    std::string benchmark_symbol,
    std::size_t regression_window,
    std::size_t residual_window,
    double extreme_z_threshold)
    :
    stock_symbol_(
        std::move(stock_symbol)),

    benchmark_symbol_(
        std::move(benchmark_symbol)),

    regression_window_(
        regression_window),

    residual_window_(
        residual_window),

    extreme_z_threshold_(
        extreme_z_threshold),

    one_minute_(
        stock_symbol_ + "_NIFTY_1M_RESIDUAL",
        residual_window_),

    five_minute_(
        stock_symbol_ + "_NIFTY_5M_RESIDUAL",
        residual_window_),

    fifteen_minute_(
        stock_symbol_ + "_NIFTY_15M_RESIDUAL",
        residual_window_)
{
    if (stock_symbol_.empty())
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine stock symbol cannot be empty.");
    }


    if (benchmark_symbol_.empty())
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine benchmark symbol cannot be empty.");
    }


    if (stock_symbol_ ==
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine stock and benchmark symbols "
            "must be different.");
    }


    if (regression_window_ < 2)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine regression window must "
            "be at least 2.");
    }


    if (residual_window_ == 0)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine residual window cannot be zero.");
    }


    if (!std::isfinite(
            extreme_z_threshold_) ||
        extreme_z_threshold_ <= 0.0)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine extreme Z threshold "
            "must be finite and positive.");
    }
}


// ============================================================================
// Accessors
// ============================================================================

const std::string&
StockBenchmarkStatisticalEngine::
stockSymbol() const noexcept
{
    return stock_symbol_;
}


const std::string&
StockBenchmarkStatisticalEngine::
benchmarkSymbol() const noexcept
{
    return benchmark_symbol_;
}


std::size_t
StockBenchmarkStatisticalEngine::
regressionWindow() const noexcept
{
    return regression_window_;
}


std::size_t
StockBenchmarkStatisticalEngine::
residualWindow() const noexcept
{
    return residual_window_;
}


double
StockBenchmarkStatisticalEngine::
extremeZThreshold() const noexcept
{
    return extreme_z_threshold_;
}


// ============================================================================
// Availability helper
// ============================================================================

bool
StockBenchmarkStatisticalEngine::
finiteObservation(
    bool available,
    double value) noexcept
{
    return
        available &&
        std::isfinite(value);
}


// ============================================================================
// Rolling regression
//
// Model:
//
//     stock_return = alpha
//                    + beta * benchmark_return
//                    + residual
//
// Population covariance / population variance are used.
//
// Since both covariance and variance use the same denominator,
// the denominator cancels in beta.
//
// Current observation is included in the rolling window.
// ============================================================================

StockBenchmarkStatisticalEngine::
RegressionResult
StockBenchmarkStatisticalEngine::
calculateRegression(
    const std::deque<ReturnPair>& observations)
    const
{
    RegressionResult output;


    if (observations.size() <
        regression_window_)
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
                observation.stock_return);

        benchmark_sum +=
            static_cast<long double>(
                observation.benchmark_return);
    }


    const long double count =
        static_cast<long double>(
            observations.size());


    const long double stock_mean =
        stock_sum /
        count;


    const long double benchmark_mean =
        benchmark_sum /
        count;


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
                observation.stock_return) -
            stock_mean;


        const long double benchmark_difference =
            static_cast<long double>(
                observation.benchmark_return) -
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


    const long double benchmark_variance =
        benchmark_variance_sum /
        count;


    const long double stock_variance =
        stock_variance_sum /
        count;


    const long double covariance =
        covariance_sum /
        count;


    if (!std::isfinite(
            static_cast<double>(
                benchmark_variance)) ||
        benchmark_variance <=
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


    double correlation =
        0.0;


    if (stock_variance >
            VARIANCE_EPSILON &&
        benchmark_variance >
            VARIANCE_EPSILON)
    {
        const long double denominator =
            std::sqrt(
                stock_variance *
                benchmark_variance);


        if (denominator >
            STANDARD_DEVIATION_EPSILON)
        {
            long double correlation_value =
                covariance /
                denominator;


            correlation_value =
                std::clamp(
                    correlation_value,
                    -1.0L,
                    1.0L);


            correlation =
                static_cast<double>(
                    correlation_value);
        }
    }


    output.beta =
        static_cast<double>(
            beta);


    output.alpha =
        static_cast<double>(
            alpha);


    output.correlation =
        correlation;


    output.valid =
        std::isfinite(output.beta) &&
        std::isfinite(output.alpha) &&
        std::isfinite(output.correlation);


    return output;
}


// ============================================================================
// Copy Phase 4.1 residual statistics
// ============================================================================

void
StockBenchmarkStatisticalEngine::
copyResidualStatistics(
    const RollingStatisticalFeatures& source,
    StockBenchmarkHorizonStatistics& destination)
{
    destination.residual_return =
        source.value;


    destination.residual_rolling_mean =
        source.rolling_mean;


    destination.residual_rolling_standard_deviation =
        source.rolling_standard_deviation;


    destination.residual_z_score =
        source.z_score;


    destination.residual_rolling_median =
        source.rolling_median;


    destination.residual_rolling_mad =
        source.rolling_mad;


    destination.residual_robust_z_score =
        source.robust_z_score;


    destination.residual_observation_count =
        source.observation_count;


    destination.residual_statistics_ready =
        source.ready;
}


// ============================================================================
// Relative statistical context
//
// This is descriptive only.
//
// It does NOT create a trading signal.
// ============================================================================

RelativeStatisticalContext
StockBenchmarkStatisticalEngine::
classifyRelativeContext(
    const StockBenchmarkHorizonStatistics& statistics)
    const noexcept
{
    if (!statistics.has_observation ||
        !statistics.regression_ready ||
        !std::isfinite(
            statistics.residual_return) ||
        !std::isfinite(
            statistics.residual_z_score))
    {
        return
            RelativeStatisticalContext::
                UNAVAILABLE;
    }


    if (statistics.residual_z_score >=
        extreme_z_threshold_)
    {
        return
            RelativeStatisticalContext::
                EXTREME_POSITIVE;
    }


    if (statistics.residual_z_score <=
        -extreme_z_threshold_)
    {
        return
            RelativeStatisticalContext::
                EXTREME_NEGATIVE;
    }


    if (statistics.residual_return > 0.0)
    {
        return
            RelativeStatisticalContext::
                POSITIVE;
    }


    if (statistics.residual_return < 0.0)
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
// Update one timeframe
// ============================================================================

void
StockBenchmarkStatisticalEngine::
updateHorizon(
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
    double relative_return)
{
    // ========================================================================
    // Source availability
    // ========================================================================

    if (!stock_has_source_candle ||
        !benchmark_has_source_candle ||
        !stock_source_timestamp ||
        !benchmark_source_timestamp)
    {
        return;
    }


    // ========================================================================
    // Stock/NIFTY candle timestamps must align
    //
    // Phase 4.4 must never regress mismatched periods.
    // ========================================================================

    if (*stock_source_timestamp !=
        *benchmark_source_timestamp)
    {
        return;
    }


    // ========================================================================
    // Backward source protection
    // ========================================================================

    if (state.last_stock_source_timestamp &&
        *stock_source_timestamp <
            *state.last_stock_source_timestamp)
    {
        throw std::runtime_error(
            "StockBenchmarkStatisticalEngine stock source timestamp "
            "moved backwards.");
    }


    if (state.last_benchmark_source_timestamp &&
        *benchmark_source_timestamp <
            *state.last_benchmark_source_timestamp)
    {
        throw std::runtime_error(
            "StockBenchmarkStatisticalEngine benchmark source timestamp "
            "moved backwards.");
    }


    // ========================================================================
    // Repeated carried-forward source protection
    //
    // Normal for 5m and 15m during minute-by-minute runtime.
    // ========================================================================

    const bool repeated_stock =
        state.last_stock_source_timestamp &&
        *stock_source_timestamp ==
            *state.last_stock_source_timestamp;


    const bool repeated_benchmark =
        state.last_benchmark_source_timestamp &&
        *benchmark_source_timestamp ==
            *state.last_benchmark_source_timestamp;


    if (repeated_stock ||
        repeated_benchmark)
    {
        if (repeated_stock &&
            repeated_benchmark)
        {
            return;
        }


        throw std::runtime_error(
            "StockBenchmarkStatisticalEngine stock/benchmark source "
            "progression became inconsistent.");
    }


    // ========================================================================
    // Returns must exist together
    // ========================================================================

    if (!has_stock_return ||
        !has_benchmark_return)
    {
        state.last_stock_source_timestamp =
            *stock_source_timestamp;

        state.last_benchmark_source_timestamp =
            *benchmark_source_timestamp;

        return;
    }


    // ========================================================================
    // Numeric integrity
    // ========================================================================

    if (!std::isfinite(stock_return) ||
        !std::isfinite(benchmark_return))
    {
        throw std::runtime_error(
            "StockBenchmarkStatisticalEngine received non-finite return.");
    }


    if (has_relative_return &&
        !std::isfinite(relative_return))
    {
        throw std::runtime_error(
            "StockBenchmarkStatisticalEngine received non-finite "
            "relative return.");
    }


    // ========================================================================
    // Phase 3.6 integrity
    //
    // Phase 3.6 relative return is:
    //
    //     stock_return - benchmark_return
    //
    // Phase 4.4 does not use this difference to calculate beta.
    // It is retained as descriptive relative context.
    // ========================================================================

    const double expected_relative_return =
        stock_return -
        benchmark_return;


    if (has_relative_return)
    {
        const double scale =
            std::max(
                {
                    1.0,
                    std::fabs(relative_return),
                    std::fabs(
                        expected_relative_return)
                });


        if (std::fabs(
                relative_return -
                expected_relative_return) >
            1.0e-10 * scale)
        {
            throw std::runtime_error(
                "StockBenchmarkStatisticalEngine Phase 3.6 relative "
                "return is inconsistent with stock/NIFTY returns.");
        }
    }


    // ========================================================================
    // Add paired observation
    // ========================================================================

    state.return_pairs.push_back(
        ReturnPair{
            stock_return,
            benchmark_return
        });


    while (state.return_pairs.size() >
           regression_window_)
    {
        state.return_pairs.pop_front();
    }


    // ========================================================================
    // Build current output
    // ========================================================================

    state.latest =
        StockBenchmarkHorizonStatistics{};


    state.latest.stock_return =
        stock_return;


    state.latest.benchmark_return =
        benchmark_return;


    state.latest.relative_return =
        has_relative_return
            ? relative_return
            : expected_relative_return;


    state.latest.has_observation =
        true;


    state.latest.paired_observation_count =
        state.return_pairs.size();


    // ========================================================================
    // Regression warm-up
    // ========================================================================

    if (state.return_pairs.size() <
        regression_window_)
    {
        state.last_stock_source_timestamp =
            *stock_source_timestamp;

        state.last_benchmark_source_timestamp =
            *benchmark_source_timestamp;

        return;
    }


    // ========================================================================
    // Calculate beta / alpha / correlation
    // ========================================================================

    const RegressionResult regression =
        calculateRegression(
            state.return_pairs);


    if (!regression.valid)
    {
        state.last_stock_source_timestamp =
            *stock_source_timestamp;

        state.last_benchmark_source_timestamp =
            *benchmark_source_timestamp;

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


    // ========================================================================
    // Current residual
    //
    // Current observation is included in the regression window.
    // ========================================================================

    const double residual =
        stock_return -
        (
            regression.alpha +
            regression.beta *
            benchmark_return
        );


    if (!std::isfinite(residual))
    {
        throw std::runtime_error(
            "StockBenchmarkStatisticalEngine produced non-finite residual.");
    }


    // ========================================================================
    // Phase 4.1 rolling residual statistics
    // ========================================================================

    const auto residual_statistics =
        state.residual_statistics.update(
            decision_time,
            residual);


    copyResidualStatistics(
        residual_statistics,
        state.latest);


    // ========================================================================
    // Final readiness
    // ========================================================================

    state.latest.fully_ready =
        state.latest.regression_ready &&
        state.latest.residual_statistics_ready;


    state.latest.relative_context =
        classifyRelativeContext(
            state.latest);


    // ========================================================================
    // Commit source timestamps only after successful processing
    // ========================================================================

    state.last_stock_source_timestamp =
        *stock_source_timestamp;


    state.last_benchmark_source_timestamp =
        *benchmark_source_timestamp;
}


// ============================================================================
// Validate complete inputs
// ============================================================================

void
StockBenchmarkStatisticalEngine::
validateInputs(
    const devai::features::PriceReturnFeatures&
        stock_price_returns,

    const devai::features::PriceReturnFeatures&
        benchmark_price_returns,

    const devai::features::StockBenchmarkFeatures&
        relative_features,

    const devai::market::MarketSnapshot&
        stock_snapshot,

    const devai::market::MarketSnapshot&
        benchmark_snapshot) const
{
    // ------------------------------------------------------------------------
    // Symbol integrity
    // ------------------------------------------------------------------------

    if (stock_price_returns.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine stock PriceReturnFeatures "
            "symbol mismatch.");
    }


    if (benchmark_price_returns.symbol !=
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine benchmark PriceReturnFeatures "
            "symbol mismatch.");
    }


    if (relative_features.stock_symbol !=
            stock_symbol_ ||
        relative_features.benchmark_symbol !=
            benchmark_symbol_)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine Phase 3.6 symbol mismatch.");
    }


    if (stock_snapshot.symbol !=
        stock_symbol_)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine stock snapshot symbol mismatch.");
    }


    if (benchmark_snapshot.symbol !=
        benchmark_symbol_)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine benchmark snapshot "
            "symbol mismatch.");
    }


    // ------------------------------------------------------------------------
    // Decision-time integrity
    // ------------------------------------------------------------------------

    const std::int64_t decision_time =
        stock_price_returns.decision_time;


    if (benchmark_price_returns.decision_time !=
            decision_time ||
        relative_features.decision_time !=
            decision_time ||
        stock_snapshot.decision_time !=
            decision_time ||
        benchmark_snapshot.decision_time !=
            decision_time)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine decision-time mismatch.");
    }


    // ------------------------------------------------------------------------
    // Trading-date / session integrity
    // ------------------------------------------------------------------------

    if (stock_snapshot.trading_date !=
        benchmark_snapshot.trading_date)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine trading-date mismatch.");
    }


    if (stock_snapshot.session_phase !=
        benchmark_snapshot.session_phase)
    {
        throw std::invalid_argument(
            "StockBenchmarkStatisticalEngine session-phase mismatch.");
    }
}


// ============================================================================
// Main update
// ============================================================================

StockBenchmarkStatisticalFeatures
StockBenchmarkStatisticalEngine::
update(
    const devai::features::PriceReturnFeatures&
        stock_price_returns,

    const devai::features::PriceReturnFeatures&
        benchmark_price_returns,

    const devai::features::StockBenchmarkFeatures&
        relative_features,

    const devai::market::MarketSnapshot&
        stock_snapshot,

    const devai::market::MarketSnapshot&
        benchmark_snapshot)
{
    validateInputs(
        stock_price_returns,
        benchmark_price_returns,
        relative_features,
        stock_snapshot,
        benchmark_snapshot);


    const std::int64_t decision_time =
        stock_price_returns.decision_time;


    // ========================================================================
    // Global decision-time protection
    // ========================================================================

    if (last_decision_time_)
    {
        if (decision_time <
            *last_decision_time_)
        {
            throw std::runtime_error(
                "StockBenchmarkStatisticalEngine decision time "
                "moved backwards.");
        }


        if (decision_time ==
            *last_decision_time_)
        {
            throw std::runtime_error(
                "StockBenchmarkStatisticalEngine duplicate decision time.");
        }
    }


    // ========================================================================
    // 1-minute
    // ========================================================================

    updateHorizon(
        one_minute_,

        stock_snapshot.one_minute.has_value(),

        stock_snapshot.one_minute
            ? std::optional<std::int64_t>(
                stock_snapshot.one_minute->
                    timestamp)
            : std::nullopt,

        benchmark_snapshot.one_minute.has_value(),

        benchmark_snapshot.one_minute
            ? std::optional<std::int64_t>(
                benchmark_snapshot.one_minute->
                    timestamp)
            : std::nullopt,

        decision_time,

        stock_price_returns.has_one_minute &&
            std::isfinite(
                stock_price_returns.
                    one_minute_return),

        stock_price_returns.one_minute_return,

        benchmark_price_returns.has_one_minute &&
            std::isfinite(
                benchmark_price_returns.
                    one_minute_return),

        benchmark_price_returns.one_minute_return,

        relative_features.relative_return.
            has_one_minute,

        relative_features.relative_return.
            one_minute);


    // ========================================================================
    // 5-minute
    // ========================================================================

    updateHorizon(
        five_minute_,

        stock_snapshot.five_minute.has_value(),

        stock_snapshot.five_minute
            ? std::optional<std::int64_t>(
                stock_snapshot.five_minute->
                    timestamp)
            : std::nullopt,

        benchmark_snapshot.five_minute.has_value(),

        benchmark_snapshot.five_minute
            ? std::optional<std::int64_t>(
                benchmark_snapshot.five_minute->
                    timestamp)
            : std::nullopt,

        decision_time,

        stock_price_returns.has_five_minute &&
            std::isfinite(
                stock_price_returns.
                    five_minute_return),

        stock_price_returns.five_minute_return,

        benchmark_price_returns.has_five_minute &&
            std::isfinite(
                benchmark_price_returns.
                    five_minute_return),

        benchmark_price_returns.five_minute_return,

        relative_features.relative_return.
            has_five_minute,

        relative_features.relative_return.
            five_minute);


    // ========================================================================
    // 15-minute
    // ========================================================================

    updateHorizon(
        fifteen_minute_,

        stock_snapshot.fifteen_minute.has_value(),

        stock_snapshot.fifteen_minute
            ? std::optional<std::int64_t>(
                stock_snapshot.fifteen_minute->
                    timestamp)
            : std::nullopt,

        benchmark_snapshot.fifteen_minute.has_value(),

        benchmark_snapshot.fifteen_minute
            ? std::optional<std::int64_t>(
                benchmark_snapshot.fifteen_minute->
                    timestamp)
            : std::nullopt,

        decision_time,

        stock_price_returns.has_fifteen_minute &&
            std::isfinite(
                stock_price_returns.
                    fifteen_minute_return),

        stock_price_returns.fifteen_minute_return,

        benchmark_price_returns.has_fifteen_minute &&
            std::isfinite(
                benchmark_price_returns.
                    fifteen_minute_return),

        benchmark_price_returns.fifteen_minute_return,

        relative_features.relative_return.
            has_fifteen_minute,

        relative_features.relative_return.
            fifteen_minute);


    // ========================================================================
    // Build Phase 4.4 output
    // ========================================================================

    StockBenchmarkStatisticalFeatures output;


    output.stock_symbol =
        stock_symbol_;


    output.benchmark_symbol =
        benchmark_symbol_;


    output.decision_time =
        decision_time;


    output.one_minute =
        one_minute_.latest;


    output.five_minute =
        five_minute_.latest;


    output.fifteen_minute =
        fifteen_minute_.latest;


    output.one_minute_ready =
        output.one_minute.fully_ready;


    output.five_minute_ready =
        output.five_minute.fully_ready;


    output.fifteen_minute_ready =
        output.fifteen_minute.fully_ready;


    output.fully_ready =
        output.one_minute_ready &&
        output.five_minute_ready &&
        output.fifteen_minute_ready;


    // ========================================================================
    // Commit decision time after successful processing
    // ========================================================================

    last_decision_time_ =
        decision_time;


    return output;
}


// ============================================================================
// Reset
//
// Continuous mathematical history is cleared ONLY by explicit reset.
//
// It is NOT reset at normal trading-session boundaries.
// ============================================================================

void
StockBenchmarkStatisticalEngine::
reset() noexcept
{
    one_minute_.return_pairs.clear();

    five_minute_.return_pairs.clear();

    fifteen_minute_.return_pairs.clear();


    one_minute_.
        last_stock_source_timestamp.reset();

    one_minute_.
        last_benchmark_source_timestamp.reset();


    five_minute_.
        last_stock_source_timestamp.reset();

    five_minute_.
        last_benchmark_source_timestamp.reset();


    fifteen_minute_.
        last_stock_source_timestamp.reset();

    fifteen_minute_.
        last_benchmark_source_timestamp.reset();


    one_minute_.
        residual_statistics.reset();

    five_minute_.
        residual_statistics.reset();

    fifteen_minute_.
        residual_statistics.reset();


    one_minute_.latest =
        StockBenchmarkHorizonStatistics{};

    five_minute_.latest =
        StockBenchmarkHorizonStatistics{};

    fifteen_minute_.latest =
        StockBenchmarkHorizonStatistics{};


    last_decision_time_.reset();
}

} // namespace devai::statistics