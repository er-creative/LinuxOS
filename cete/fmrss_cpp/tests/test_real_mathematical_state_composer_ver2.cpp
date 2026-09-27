#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"
#include "devai/features/TrendMomentumFeatureEngine.hpp"
#include "devai/features/VolatilityFeatureEngine.hpp"
#include "devai/features/VolumeFeatureEngine.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/Timeframe.hpp"

#include "devai/statistics/MarketRegimeEngine.hpp"
#include "devai/statistics/MathematicalStateComposer.hpp"
#include "devai/statistics/ReturnStatisticalEngine.hpp"
#include "devai/statistics/StockBenchmarkStatisticalEngine.hpp"
#include "devai/statistics/VolumeStatisticalEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>


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


// ============================================================================
// Statistical windows
// ============================================================================

constexpr std::size_t RETURN_WINDOW = 60;
constexpr std::size_t VOLUME_WINDOW = 60;
constexpr std::size_t REGRESSION_WINDOW = 60;
constexpr std::size_t RESIDUAL_WINDOW = 60;


// ============================================================================
// Phase 4.3 — Volume statistical thresholds
// ============================================================================

constexpr double VOLUME_LOW_Z_THRESHOLD =
    -1.0;

constexpr double VOLUME_HIGH_Z_THRESHOLD =
    1.0;

constexpr double VOLUME_EXTREME_Z_THRESHOLD =
    2.0;


// ============================================================================
// Phase 4.5 — Market-regime thresholds
// ============================================================================

constexpr double REGIME_ELEVATED_Z_THRESHOLD =
    1.0;

constexpr double REGIME_EXTREME_Z_THRESHOLD =
    2.0;


// ============================================================================
// Validation configuration
// ============================================================================

constexpr double NUMERIC_TOLERANCE =
    1.0e-9;

constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr std::int64_t FIVE_MINUTE_SECONDS =
    300;

constexpr std::int64_t FIFTEEN_MINUTE_SECONDS =
    900;


// ============================================================================
// Validation statistics
// ============================================================================

struct ValidationStats
{
    std::size_t decision_points{0};
    std::size_t active_session_points{0};
    std::size_t synchronized_points{0};
    std::size_t incomplete_pairs{0};
    std::size_t new_sessions{0};

    std::size_t composer_observations{0};

    std::size_t one_minute_ready{0};
    std::size_t five_minute_ready{0};
    std::size_t fifteen_minute_ready{0};
    std::size_t fully_ready{0};

    std::size_t numerical_integrity_passed{0};
    std::size_t numerical_integrity_failed{0};

    std::size_t lookahead_violations{0};
    std::size_t decision_time_violations{0};
    std::size_t symbol_violations{0};
    std::size_t timestamp_alignment_violations{0};

    std::size_t return_composition_mismatches{0};
    std::size_t volume_composition_mismatches{0};
    std::size_t stock_benchmark_composition_mismatches{0};
    std::size_t regime_composition_mismatches{0};

    std::size_t one_minute_readiness_mismatches{0};
    std::size_t five_minute_readiness_mismatches{0};
    std::size_t fifteen_minute_readiness_mismatches{0};

    std::size_t global_integrity_mismatches{0};
    std::size_t global_readiness_mismatches{0};

    std::size_t price_return_exceptions{0};
    std::size_t trend_exceptions{0};
    std::size_t volatility_exceptions{0};
    std::size_t volume_feature_exceptions{0};

    std::size_t return_statistical_exceptions{0};
    std::size_t volume_statistical_exceptions{0};
    std::size_t stock_benchmark_statistical_exceptions{0};
    std::size_t market_regime_exceptions{0};
    std::size_t composer_exceptions{0};


    [[nodiscard]]
    bool passed() const noexcept
    {
        return
            composer_observations > 0 &&
            fully_ready > 0 &&

            lookahead_violations == 0 &&
            decision_time_violations == 0 &&
            symbol_violations == 0 &&
            timestamp_alignment_violations == 0 &&

            return_composition_mismatches == 0 &&
            volume_composition_mismatches == 0 &&
            stock_benchmark_composition_mismatches == 0 &&
            regime_composition_mismatches == 0 &&

            one_minute_readiness_mismatches == 0 &&
            five_minute_readiness_mismatches == 0 &&
            fifteen_minute_readiness_mismatches == 0 &&

            global_integrity_mismatches == 0 &&
            global_readiness_mismatches == 0 &&

            numerical_integrity_failed == 0 &&

            price_return_exceptions == 0 &&
            trend_exceptions == 0 &&
            volatility_exceptions == 0 &&
            volume_feature_exceptions == 0 &&

            return_statistical_exceptions == 0 &&
            volume_statistical_exceptions == 0 &&
            stock_benchmark_statistical_exceptions == 0 &&
            market_regime_exceptions == 0 &&
            composer_exceptions == 0;
    }
};


// ============================================================================
// General display helpers
// ============================================================================

const char* yesNo(
    bool value) noexcept
{
    return
        value ? "YES" : "NO";
}


const char* readyText(
    bool value) noexcept
{
    return
        value ? "READY" : "NOT READY";
}


const char* integrityText(
    bool value) noexcept
{
    return
        value ? "PASSED" : "FAILED";
}


// ============================================================================
// Enum -> text
// ============================================================================

const char* extremeStateText(
    StatisticalExtremeState state) noexcept
{
    switch (state)
    {
        case StatisticalExtremeState::EXTREME_NEGATIVE:
            return "EXTREME_NEGATIVE";

        case StatisticalExtremeState::NEGATIVE:
            return "NEGATIVE";

        case StatisticalExtremeState::NORMAL:
            return "NORMAL";

        case StatisticalExtremeState::POSITIVE:
            return "POSITIVE";

        case StatisticalExtremeState::EXTREME_POSITIVE:
            return "EXTREME_POSITIVE";

        case StatisticalExtremeState::UNAVAILABLE:
        default:
            return "UNAVAILABLE";
    }
}


const char* trendStateText(
    TrendState state) noexcept
{
    switch (state)
    {
        case TrendState::STRONG_DOWNTREND:
            return "STRONG_DOWNTREND";

        case TrendState::DOWNTREND:
            return "DOWNTREND";

        case TrendState::NEUTRAL:
            return "NEUTRAL";

        case TrendState::UPTREND:
            return "UPTREND";

        case TrendState::STRONG_UPTREND:
            return "STRONG_UPTREND";

        case TrendState::UNAVAILABLE:
        default:
            return "UNAVAILABLE";
    }
}


const char* volatilityStateText(
    VolatilityState state) noexcept
{
    switch (state)
    {
        case VolatilityState::LOW:
            return "LOW";

        case VolatilityState::NORMAL:
            return "NORMAL";

        case VolatilityState::HIGH:
            return "HIGH";

        case VolatilityState::EXTREME:
            return "EXTREME";

        case VolatilityState::UNAVAILABLE:
        default:
            return "UNAVAILABLE";
    }
}


const char* stabilityText(
    StatisticalStability state) noexcept
{
    switch (state)
    {
        case StatisticalStability::STABLE:
            return "STABLE";

        case StatisticalStability::ELEVATED:
            return "ELEVATED";

        case StatisticalStability::UNSTABLE:
            return "UNSTABLE";

        case StatisticalStability::UNAVAILABLE:
        default:
            return "UNAVAILABLE";
    }
}


const char* regimeText(
    MarketRegime regime) noexcept
{
    switch (regime)
    {
        case MarketRegime::TRENDING_UP:
            return "TRENDING_UP";

        case MarketRegime::TRENDING_DOWN:
            return "TRENDING_DOWN";

        case MarketRegime::RANGE_BOUND:
            return "RANGE_BOUND";

        case MarketRegime::HIGH_VOLATILITY:
            return "HIGH_VOLATILITY";

        case MarketRegime::UNSTABLE:
            return "UNSTABLE";

        case MarketRegime::UNAVAILABLE:
        default:
            return "UNAVAILABLE";
    }
}


// ============================================================================
// Timestamp formatting
// ============================================================================

std::string formatTimestamp(
    std::int64_t timestamp)
{
    const std::time_t raw_time =
        static_cast<std::time_t>(
            timestamp);


    std::tm local_time{};


#if defined(_WIN32)

    localtime_s(
        &local_time,
        &raw_time);

#else

    localtime_r(
        &raw_time,
        &local_time);

#endif


    std::ostringstream stream;


    stream
        << std::put_time(
            &local_time,
            "%Y-%m-%d %H:%M:%S");


    return
        stream.str();
}


// ============================================================================
// Output helpers
// ============================================================================

void printValue(
    const std::string& name,
    double value)
{
    std::cout
        << std::left
        << std::setw(37)
        << name
        << " : ";


    if (std::isfinite(value))
    {
        std::cout
            << std::fixed
            << std::setprecision(8)
            << value;
    }
    else if (std::isnan(value))
    {
        std::cout
            << "NaN";
    }
    else
    {
        std::cout
            << value;
    }


    std::cout
        << '\n';
}


void printCountValue(
    const std::string& name,
    std::size_t value)
{
    std::cout
        << std::left
        << std::setw(39)
        << name
        << " : "
        << value
        << '\n';
}


void printTextValue(
    const std::string& name,
    const std::string& value)
{
    std::cout
        << std::left
        << std::setw(37)
        << name
        << " : "
        << value
        << '\n';
}


// ============================================================================
// Numeric comparison
// ============================================================================

bool sameDouble(
    double lhs,
    double rhs)
{
    if (std::isnan(lhs) &&
        std::isnan(rhs))
    {
        return true;
    }


    if (!std::isfinite(lhs) ||
        !std::isfinite(rhs))
    {
        return
            lhs == rhs;
    }


    const double scale =
        std::max(
            1.0,
            std::max(
                std::fabs(lhs),
                std::fabs(rhs)));


    return
        std::fabs(lhs - rhs) <=
        NUMERIC_TOLERANCE *
        scale;
}


// ============================================================================
// Phase 3.6 relative-return context
// ============================================================================

StockBenchmarkFeatures makeRelativeFeatures(
    const PriceReturnFeatures& stock,
    const PriceReturnFeatures& benchmark)
{
    StockBenchmarkFeatures output;


    output.stock_symbol =
        stock.symbol;

    output.benchmark_symbol =
        benchmark.symbol;

    output.decision_time =
        stock.decision_time;


    if (stock.has_one_minute &&
        benchmark.has_one_minute &&
        std::isfinite(
            stock.one_minute_return) &&
        std::isfinite(
            benchmark.one_minute_return))
    {
        output.relative_return.one_minute =
            stock.one_minute_return -
            benchmark.one_minute_return;

        output.relative_return.has_one_minute =
            true;
    }


    if (stock.has_five_minute &&
        benchmark.has_five_minute &&
        std::isfinite(
            stock.five_minute_return) &&
        std::isfinite(
            benchmark.five_minute_return))
    {
        output.relative_return.five_minute =
            stock.five_minute_return -
            benchmark.five_minute_return;

        output.relative_return.has_five_minute =
            true;
    }


    if (stock.has_fifteen_minute &&
        benchmark.has_fifteen_minute &&
        std::isfinite(
            stock.fifteen_minute_return) &&
        std::isfinite(
            benchmark.fifteen_minute_return))
    {
        output.relative_return.fifteen_minute =
            stock.fifteen_minute_return -
            benchmark.fifteen_minute_return;

        output.relative_return.has_fifteen_minute =
            true;
    }


    return
        output;
}


// ============================================================================
// Look-ahead validation
// ============================================================================

bool hasLookahead(
    const MarketSnapshot& snapshot)
{
    if (snapshot.one_minute.has_value())
    {
        if (snapshot.one_minute->timestamp +
                ONE_MINUTE_SECONDS >
            snapshot.decision_time)
        {
            return true;
        }
    }


    if (snapshot.five_minute.has_value())
    {
        if (snapshot.five_minute->timestamp +
                FIVE_MINUTE_SECONDS >
            snapshot.decision_time)
        {
            return true;
        }
    }


    if (snapshot.fifteen_minute.has_value())
    {
        if (snapshot.fifteen_minute->timestamp +
                FIFTEEN_MINUTE_SECONDS >
            snapshot.decision_time)
        {
            return true;
        }
    }


    return false;
}


// ============================================================================
// Phase 4.2 comparison
// ============================================================================

bool sameReturnStatistics(
    const ReturnHorizonStatistics& lhs,
    const ReturnHorizonStatistics& rhs)
{
    return
        sameDouble(lhs.return_value, rhs.return_value) &&
        sameDouble(lhs.absolute_return, rhs.absolute_return) &&

        sameDouble(lhs.rolling_mean, rhs.rolling_mean) &&
        sameDouble(
            lhs.rolling_standard_deviation,
            rhs.rolling_standard_deviation) &&

        sameDouble(lhs.z_score, rhs.z_score) &&
        sameDouble(lhs.rolling_median, rhs.rolling_median) &&
        sameDouble(lhs.rolling_mad, rhs.rolling_mad) &&
        sameDouble(lhs.robust_z_score, rhs.robust_z_score) &&

        sameDouble(
            lhs.absolute_return_mean,
            rhs.absolute_return_mean) &&

        sameDouble(
            lhs.absolute_return_standard_deviation,
            rhs.absolute_return_standard_deviation) &&

        sameDouble(
            lhs.absolute_return_z_score,
            rhs.absolute_return_z_score) &&

        sameDouble(
            lhs.absolute_return_median,
            rhs.absolute_return_median) &&

        sameDouble(
            lhs.absolute_return_mad,
            rhs.absolute_return_mad) &&

        sameDouble(
            lhs.absolute_return_robust_z_score,
            rhs.absolute_return_robust_z_score) &&

        lhs.extreme_state ==
            rhs.extreme_state &&

        lhs.observation_count ==
            rhs.observation_count &&

        lhs.has_observation ==
            rhs.has_observation &&

        lhs.ready ==
            rhs.ready;
}


// ============================================================================
// Phase 4.3 metric comparison
// ============================================================================

bool sameVolumeMetric(
    const VolumeMetricStatistics& lhs,
    const VolumeMetricStatistics& rhs)
{
    return
        sameDouble(lhs.value, rhs.value) &&
        sameDouble(lhs.rolling_mean, rhs.rolling_mean) &&

        sameDouble(
            lhs.rolling_standard_deviation,
            rhs.rolling_standard_deviation) &&

        sameDouble(lhs.z_score, rhs.z_score) &&
        sameDouble(lhs.rolling_median, rhs.rolling_median) &&
        sameDouble(lhs.rolling_mad, rhs.rolling_mad) &&
        sameDouble(lhs.robust_z_score, rhs.robust_z_score) &&

        lhs.observation_count ==
            rhs.observation_count &&

        lhs.has_observation ==
            rhs.has_observation &&

        lhs.ready ==
            rhs.ready;
}


// ============================================================================
// Phase 4.3 horizon comparison
// ============================================================================

bool sameVolumeStatistics(
    const VolumeHorizonStatistics& lhs,
    const VolumeHorizonStatistics& rhs)
{
    return
        sameVolumeMetric(
            lhs.volume,
            rhs.volume) &&

        sameVolumeMetric(
            lhs.rvol20,
            rhs.rvol20) &&

        sameVolumeMetric(
            lhs.rvol50,
            rhs.rvol50) &&

        lhs.has_volume ==
            rhs.has_volume &&

        lhs.has_rvol20 ==
            rhs.has_rvol20 &&

        lhs.has_rvol50 ==
            rhs.has_rvol50 &&

        lhs.volume_ready ==
            rhs.volume_ready &&

        lhs.rvol20_ready ==
            rhs.rvol20_ready &&

        lhs.rvol50_ready ==
            rhs.rvol50_ready &&

        lhs.fully_ready ==
            rhs.fully_ready &&

        lhs.abnormal_volume_state ==
            rhs.abnormal_volume_state;
}


// ============================================================================
// Phase 4.4 comparison
// ============================================================================

bool sameStockBenchmarkStatistics(
    const StockBenchmarkHorizonStatistics& lhs,
    const StockBenchmarkHorizonStatistics& rhs)
{
    return
        sameDouble(lhs.stock_return, rhs.stock_return) &&
        sameDouble(lhs.benchmark_return, rhs.benchmark_return) &&
        sameDouble(lhs.relative_return, rhs.relative_return) &&

        sameDouble(lhs.rolling_beta, rhs.rolling_beta) &&
        sameDouble(lhs.rolling_alpha, rhs.rolling_alpha) &&
        sameDouble(
            lhs.rolling_correlation,
            rhs.rolling_correlation) &&

        sameDouble(lhs.residual_return, rhs.residual_return) &&

        sameDouble(
            lhs.residual_rolling_mean,
            rhs.residual_rolling_mean) &&

        sameDouble(
            lhs.residual_rolling_standard_deviation,
            rhs.residual_rolling_standard_deviation) &&

        sameDouble(
            lhs.residual_z_score,
            rhs.residual_z_score) &&

        sameDouble(
            lhs.residual_rolling_median,
            rhs.residual_rolling_median) &&

        sameDouble(
            lhs.residual_rolling_mad,
            rhs.residual_rolling_mad) &&

        sameDouble(
            lhs.residual_robust_z_score,
            rhs.residual_robust_z_score) &&

        lhs.relative_context ==
            rhs.relative_context &&

        lhs.paired_observation_count ==
            rhs.paired_observation_count &&

        lhs.residual_observation_count ==
            rhs.residual_observation_count &&

        lhs.has_observation ==
            rhs.has_observation &&

        lhs.regression_ready ==
            rhs.regression_ready &&

        lhs.residual_statistics_ready ==
            rhs.residual_statistics_ready &&

        lhs.fully_ready ==
            rhs.fully_ready;
}


// ============================================================================
// Phase 4.5 comparison
// ============================================================================

bool sameMarketRegime(
    const MarketRegimeHorizon& lhs,
    const MarketRegimeHorizon& rhs)
{
    return
        lhs.trend_state ==
            rhs.trend_state &&

        lhs.volatility_state ==
            rhs.volatility_state &&

        lhs.statistical_stability ==
            rhs.statistical_stability &&

        lhs.regime ==
            rhs.regime &&

        lhs.trend_score ==
            rhs.trend_score &&

        sameDouble(
            lhs.atr14_percent,
            rhs.atr14_percent) &&

        sameDouble(
            lhs.realized_volatility_20,
            rhs.realized_volatility_20) &&

        sameDouble(
            lhs.return_z_score,
            rhs.return_z_score) &&

        sameDouble(
            lhs.absolute_return_z_score,
            rhs.absolute_return_z_score) &&

        sameDouble(
            lhs.residual_z_score,
            rhs.residual_z_score) &&

        sameDouble(
            lhs.rolling_correlation,
            rhs.rolling_correlation) &&

        lhs.trend_ready ==
            rhs.trend_ready &&

        lhs.volatility_ready ==
            rhs.volatility_ready &&

        lhs.statistical_stability_ready ==
            rhs.statistical_stability_ready &&

        lhs.regime_ready ==
            rhs.regime_ready;
}


// ============================================================================
// Expected Phase 4.6 horizon readiness
// ============================================================================

bool expectedHorizonReady(
    const ReturnHorizonStatistics& returns,
    const VolumeHorizonStatistics& volume,
    const StockBenchmarkHorizonStatistics& benchmark,
    const MarketRegimeHorizon& regime,
    bool numerical_integrity)
{
    return
        returns.ready &&
        volume.fully_ready &&
        benchmark.fully_ready &&
        regime.regime_ready &&
        numerical_integrity;
}


// ============================================================================
// Return statistics display
// ============================================================================

void printReturnStatistics(
    const ReturnHorizonStatistics& state)
{
    std::cout
        << "\nRETURN STATISTICS\n"
        << "------------------------------------------------------------\n";


    printValue("Return", state.return_value);
    printValue("Absolute Return", state.absolute_return);
    printValue("Rolling Mean", state.rolling_mean);

    printValue(
        "Rolling StdDev",
        state.rolling_standard_deviation);

    printValue("Z-Score", state.z_score);
    printValue("Rolling Median", state.rolling_median);
    printValue("Rolling MAD", state.rolling_mad);
    printValue("Robust Z-Score", state.robust_z_score);


    std::cout
        << "\nABSOLUTE RETURN STATISTICS\n"
        << "------------------------------------------------------------\n";


    printValue(
        "Absolute Return Mean",
        state.absolute_return_mean);

    printValue(
        "Absolute Return StdDev",
        state.absolute_return_standard_deviation);

    printValue(
        "Absolute Return Z-Score",
        state.absolute_return_z_score);

    printValue(
        "Absolute Return Median",
        state.absolute_return_median);

    printValue(
        "Absolute Return MAD",
        state.absolute_return_mad);

    printValue(
        "Absolute Return Robust Z",
        state.absolute_return_robust_z_score);


    printTextValue(
        "Extreme State",
        extremeStateText(
            state.extreme_state));


    printCountValue(
        "Observation Count",
        state.observation_count);


    printTextValue(
        "Has Observation",
        yesNo(
            state.has_observation));


    printTextValue(
        "Return Statistics Ready",
        readyText(
            state.ready));
}


// ============================================================================
// Volume metric display
// ============================================================================

void printVolumeMetric(
    const std::string& title,
    const VolumeMetricStatistics& metric)
{
    std::cout
        << "\n"
        << title
        << '\n'
        << "------------------------------------------------------------\n";


    printValue("Value", metric.value);
    printValue("Rolling Mean", metric.rolling_mean);

    printValue(
        "Rolling StdDev",
        metric.rolling_standard_deviation);

    printValue("Z-Score", metric.z_score);
    printValue("Rolling Median", metric.rolling_median);
    printValue("Rolling MAD", metric.rolling_mad);
    printValue("Robust Z-Score", metric.robust_z_score);


    printCountValue(
        "Observation Count",
        metric.observation_count);


    printTextValue(
        "Has Observation",
        yesNo(
            metric.has_observation));


    printTextValue(
        "Ready",
        readyText(
            metric.ready));
}


// ============================================================================
// Volume statistics display
// ============================================================================

void printVolumeStatistics(
    const VolumeHorizonStatistics& state)
{
    std::cout
        << "\nVOLUME STATISTICS\n"
        << "============================================================\n";


    printVolumeMetric(
        "RAW VOLUME",
        state.volume);


    printVolumeMetric(
        "RVOL20",
        state.rvol20);


    printVolumeMetric(
        "RVOL50",
        state.rvol50);


    std::cout
        << "\nVOLUME READINESS\n"
        << "------------------------------------------------------------\n";


    printTextValue(
        "Has Volume",
        yesNo(
            state.has_volume));


    printTextValue(
        "Has RVOL20",
        yesNo(
            state.has_rvol20));


    printTextValue(
        "Has RVOL50",
        yesNo(
            state.has_rvol50));


    printTextValue(
        "Volume Ready",
        readyText(
            state.volume_ready));


    printTextValue(
        "RVOL20 Ready",
        readyText(
            state.rvol20_ready));


    printTextValue(
        "RVOL50 Ready",
        readyText(
            state.rvol50_ready));


    printTextValue(
        "Volume Statistics Fully Ready",
        readyText(
            state.fully_ready));
}


// ============================================================================
// Stock / NIFTY statistics display
// ============================================================================

void printStockBenchmarkStatistics(
    const StockBenchmarkHorizonStatistics& state)
{
    std::cout
        << "\nSTOCK / NIFTY STATISTICS\n"
        << "------------------------------------------------------------\n";


    printValue(
        "Stock Return",
        state.stock_return);


    printValue(
        "NIFTY Return",
        state.benchmark_return);


    printValue(
        "Relative Return",
        state.relative_return);


    printValue(
        "Rolling Beta",
        state.rolling_beta);


    printValue(
        "Rolling Alpha",
        state.rolling_alpha);


    printValue(
        "Rolling Correlation",
        state.rolling_correlation);


    printValue(
        "Residual Return",
        state.residual_return);


    printValue(
        "Residual Rolling Mean",
        state.residual_rolling_mean);


    printValue(
        "Residual Rolling StdDev",
        state.residual_rolling_standard_deviation);


    printValue(
        "Residual Z-Score",
        state.residual_z_score);


    printValue(
        "Residual Rolling Median",
        state.residual_rolling_median);


    printValue(
        "Residual Rolling MAD",
        state.residual_rolling_mad);


    printValue(
        "Residual Robust Z",
        state.residual_robust_z_score);


    printCountValue(
        "Paired Observation Count",
        state.paired_observation_count);


    printCountValue(
        "Residual Observation Count",
        state.residual_observation_count);


    printTextValue(
        "Has Observation",
        yesNo(
            state.has_observation));


    printTextValue(
        "Regression Ready",
        readyText(
            state.regression_ready));


    printTextValue(
        "Residual Statistics Ready",
        readyText(
            state.residual_statistics_ready));


    printTextValue(
        "Stock/NIFTY Fully Ready",
        readyText(
            state.fully_ready));
}


// ============================================================================
// Market regime display
// ============================================================================

void printMarketRegime(
    const MarketRegimeHorizon& state)
{
    std::cout
        << "\nMARKET REGIME\n"
        << "------------------------------------------------------------\n";


    printTextValue(
        "Trend State",
        trendStateText(
            state.trend_state));


    std::cout
        << std::left
        << std::setw(37)
        << "Trend Score"
        << " : "
        << state.trend_score
        << '\n';


    printTextValue(
        "Volatility State",
        volatilityStateText(
            state.volatility_state));


    printTextValue(
        "Statistical Stability",
        stabilityText(
            state.statistical_stability));


    printTextValue(
        "Final Market Regime",
        regimeText(
            state.regime));


    printValue(
        "ATR14 %",
        state.atr14_percent);


    printValue(
        "Realized Volatility 20",
        state.realized_volatility_20);


    printValue(
        "Return Z-Score",
        state.return_z_score);


    printValue(
        "Absolute Return Z-Score",
        state.absolute_return_z_score);


    printValue(
        "Residual Z-Score",
        state.residual_z_score);


    printValue(
        "Rolling Correlation",
        state.rolling_correlation);


    printTextValue(
        "Trend Ready",
        readyText(
            state.trend_ready));


    printTextValue(
        "Volatility Ready",
        readyText(
            state.volatility_ready));


    printTextValue(
        "Statistical Stability Ready",
        readyText(
            state.statistical_stability_ready));


    printTextValue(
        "Market Regime Ready",
        readyText(
            state.regime_ready));
}


// ============================================================================
// Complete mathematical horizon display
// ============================================================================

void printMathematicalHorizon(
    const std::string& timeframe,
    const MathematicalHorizonState& state)
{
    std::cout
        << "\n\n"
        << "============================================================\n"
        << timeframe
        << " MATHEMATICAL STATE\n"
        << "============================================================\n";


    printReturnStatistics(
        state.return_statistics);


    printVolumeStatistics(
        state.volume_statistics);


    printStockBenchmarkStatistics(
        state.stock_benchmark_statistics);


    printMarketRegime(
        state.market_regime);


    std::cout
        << "\nMATHEMATICAL READINESS\n"
        << "------------------------------------------------------------\n";


    printTextValue(
        "Return Statistics",
        readyText(
            state.readiness.return_statistics_ready));


    printTextValue(
        "Volume Statistics",
        readyText(
            state.readiness.volume_statistics_ready));


    printTextValue(
        "Stock/NIFTY Statistics",
        readyText(
            state.readiness.stock_benchmark_statistics_ready));


    printTextValue(
        "Market Regime",
        readyText(
            state.readiness.market_regime_ready));


    printTextValue(
        "Numerical Integrity",
        integrityText(
            state.readiness.numerical_integrity));


    printTextValue(
        "Final Horizon State",
        readyText(
            state.readiness.fully_ready));
}


// ============================================================================
// Complete Phase 4.6 calculated-state display
// ============================================================================

void printMathematicalState(
    const MathematicalStateFeatures& state)
{
    std::cout
        << "\n\n"
        << "============================================================\n"
        << "LATEST FULLY-READY PHASE-4 MATHEMATICAL STATE\n"
        << "============================================================\n";


    printTextValue(
        "Stock",
        state.stock_symbol);


    printTextValue(
        "Benchmark",
        state.benchmark_symbol);


    printTextValue(
        "Decision Time",
        formatTimestamp(
            state.decision_time));


    std::cout
        << std::left
        << std::setw(37)
        << "Decision Timestamp"
        << " : "
        << state.decision_time
        << '\n';


    printMathematicalHorizon(
        "1-MINUTE",
        state.one_minute);


    printMathematicalHorizon(
        "5-MINUTE",
        state.five_minute);


    printMathematicalHorizon(
        "15-MINUTE",
        state.fifteen_minute);


    std::cout
        << "\n\n"
        << "============================================================\n"
        << "FINAL PHASE-4 OBSERVATION\n"
        << "============================================================\n";


    printTextValue(
        "1-Minute State",
        readyText(
            state.one_minute_ready));


    printTextValue(
        "5-Minute State",
        readyText(
            state.five_minute_ready));


    printTextValue(
        "15-Minute State",
        readyText(
            state.fifteen_minute_ready));


    printTextValue(
        "Global Numerical Integrity",
        integrityText(
            state.numerical_integrity));


    printTextValue(
        "Complete Phase-4 State",
        readyText(
            state.fully_ready));


    std::cout
        << "============================================================\n";
}


// ============================================================================
// Real validation
// ============================================================================

int runValidation(
    const std::filesystem::path& data_folder,
    const std::string& stock_symbol,
    const std::string& benchmark_symbol)
{
    ValidationStats stats;


    // ========================================================================
    // Latest fully-ready state
    // ========================================================================

    MathematicalStateFeatures
        latest_fully_ready_state;


    bool has_latest_fully_ready_state =
        false;


    // ========================================================================
    // Header
    // ========================================================================

    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.6 — REAL MATHEMATICAL STATE VALIDATION VER2\n"
        << "============================================================\n"
        << "Data folder       : "
        << data_folder
        << '\n'
        << "Stock             : "
        << stock_symbol
        << '\n'
        << "Benchmark         : "
        << benchmark_symbol
        << '\n'
        << "Return window     : "
        << RETURN_WINDOW
        << '\n'
        << "Volume window     : "
        << VOLUME_WINDOW
        << '\n'
        << "Regression window : "
        << REGRESSION_WINDOW
        << '\n'
        << "Residual window   : "
        << RESIDUAL_WINDOW
        << '\n'
        << "Volume low Z      : "
        << VOLUME_LOW_Z_THRESHOLD
        << '\n'
        << "Volume high Z     : "
        << VOLUME_HIGH_Z_THRESHOLD
        << '\n'
        << "Volume extreme Z  : "
        << VOLUME_EXTREME_Z_THRESHOLD
        << '\n'
        << "Regime elevated Z : "
        << REGIME_ELEVATED_Z_THRESHOLD
        << '\n'
        << "Regime extreme Z  : "
        << REGIME_EXTREME_Z_THRESHOLD
        << '\n'
        << "============================================================\n";


    // ========================================================================
    // Load real data
    // ========================================================================

    MarketDataLoader loader(
        data_folder);


    const std::filesystem::path stock_file =
        data_folder /
        (
            stock_symbol +
            "_1min.txt"
        );


    const std::filesystem::path benchmark_file =
        data_folder /
        (
            benchmark_symbol +
            "_1min.txt"
        );


    const auto stock_load =
        loader.loadPath(
            stock_file);


    const auto benchmark_load =
        loader.loadPath(
            benchmark_file);


    if (stock_load.candles.empty())
    {
        throw std::runtime_error(
            "No stock candles loaded.");
    }


    if (benchmark_load.candles.empty())
    {
        throw std::runtime_error(
            "No benchmark candles loaded.");
    }


    // ========================================================================
    // Aggregate
    // ========================================================================

    CandleAggregator aggregator;


    const auto stock_5m =
        aggregator.aggregate(
            stock_load.candles,
            Timeframe::FIVE_MINUTES);


    const auto stock_15m =
        aggregator.aggregate(
            stock_load.candles,
            Timeframe::FIFTEEN_MINUTES);


    const auto benchmark_5m =
        aggregator.aggregate(
            benchmark_load.candles,
            Timeframe::FIVE_MINUTES);


    const auto benchmark_15m =
        aggregator.aggregate(
            benchmark_load.candles,
            Timeframe::FIFTEEN_MINUTES);


    // ========================================================================
    // Market-data report
    // ========================================================================

    std::cout
        << "\nMarket data\n"
        << "------------------------------------------------------------\n";


    printCountValue(
        "Stock 1m",
        stock_load.candles.size());

    printCountValue(
        "Stock 5m",
        stock_5m.candles.size());

    printCountValue(
        "Stock 15m",
        stock_15m.candles.size());

    printCountValue(
        "Stock incomplete 5m",
        stock_5m.incomplete_buckets);

    printCountValue(
        "Stock incomplete 15m",
        stock_15m.incomplete_buckets);


    printCountValue(
        "NIFTY 1m",
        benchmark_load.candles.size());

    printCountValue(
        "NIFTY 5m",
        benchmark_5m.candles.size());

    printCountValue(
        "NIFTY 15m",
        benchmark_15m.candles.size());

    printCountValue(
        "NIFTY incomplete 5m",
        benchmark_5m.incomplete_buckets);

    printCountValue(
        "NIFTY incomplete 15m",
        benchmark_15m.incomplete_buckets);


    // ========================================================================
    // Availability
    // ========================================================================

    MultiTimeframeSynchronizerConfig
        synchronizer_config;


    synchronizer_config.mode =
        AvailabilityMode::ZERO_LATENCY;


    MultiTimeframeSynchronizer synchronizer(
        synchronizer_config);


    const auto stock_1m_timed =
        synchronizer.prepareOneMinute(
            stock_load.candles);


    const auto stock_5m_timed =
        synchronizer.prepareFiveMinute(
            stock_5m.candles);


    const auto stock_15m_timed =
        synchronizer.prepareFifteenMinute(
            stock_15m.candles);


    const auto benchmark_1m_timed =
        synchronizer.prepareOneMinute(
            benchmark_load.candles);


    const auto benchmark_5m_timed =
        synchronizer.prepareFiveMinute(
            benchmark_5m.candles);


    const auto benchmark_15m_timed =
        synchronizer.prepareFifteenMinute(
            benchmark_15m.candles);


    // ========================================================================
    // Runtime
    // ========================================================================

    MultiTimeframeCursor stock_cursor(
        stock_1m_timed,
        stock_5m_timed,
        stock_15m_timed);


    MultiTimeframeCursor benchmark_cursor(
        benchmark_1m_timed,
        benchmark_5m_timed,
        benchmark_15m_timed);


    SessionState stock_session;
    SessionState benchmark_session;


    MarketSnapshotBuilder stock_builder(
        stock_symbol);


    MarketSnapshotBuilder benchmark_builder(
        benchmark_symbol);


    // ========================================================================
    // Phase 3 engines
    // ========================================================================

    PriceReturnFeatureEngine stock_return_engine(
        stock_symbol,
        64);


    PriceReturnFeatureEngine benchmark_return_engine(
        benchmark_symbol,
        64);


    TrendMomentumFeatureEngine trend_engine(
        stock_symbol);


    VolumeFeatureEngine volume_engine(
        stock_symbol);


    VolatilityFeatureEngine volatility_engine(
        stock_symbol);


    // ========================================================================
    // Phase 4 engines
    // ========================================================================

    ReturnStatisticalEngine return_statistical_engine(
        stock_symbol,
        RETURN_WINDOW,
        REGIME_EXTREME_Z_THRESHOLD);


    VolumeStatisticalEngine volume_statistical_engine(
        stock_symbol,
        VOLUME_WINDOW,
        VOLUME_LOW_Z_THRESHOLD,
        VOLUME_HIGH_Z_THRESHOLD,
        VOLUME_EXTREME_Z_THRESHOLD);


    StockBenchmarkStatisticalEngine
        stock_benchmark_engine(
            stock_symbol,
            benchmark_symbol,
            REGRESSION_WINDOW,
            RESIDUAL_WINDOW,
            REGIME_EXTREME_Z_THRESHOLD);


    MarketRegimeEngine market_regime_engine(
        stock_symbol,
        benchmark_symbol,
        REGIME_ELEVATED_Z_THRESHOLD,
        REGIME_EXTREME_Z_THRESHOLD);


    MathematicalStateComposer composer(
        stock_symbol,
        benchmark_symbol);


    // ========================================================================
    // Historical replay
    // ========================================================================

    for (const Candle& source :
         stock_load.candles)
    {
        ++stats.decision_points;


        const std::int64_t decision_time =
            source.timestamp +
            ONE_MINUTE_SECONDS;


        stock_cursor.advanceTo(
            decision_time);


        benchmark_cursor.advanceTo(
            decision_time);


        const MarketSnapshot stock_snapshot =
            stock_builder.build(
                decision_time,
                stock_cursor,
                stock_session);


        const MarketSnapshot benchmark_snapshot =
            benchmark_builder.build(
                decision_time,
                benchmark_cursor,
                benchmark_session);


        // ====================================================================
        // Session
        // ====================================================================

        if (stock_snapshot.session_phase !=
            SessionPhase::ACTIVE)
        {
            ++stats.incomplete_pairs;

            continue;
        }


        ++stats.active_session_points;


        if (stock_snapshot.new_session)
        {
            ++stats.new_sessions;
        }


        // ====================================================================
        // Look-ahead
        // ====================================================================

        if (hasLookahead(
                stock_snapshot) ||
            hasLookahead(
                benchmark_snapshot))
        {
            ++stats.lookahead_violations;
        }


        // ====================================================================
        // Require aligned 1m pair
        // ====================================================================

        if (!stock_snapshot.one_minute.has_value() ||
            !benchmark_snapshot.one_minute.has_value())
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


        // ====================================================================
        // Phase 3.1
        // ====================================================================

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
            ++stats.price_return_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 3.2
        // ====================================================================

        TrendMomentumFeatures trend_features;


        try
        {
            trend_features =
                trend_engine.update(
                    stock_snapshot);
        }
        catch (const std::exception&)
        {
            ++stats.trend_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 3.3
        // ====================================================================

        VolumeFeatures volume_features;


        try
        {
            volume_features =
                volume_engine.update(
                    stock_snapshot);
        }
        catch (const std::exception&)
        {
            ++stats.volume_feature_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 3.4
        // ====================================================================

        VolatilityFeatures volatility_features;


        try
        {
            volatility_features =
                volatility_engine.update(
                    stock_snapshot);
        }
        catch (const std::exception&)
        {
            ++stats.volatility_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 4.2
        // ====================================================================

        ReturnStatisticalFeatures
            return_statistics;


        try
        {
            return_statistics =
                return_statistical_engine.update(
                    stock_returns,
                    stock_snapshot);
        }
        catch (const std::exception&)
        {
            ++stats.return_statistical_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 4.3
        // ====================================================================

        VolumeStatisticalFeatures
            volume_statistics;


        try
        {
            volume_statistics =
                volume_statistical_engine.update(
                    volume_features,
                    stock_snapshot);
        }
        catch (const std::exception&)
        {
            ++stats.volume_statistical_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 3.6 relative context
        // ====================================================================

        const StockBenchmarkFeatures
            relative_features =
                makeRelativeFeatures(
                    stock_returns,
                    benchmark_returns);


        // ====================================================================
        // Phase 4.4
        // ====================================================================

        StockBenchmarkStatisticalFeatures
            stock_benchmark_statistics;


        try
        {
            stock_benchmark_statistics =
                stock_benchmark_engine.update(
                    stock_returns,
                    benchmark_returns,
                    relative_features,
                    stock_snapshot,
                    benchmark_snapshot);
        }
        catch (const std::exception&)
        {
            ++stats.stock_benchmark_statistical_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 4.5
        // ====================================================================

        MarketRegimeFeatures
            regime_features;


        try
        {
            regime_features =
                market_regime_engine.update(
                    trend_features,
                    volatility_features,
                    return_statistics,
                    stock_benchmark_statistics);
        }
        catch (const std::exception&)
        {
            ++stats.market_regime_exceptions;

            continue;
        }


        // ====================================================================
        // Phase 4.6
        // ====================================================================

        MathematicalStateFeatures
            mathematical_state;


        try
        {
            mathematical_state =
                composer.compose(
                    return_statistics,
                    volume_statistics,
                    stock_benchmark_statistics,
                    regime_features);
        }
        catch (const std::exception&)
        {
            ++stats.composer_exceptions;

            continue;
        }


        ++stats.composer_observations;


        // ====================================================================
        // Metadata
        // ====================================================================

        if (mathematical_state.stock_symbol !=
                stock_symbol ||

            mathematical_state.benchmark_symbol !=
                benchmark_symbol)
        {
            ++stats.symbol_violations;
        }


        if (mathematical_state.decision_time !=
                decision_time ||

            return_statistics.decision_time !=
                decision_time ||

            volume_statistics.decision_time !=
                decision_time ||

            stock_benchmark_statistics.decision_time !=
                decision_time ||

            regime_features.decision_time !=
                decision_time)
        {
            ++stats.decision_time_violations;
        }


        // ====================================================================
        // Lossless 4.2
        // ====================================================================

        if (!sameReturnStatistics(
                mathematical_state.one_minute.
                    return_statistics,
                return_statistics.one_minute) ||

            !sameReturnStatistics(
                mathematical_state.five_minute.
                    return_statistics,
                return_statistics.five_minute) ||

            !sameReturnStatistics(
                mathematical_state.fifteen_minute.
                    return_statistics,
                return_statistics.fifteen_minute))
        {
            ++stats.return_composition_mismatches;
        }


        // ====================================================================
        // Lossless 4.3
        // ====================================================================

        if (!sameVolumeStatistics(
                mathematical_state.one_minute.
                    volume_statistics,
                volume_statistics.one_minute) ||

            !sameVolumeStatistics(
                mathematical_state.five_minute.
                    volume_statistics,
                volume_statistics.five_minute) ||

            !sameVolumeStatistics(
                mathematical_state.fifteen_minute.
                    volume_statistics,
                volume_statistics.fifteen_minute))
        {
            ++stats.volume_composition_mismatches;
        }


        // ====================================================================
        // Lossless 4.4
        // ====================================================================

        if (!sameStockBenchmarkStatistics(
                mathematical_state.one_minute.
                    stock_benchmark_statistics,
                stock_benchmark_statistics.one_minute) ||

            !sameStockBenchmarkStatistics(
                mathematical_state.five_minute.
                    stock_benchmark_statistics,
                stock_benchmark_statistics.five_minute) ||

            !sameStockBenchmarkStatistics(
                mathematical_state.fifteen_minute.
                    stock_benchmark_statistics,
                stock_benchmark_statistics.fifteen_minute))
        {
            ++stats.stock_benchmark_composition_mismatches;
        }


        // ====================================================================
        // Lossless 4.5
        // ====================================================================

        if (!sameMarketRegime(
                mathematical_state.one_minute.
                    market_regime,
                regime_features.one_minute) ||

            !sameMarketRegime(
                mathematical_state.five_minute.
                    market_regime,
                regime_features.five_minute) ||

            !sameMarketRegime(
                mathematical_state.fifteen_minute.
                    market_regime,
                regime_features.fifteen_minute))
        {
            ++stats.regime_composition_mismatches;
        }


        // ====================================================================
        // Expected readiness
        // ====================================================================

        const bool expected_one_minute =
            expectedHorizonReady(
                return_statistics.one_minute,
                volume_statistics.one_minute,
                stock_benchmark_statistics.one_minute,
                regime_features.one_minute,
                mathematical_state.one_minute.
                    readiness.numerical_integrity);


        const bool expected_five_minute =
            expectedHorizonReady(
                return_statistics.five_minute,
                volume_statistics.five_minute,
                stock_benchmark_statistics.five_minute,
                regime_features.five_minute,
                mathematical_state.five_minute.
                    readiness.numerical_integrity);


        const bool expected_fifteen_minute =
            expectedHorizonReady(
                return_statistics.fifteen_minute,
                volume_statistics.fifteen_minute,
                stock_benchmark_statistics.fifteen_minute,
                regime_features.fifteen_minute,
                mathematical_state.fifteen_minute.
                    readiness.numerical_integrity);


        // ====================================================================
        // Readiness validation
        // ====================================================================

        if (mathematical_state.one_minute_ready !=
                expected_one_minute ||

            mathematical_state.one_minute.
                readiness.fully_ready !=
                expected_one_minute)
        {
            ++stats.one_minute_readiness_mismatches;
        }


        if (mathematical_state.five_minute_ready !=
                expected_five_minute ||

            mathematical_state.five_minute.
                readiness.fully_ready !=
                expected_five_minute)
        {
            ++stats.five_minute_readiness_mismatches;
        }


        if (mathematical_state.fifteen_minute_ready !=
                expected_fifteen_minute ||

            mathematical_state.fifteen_minute.
                readiness.fully_ready !=
                expected_fifteen_minute)
        {
            ++stats.fifteen_minute_readiness_mismatches;
        }


        // ====================================================================
        // Readiness counts
        // ====================================================================

        if (mathematical_state.one_minute_ready)
        {
            ++stats.one_minute_ready;
        }


        if (mathematical_state.five_minute_ready)
        {
            ++stats.five_minute_ready;
        }


        if (mathematical_state.fifteen_minute_ready)
        {
            ++stats.fifteen_minute_ready;
        }


        // ====================================================================
        // Global numerical integrity
        // ====================================================================

        const bool expected_numerical_integrity =
            mathematical_state.one_minute.
                readiness.numerical_integrity &&

            mathematical_state.five_minute.
                readiness.numerical_integrity &&

            mathematical_state.fifteen_minute.
                readiness.numerical_integrity;


        if (mathematical_state.numerical_integrity !=
            expected_numerical_integrity)
        {
            ++stats.global_integrity_mismatches;
        }


        if (mathematical_state.numerical_integrity)
        {
            ++stats.numerical_integrity_passed;
        }
        else
        {
            ++stats.numerical_integrity_failed;
        }


        // ====================================================================
        // Global Phase-4 readiness
        // ====================================================================

        const bool expected_fully_ready =
            expected_one_minute &&
            expected_five_minute &&
            expected_fifteen_minute &&
            mathematical_state.numerical_integrity;


        if (mathematical_state.fully_ready !=
            expected_fully_ready)
        {
            ++stats.global_readiness_mismatches;
        }


        // ====================================================================
        // Store latest fully-ready real mathematical state
        // ====================================================================

        if (mathematical_state.fully_ready)
        {
            ++stats.fully_ready;


            latest_fully_ready_state =
                mathematical_state;


            has_latest_fully_ready_state =
                true;
        }
    }


    // ========================================================================
    // Display actual calculated mathematical state
    // ========================================================================

    if (has_latest_fully_ready_state)
    {
        printMathematicalState(
            latest_fully_ready_state);
    }
    else
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "NO FULLY-READY PHASE-4 OBSERVATION AVAILABLE\n"
            << "============================================================\n";
    }


    // ========================================================================
    // Validation report
    // ========================================================================

    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.6 COMPOSITION VALIDATION VER2\n"
        << "============================================================\n";


    printCountValue(
        "Composer observations",
        stats.composer_observations);


    printCountValue(
        "1m ready",
        stats.one_minute_ready);


    printCountValue(
        "5m ready",
        stats.five_minute_ready);


    printCountValue(
        "15m ready",
        stats.fifteen_minute_ready);


    printCountValue(
        "Fully-ready Phase 4 observations",
        stats.fully_ready);


    printCountValue(
        "Numerical integrity passed",
        stats.numerical_integrity_passed);


    printCountValue(
        "Numerical integrity failed",
        stats.numerical_integrity_failed);


    // ========================================================================
    // Composition integrity
    // ========================================================================

    std::cout
        << "\nComposition integrity\n"
        << "------------------------------------------------------------\n";


    printCountValue(
        "Return composition mismatches",
        stats.return_composition_mismatches);


    printCountValue(
        "Volume composition mismatches",
        stats.volume_composition_mismatches);


    printCountValue(
        "Stock/NIFTY composition mismatches",
        stats.stock_benchmark_composition_mismatches);


    printCountValue(
        "Regime composition mismatches",
        stats.regime_composition_mismatches);


    // ========================================================================
    // Readiness integrity
    // ========================================================================

    std::cout
        << "\nReadiness integrity\n"
        << "------------------------------------------------------------\n";


    printCountValue(
        "1m readiness mismatches",
        stats.one_minute_readiness_mismatches);


    printCountValue(
        "5m readiness mismatches",
        stats.five_minute_readiness_mismatches);


    printCountValue(
        "15m readiness mismatches",
        stats.fifteen_minute_readiness_mismatches);


    printCountValue(
        "Global integrity mismatches",
        stats.global_integrity_mismatches);


    printCountValue(
        "Global readiness mismatches",
        stats.global_readiness_mismatches);


    // ========================================================================
    // Runtime / safety
    // ========================================================================

    std::cout
        << "\nRuntime / safety\n"
        << "------------------------------------------------------------\n";


    printCountValue(
        "Decision points",
        stats.decision_points);


    printCountValue(
        "Active-session points",
        stats.active_session_points);


    printCountValue(
        "Synchronized points",
        stats.synchronized_points);


    printCountValue(
        "Incomplete pairs",
        stats.incomplete_pairs);


    printCountValue(
        "New sessions",
        stats.new_sessions);


    printCountValue(
        "Look-ahead violations",
        stats.lookahead_violations);


    printCountValue(
        "Decision-time violations",
        stats.decision_time_violations);


    printCountValue(
        "Symbol violations",
        stats.symbol_violations);


    printCountValue(
        "Timestamp-alignment violations",
        stats.timestamp_alignment_violations);


    // ========================================================================
    // Exceptions
    // ========================================================================

    std::cout
        << "\nExceptions\n"
        << "------------------------------------------------------------\n";


    printCountValue(
        "Price-return exceptions",
        stats.price_return_exceptions);


    printCountValue(
        "Trend exceptions",
        stats.trend_exceptions);


    printCountValue(
        "Volatility exceptions",
        stats.volatility_exceptions);


    printCountValue(
        "Volume-feature exceptions",
        stats.volume_feature_exceptions);


    printCountValue(
        "Return-statistical exceptions",
        stats.return_statistical_exceptions);


    printCountValue(
        "Volume-statistical exceptions",
        stats.volume_statistical_exceptions);


    printCountValue(
        "Stock/NIFTY statistical exceptions",
        stats.stock_benchmark_statistical_exceptions);


    printCountValue(
        "Market-regime exceptions",
        stats.market_regime_exceptions);


    printCountValue(
        "Composer exceptions",
        stats.composer_exceptions);


    // ========================================================================
    // Final result
    // ========================================================================

    std::cout
        << "\n"
        << "============================================================\n";


    if (stats.passed())
    {
        std::cout
            << "PHASE 4.6 REAL MATHEMATICAL STATE VALIDATION VER2 PASSED\n"
            << "============================================================\n"
            << "Calculated State Display            : PASSED\n"
            << "Lossless Phase 4.2 Composition      : PASSED\n"
            << "Lossless Phase 4.3 Composition      : PASSED\n"
            << "Lossless Phase 4.4 Composition      : PASSED\n"
            << "Lossless Phase 4.5 Composition      : PASSED\n"
            << "1m Readiness Integrity              : PASSED\n"
            << "5m Readiness Integrity              : PASSED\n"
            << "15m Readiness Integrity             : PASSED\n"
            << "Global Numerical Integrity          : PASSED\n"
            << "Final Phase-4 Readiness             : PASSED\n"
            << "Look-Ahead Protection               : PASSED\n"
            << "Decision-Time Integrity             : PASSED\n"
            << "Symbol Integrity                    : PASSED\n"
            << "============================================================\n";


        return
            EXIT_SUCCESS;
    }


    std::cout
        << "PHASE 4.6 REAL MATHEMATICAL STATE VALIDATION VER2 FAILED\n"
        << "============================================================\n";


    return
        EXIT_FAILURE;
}

} // namespace


// ============================================================================
// main
// ============================================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        const std::filesystem::path data_folder =
            argc >= 2
                ? std::filesystem::path(
                    argv[1])
                : std::filesystem::path(
                    DEFAULT_DATA_FOLDER);


        const std::string stock_symbol =
            argc >= 3
                ? argv[2]
                : DEFAULT_STOCK_SYMBOL;


        const std::string benchmark_symbol =
            argc >= 4
                ? argv[3]
                : DEFAULT_BENCHMARK_SYMBOL;


        return
            runValidation(
                data_folder,
                stock_symbol,
                benchmark_symbol);
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "PHASE 4.6 REAL VALIDATION VER2 ERROR\n"
            << exception.what()
            << '\n';


        return
            EXIT_FAILURE;
    }
}