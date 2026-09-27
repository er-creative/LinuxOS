#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"
#include "devai/features/TrendMomentumFeatureEngine.hpp"
#include "devai/features/TrendMomentumFeatures.hpp"
#include "devai/features/VolatilityFeatureEngine.hpp"
#include "devai/features/VolatilityFeatures.hpp"

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
#include "devai/statistics/MarketRegimeFeatures.hpp"
#include "devai/statistics/ReturnStatisticalEngine.hpp"
#include "devai/statistics/ReturnStatisticalFeatures.hpp"
#include "devai/statistics/StockBenchmarkStatisticalEngine.hpp"
#include "devai/statistics/StockBenchmarkStatisticalFeatures.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
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


constexpr std::size_t PRICE_HISTORY =
    64;

constexpr std::size_t RETURN_STATISTICAL_WINDOW =
    60;

constexpr std::size_t REGRESSION_WINDOW =
    60;

constexpr std::size_t RESIDUAL_WINDOW =
    60;


constexpr double ELEVATED_Z_THRESHOLD =
    1.0;

constexpr double EXTREME_Z_THRESHOLD =
    2.0;

constexpr double NUMERIC_TOLERANCE =
    1.0e-9;


constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr std::int64_t FIVE_MINUTE_SECONDS =
    300;

constexpr std::int64_t FIFTEEN_MINUTE_SECONDS =
    900;


// ============================================================================
// Per-timeframe validation
// ============================================================================

struct HorizonValidation
{
    std::size_t observations{0};

    std::size_t ready{0};
    std::size_t unavailable{0};

    std::size_t strong_uptrend{0};
    std::size_t uptrend{0};
    std::size_t neutral_trend{0};
    std::size_t downtrend{0};
    std::size_t strong_downtrend{0};

    std::size_t low_volatility{0};
    std::size_t normal_volatility{0};
    std::size_t high_volatility{0};
    std::size_t extreme_volatility{0};

    std::size_t stable{0};
    std::size_t elevated{0};
    std::size_t unstable{0};

    std::size_t regime_trending_up{0};
    std::size_t regime_trending_down{0};
    std::size_t regime_range_bound{0};
    std::size_t regime_high_volatility{0};
    std::size_t regime_unstable{0};

    std::size_t trend_mismatches{0};
    std::size_t volatility_mismatches{0};
    std::size_t stability_mismatches{0};
    std::size_t regime_mismatches{0};

    std::size_t readiness_mismatches{0};
    std::size_t evidence_mismatches{0};
    std::size_t numeric_violations{0};
    std::size_t trend_score_violations{0};
};


// ============================================================================
// Complete validation state
// ============================================================================

struct ValidationStats
{
    std::size_t decision_points{0};
    std::size_t active_session_points{0};
    std::size_t synchronized_points{0};
    std::size_t incomplete_pairs{0};
    std::size_t new_sessions{0};

    std::size_t fully_ready{0};

    std::size_t lookahead_violations{0};
    std::size_t decision_time_violations{0};
    std::size_t symbol_violations{0};
    std::size_t timestamp_alignment_violations{0};

    std::size_t top_level_readiness_mismatches{0};

    std::size_t price_feature_exceptions{0};
    std::size_t trend_feature_exceptions{0};
    std::size_t volatility_feature_exceptions{0};

    std::size_t return_statistical_exceptions{0};
    std::size_t stock_benchmark_statistical_exceptions{0};
    std::size_t market_regime_exceptions{0};

    HorizonValidation one_minute;
    HorizonValidation five_minute;
    HorizonValidation fifteen_minute;
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
// Look-ahead validation
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
// Phase 4.4 only requires the stock-vs-benchmark relative-return fields here.
// ============================================================================

StockBenchmarkFeatures makeRelativeFeatures(
    const PriceReturnFeatures& stock,
    const PriceReturnFeatures& benchmark)
{
    if (stock.decision_time !=
        benchmark.decision_time)
    {
        throw std::runtime_error(
            "Price-return decision-time mismatch."
        );
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
// Independent expected trend classification
// ============================================================================

TrendState expectedTrend(
    const TimeframeTrendMomentumFeatures& trend,
    int& score)
{
    score =
        0;


    const bool ready =
        trend.has_ema20 &&
        trend.has_ema50 &&
        trend.has_ema20_slope &&
        trend.has_ema50_slope &&
        std::isfinite(trend.ema20) &&
        std::isfinite(trend.ema50) &&
        std::isfinite(trend.price_vs_ema20) &&
        std::isfinite(trend.ema20_vs_ema50) &&
        std::isfinite(trend.ema20_slope) &&
        std::isfinite(trend.ema50_slope);


    if (!ready)
    {
        return
            TrendState::UNAVAILABLE;
    }


    if (trend.ema20_vs_ema50 > 0.0)
    {
        ++score;
    }
    else if (trend.ema20_vs_ema50 < 0.0)
    {
        --score;
    }


    if (trend.price_vs_ema20 > 0.0)
    {
        ++score;
    }
    else if (trend.price_vs_ema20 < 0.0)
    {
        --score;
    }


    if (trend.ema20_slope > 0.0)
    {
        ++score;
    }
    else if (trend.ema20_slope < 0.0)
    {
        --score;
    }


    if (trend.ema50_slope > 0.0)
    {
        ++score;
    }
    else if (trend.ema50_slope < 0.0)
    {
        --score;
    }


    if (score >= 3)
    {
        return
            TrendState::STRONG_UPTREND;
    }


    if (score >= 1)
    {
        return
            TrendState::UPTREND;
    }


    if (score <= -3)
    {
        return
            TrendState::STRONG_DOWNTREND;
    }


    if (score <= -1)
    {
        return
            TrendState::DOWNTREND;
    }


    return
        TrendState::NEUTRAL;
}


// ============================================================================
// Independent expected volatility classification
// ============================================================================

VolatilityState expectedVolatility(
    const TimeframeVolatilityFeatures& volatility,
    const ReturnHorizonStatistics& returns)
{
    const bool volatility_ready =
        volatility.has_atr14_percent &&
        volatility.has_realized_volatility_20 &&
        std::isfinite(
            volatility.atr14_percent) &&
        std::isfinite(
            volatility.realized_volatility_20);


    const bool statistics_ready =
        returns.ready &&
        returns.has_observation &&
        std::isfinite(
            returns.absolute_return_z_score);


    if (!volatility_ready ||
        !statistics_ready)
    {
        return
            VolatilityState::UNAVAILABLE;
    }


    const double z =
        returns.absolute_return_z_score;


    if (z >=
        EXTREME_Z_THRESHOLD)
    {
        return
            VolatilityState::EXTREME;
    }


    if (z >=
        ELEVATED_Z_THRESHOLD)
    {
        return
            VolatilityState::HIGH;
    }


    if (z <=
        -ELEVATED_Z_THRESHOLD)
    {
        return
            VolatilityState::LOW;
    }


    return
        VolatilityState::NORMAL;
}


// ============================================================================
// Independent expected statistical stability
// ============================================================================

StatisticalStability expectedStability(
    const ReturnHorizonStatistics& returns,
    const StockBenchmarkHorizonStatistics& benchmark)
{
    const bool return_ready =
        returns.ready &&
        returns.has_observation &&
        std::isfinite(returns.z_score) &&
        std::isfinite(returns.robust_z_score);


    const bool residual_ready =
        benchmark.residual_statistics_ready &&
        benchmark.regression_ready &&
        std::isfinite(
            benchmark.residual_z_score) &&
        std::isfinite(
            benchmark.residual_robust_z_score);


    if (!return_ready ||
        !residual_ready)
    {
        return
            StatisticalStability::UNAVAILABLE;
    }


    const double return_displacement =
        std::max(
            std::fabs(
                returns.z_score),
            std::fabs(
                returns.robust_z_score));


    const double residual_displacement =
        std::max(
            std::fabs(
                benchmark.residual_z_score),
            std::fabs(
                benchmark.residual_robust_z_score));


    const double maximum_displacement =
        std::max(
            return_displacement,
            residual_displacement);


    if (maximum_displacement >=
        EXTREME_Z_THRESHOLD)
    {
        return
            StatisticalStability::UNSTABLE;
    }


    if (maximum_displacement >=
        ELEVATED_Z_THRESHOLD)
    {
        return
            StatisticalStability::ELEVATED;
    }


    return
        StatisticalStability::STABLE;
}


// ============================================================================
// Independent expected final regime
// ============================================================================

MarketRegime expectedRegime(
    TrendState trend,
    VolatilityState volatility,
    StatisticalStability stability)
{
    if (trend ==
            TrendState::UNAVAILABLE ||
        volatility ==
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


    if (volatility ==
            VolatilityState::HIGH ||
        volatility ==
            VolatilityState::EXTREME)
    {
        return
            MarketRegime::HIGH_VOLATILITY;
    }


    if (trend ==
            TrendState::UPTREND ||
        trend ==
            TrendState::STRONG_UPTREND)
    {
        return
            MarketRegime::TRENDING_UP;
    }


    if (trend ==
            TrendState::DOWNTREND ||
        trend ==
            TrendState::STRONG_DOWNTREND)
    {
        return
            MarketRegime::TRENDING_DOWN;
    }


    return
        MarketRegime::RANGE_BOUND;
}


// ============================================================================
// Count classifications
// ============================================================================

void countTrend(
    TrendState state,
    HorizonValidation& stats)
{
    switch (state)
    {
        case TrendState::STRONG_UPTREND:
            ++stats.strong_uptrend;
            break;

        case TrendState::UPTREND:
            ++stats.uptrend;
            break;

        case TrendState::NEUTRAL:
            ++stats.neutral_trend;
            break;

        case TrendState::DOWNTREND:
            ++stats.downtrend;
            break;

        case TrendState::STRONG_DOWNTREND:
            ++stats.strong_downtrend;
            break;

        case TrendState::UNAVAILABLE:
            break;
    }
}


void countVolatility(
    VolatilityState state,
    HorizonValidation& stats)
{
    switch (state)
    {
        case VolatilityState::LOW:
            ++stats.low_volatility;
            break;

        case VolatilityState::NORMAL:
            ++stats.normal_volatility;
            break;

        case VolatilityState::HIGH:
            ++stats.high_volatility;
            break;

        case VolatilityState::EXTREME:
            ++stats.extreme_volatility;
            break;

        case VolatilityState::UNAVAILABLE:
            break;
    }
}


void countStability(
    StatisticalStability state,
    HorizonValidation& stats)
{
    switch (state)
    {
        case StatisticalStability::STABLE:
            ++stats.stable;
            break;

        case StatisticalStability::ELEVATED:
            ++stats.elevated;
            break;

        case StatisticalStability::UNSTABLE:
            ++stats.unstable;
            break;

        case StatisticalStability::UNAVAILABLE:
            break;
    }
}


void countRegime(
    MarketRegime state,
    HorizonValidation& stats)
{
    switch (state)
    {
        case MarketRegime::TRENDING_UP:
            ++stats.regime_trending_up;
            break;

        case MarketRegime::TRENDING_DOWN:
            ++stats.regime_trending_down;
            break;

        case MarketRegime::RANGE_BOUND:
            ++stats.regime_range_bound;
            break;

        case MarketRegime::HIGH_VOLATILITY:
            ++stats.regime_high_volatility;
            break;

        case MarketRegime::UNSTABLE:
            ++stats.regime_unstable;
            break;

        case MarketRegime::UNAVAILABLE:
            ++stats.unavailable;
            break;
    }
}


// ============================================================================
// Validate numerical evidence
// ============================================================================

void validateEvidence(
    const MarketRegimeHorizon& actual,
    const TimeframeVolatilityFeatures& volatility,
    const ReturnHorizonStatistics& returns,
    const StockBenchmarkHorizonStatistics& benchmark,
    HorizonValidation& stats)
{
    if (volatility.has_atr14_percent &&
        std::isfinite(
            volatility.atr14_percent))
    {
        if (!approximatelyEqual(
                actual.atr14_percent,
                volatility.atr14_percent))
        {
            ++stats.evidence_mismatches;
        }
    }


    if (volatility.has_realized_volatility_20 &&
        std::isfinite(
            volatility.realized_volatility_20))
    {
        if (!approximatelyEqual(
                actual.realized_volatility_20,
                volatility.realized_volatility_20))
        {
            ++stats.evidence_mismatches;
        }
    }


    if (returns.has_observation &&
        std::isfinite(
            returns.z_score))
    {
        if (!approximatelyEqual(
                actual.return_z_score,
                returns.z_score))
        {
            ++stats.evidence_mismatches;
        }
    }


    if (returns.has_observation &&
        std::isfinite(
            returns.absolute_return_z_score))
    {
        if (!approximatelyEqual(
                actual.absolute_return_z_score,
                returns.absolute_return_z_score))
        {
            ++stats.evidence_mismatches;
        }
    }


    if (benchmark.regression_ready &&
        std::isfinite(
            benchmark.residual_z_score))
    {
        if (!approximatelyEqual(
                actual.residual_z_score,
                benchmark.residual_z_score))
        {
            ++stats.evidence_mismatches;
        }
    }


    if (benchmark.regression_ready &&
        std::isfinite(
            benchmark.rolling_correlation))
    {
        if (!approximatelyEqual(
                actual.rolling_correlation,
                benchmark.rolling_correlation))
        {
            ++stats.evidence_mismatches;
        }
    }
}


// ============================================================================
// Validate one timeframe
// ============================================================================

void validateHorizon(
    const MarketRegimeHorizon& actual,
    const TimeframeTrendMomentumFeatures& trend,
    const TimeframeVolatilityFeatures& volatility,
    const ReturnHorizonStatistics& returns,
    const StockBenchmarkHorizonStatistics& benchmark,
    HorizonValidation& stats)
{
    ++stats.observations;


    int expected_trend_score =
        0;


    const TrendState expected_trend =
        expectedTrend(
            trend,
            expected_trend_score);


    const VolatilityState expected_volatility =
        expectedVolatility(
            volatility,
            returns);


    const StatisticalStability expected_stability =
        expectedStability(
            returns,
            benchmark);


    const MarketRegime expected_regime =
        expectedRegime(
            expected_trend,
            expected_volatility,
            expected_stability);


    // ------------------------------------------------------------------------
    // Classification comparison
    // ------------------------------------------------------------------------

    if (actual.trend_state !=
        expected_trend)
    {
        ++stats.trend_mismatches;
    }


    if (actual.trend_score !=
        expected_trend_score)
    {
        ++stats.trend_score_violations;
    }


    if (actual.volatility_state !=
        expected_volatility)
    {
        ++stats.volatility_mismatches;
    }


    if (actual.statistical_stability !=
        expected_stability)
    {
        ++stats.stability_mismatches;
    }


    if (actual.regime !=
        expected_regime)
    {
        ++stats.regime_mismatches;
    }


    // ------------------------------------------------------------------------
    // Readiness
    // ------------------------------------------------------------------------

    const bool expected_trend_ready =
        expected_trend !=
        TrendState::UNAVAILABLE;


    const bool expected_volatility_ready =
        expected_volatility !=
        VolatilityState::UNAVAILABLE;


    const bool expected_stability_ready =
        expected_stability !=
        StatisticalStability::UNAVAILABLE;


    const bool expected_regime_ready =
        expected_regime !=
        MarketRegime::UNAVAILABLE;


    if (actual.trend_ready !=
            expected_trend_ready ||
        actual.volatility_ready !=
            expected_volatility_ready ||
        actual.statistical_stability_ready !=
            expected_stability_ready ||
        actual.regime_ready !=
            expected_regime_ready)
    {
        ++stats.readiness_mismatches;
    }


    if (actual.regime_ready)
    {
        ++stats.ready;
    }


    // ------------------------------------------------------------------------
    // Evidence
    // ------------------------------------------------------------------------

    validateEvidence(
        actual,
        volatility,
        returns,
        benchmark,
        stats);


    // ------------------------------------------------------------------------
    // Numeric integrity
    // ------------------------------------------------------------------------

    if (actual.trend_score < -4 ||
        actual.trend_score > 4)
    {
        ++stats.numeric_violations;
    }


    if (std::isinf(
            actual.atr14_percent) ||
        std::isinf(
            actual.realized_volatility_20) ||
        std::isinf(
            actual.return_z_score) ||
        std::isinf(
            actual.absolute_return_z_score) ||
        std::isinf(
            actual.residual_z_score) ||
        std::isinf(
            actual.rolling_correlation))
    {
        ++stats.numeric_violations;
    }


    if (std::isfinite(
            actual.rolling_correlation) &&
        (
            actual.rolling_correlation <
                -1.000000001 ||
            actual.rolling_correlation >
                1.000000001
        ))
    {
        ++stats.numeric_violations;
    }


    // ------------------------------------------------------------------------
    // Distribution
    // ------------------------------------------------------------------------

    countTrend(
        actual.trend_state,
        stats);

    countVolatility(
        actual.volatility_state,
        stats);

    countStability(
        actual.statistical_stability,
        stats);

    countRegime(
        actual.regime,
        stats);
}


// ============================================================================
// Horizon pass/fail
// ============================================================================

bool horizonPassed(
    const HorizonValidation& stats)
{
    return
        stats.observations > 0 &&
        stats.ready > 0 &&
        stats.trend_mismatches == 0 &&
        stats.volatility_mismatches == 0 &&
        stats.stability_mismatches == 0 &&
        stats.regime_mismatches == 0 &&
        stats.readiness_mismatches == 0 &&
        stats.evidence_mismatches == 0 &&
        stats.numeric_violations == 0 &&
        stats.trend_score_violations == 0;
}


// ============================================================================
// Print horizon
// ============================================================================

void printHorizon(
    const std::string& name,
    const HorizonValidation& stats)
{
    std::cout
        << "\n"
        << name
        << "\n"
        << "------------------------------------------------------------\n"

        << "Observations                    : "
        << stats.observations
        << "\n"

        << "Regime ready                    : "
        << stats.ready
        << "\n"

        << "Unavailable                     : "
        << stats.unavailable
        << "\n"

        << "\nTrend distribution\n"

        << "Strong uptrend                  : "
        << stats.strong_uptrend
        << "\n"

        << "Uptrend                         : "
        << stats.uptrend
        << "\n"

        << "Neutral                         : "
        << stats.neutral_trend
        << "\n"

        << "Downtrend                       : "
        << stats.downtrend
        << "\n"

        << "Strong downtrend                : "
        << stats.strong_downtrend
        << "\n"

        << "\nVolatility distribution\n"

        << "Low                             : "
        << stats.low_volatility
        << "\n"

        << "Normal                          : "
        << stats.normal_volatility
        << "\n"

        << "High                            : "
        << stats.high_volatility
        << "\n"

        << "Extreme                         : "
        << stats.extreme_volatility
        << "\n"

        << "\nStatistical stability\n"

        << "Stable                          : "
        << stats.stable
        << "\n"

        << "Elevated                        : "
        << stats.elevated
        << "\n"

        << "Unstable                        : "
        << stats.unstable
        << "\n"

        << "\nFinal regime distribution\n"

        << "Trending up                     : "
        << stats.regime_trending_up
        << "\n"

        << "Trending down                   : "
        << stats.regime_trending_down
        << "\n"

        << "Range bound                     : "
        << stats.regime_range_bound
        << "\n"

        << "High volatility                 : "
        << stats.regime_high_volatility
        << "\n"

        << "Unstable                        : "
        << stats.regime_unstable
        << "\n"

        << "\nIntegrity\n"

        << "Trend mismatches                : "
        << stats.trend_mismatches
        << "\n"

        << "Trend-score mismatches          : "
        << stats.trend_score_violations
        << "\n"

        << "Volatility mismatches           : "
        << stats.volatility_mismatches
        << "\n"

        << "Stability mismatches            : "
        << stats.stability_mismatches
        << "\n"

        << "Regime mismatches               : "
        << stats.regime_mismatches
        << "\n"

        << "Readiness mismatches            : "
        << stats.readiness_mismatches
        << "\n"

        << "Evidence mismatches             : "
        << stats.evidence_mismatches
        << "\n"

        << "Numeric violations              : "
        << stats.numeric_violations
        << "\n";
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
        // ====================================================================
        // Arguments
        // ====================================================================

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
            << "PHASE 4.5 — REAL MARKET REGIME VALIDATION\n"
            << "============================================================\n"
            << "Data folder       : "
            << data_folder
            << "\n"
            << "Stock             : "
            << stock_symbol
            << "\n"
            << "Benchmark         : "
            << benchmark_symbol
            << "\n"
            << "Return window     : "
            << RETURN_STATISTICAL_WINDOW
            << "\n"
            << "Regression window : "
            << REGRESSION_WINDOW
            << "\n"
            << "Residual window   : "
            << RESIDUAL_WINDOW
            << "\n"
            << "Elevated Z        : "
            << ELEVATED_Z_THRESHOLD
            << "\n"
            << "Extreme Z         : "
            << EXTREME_Z_THRESHOLD
            << "\n"
            << "============================================================\n";


        // ====================================================================
        // File validation
        // ====================================================================

        if (!std::filesystem::exists(
                stock_file))
        {
            throw std::runtime_error(
                "Stock file not found: " +
                stock_file.string()
            );
        }


        if (!std::filesystem::exists(
                benchmark_file))
        {
            throw std::runtime_error(
                "Benchmark file not found: " +
                benchmark_file.string()
            );
        }


        // ====================================================================
        // Load real data
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
                "Stock or benchmark data is empty."
            );
        }


        // ====================================================================
        // Aggregate real 1m -> 5m / 15m
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

        MultiTimeframeSynchronizerConfig
            synchronizer_config;

        synchronizer_config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer synchronizer(
            synchronizer_config);


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
        // Phase 3 engines
        // ====================================================================

        PriceReturnFeatureEngine
            stock_price_return_engine(
                stock_symbol,
                PRICE_HISTORY);


        PriceReturnFeatureEngine
            benchmark_price_return_engine(
                benchmark_symbol,
                PRICE_HISTORY);


        TrendMomentumFeatureEngine
            trend_engine(
                stock_symbol);


        VolatilityFeatureEngine
            volatility_engine(
                stock_symbol);


        // ====================================================================
        // Phase 4.2
        // ====================================================================

        ReturnStatisticalEngine
            return_statistical_engine(
                stock_symbol,
                RETURN_STATISTICAL_WINDOW,
                EXTREME_Z_THRESHOLD);


        // ====================================================================
        // Phase 4.4
        // ====================================================================

        StockBenchmarkStatisticalEngine
            stock_benchmark_statistical_engine(
                stock_symbol,
                benchmark_symbol,
                REGRESSION_WINDOW,
                RESIDUAL_WINDOW,
                EXTREME_Z_THRESHOLD);


        // ====================================================================
        // Phase 4.5
        // ====================================================================

        MarketRegimeEngine
            market_regime_engine(
                stock_symbol,
                benchmark_symbol,
                ELEVATED_Z_THRESHOLD,
                EXTREME_Z_THRESHOLD);


        ValidationStats stats;


        // ====================================================================
        // Real runtime replay
        //
        // Each real stock 1-minute candle completion is a decision point.
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
            // Stock/NIFTY 1m synchronization
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
            // Phase 3.1 Price / Return
            // ================================================================

            PriceReturnFeatures stock_returns;
            PriceReturnFeatures benchmark_returns;


            try
            {
                stock_returns =
                    stock_price_return_engine.update(
                        stock_snapshot);


                benchmark_returns =
                    benchmark_price_return_engine.update(
                        benchmark_snapshot);
            }
            catch (const std::exception&)
            {
                ++stats.price_feature_exceptions;
                continue;
            }


            // ================================================================
            // Phase 3.2 Trend / Momentum
            // ================================================================

            TrendMomentumFeatures
                trend_features;


            try
            {
                trend_features =
                    trend_engine.update(
                        stock_snapshot);
            }
            catch (const std::exception&)
            {
                ++stats.trend_feature_exceptions;
                continue;
            }


            // ================================================================
            // Phase 3.4 Volatility
            // ================================================================

            VolatilityFeatures
                volatility_features;


            try
            {
                volatility_features =
                    volatility_engine.update(
                        stock_snapshot);
            }
            catch (const std::exception&)
            {
                ++stats.volatility_feature_exceptions;
                continue;
            }


            // ================================================================
            // Phase 4.2 Return statistics
            // ================================================================

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


            // ================================================================
            // Phase 3.6 relative-return context required by Phase 4.4
            // ================================================================

            const StockBenchmarkFeatures
                relative_features =
                    makeRelativeFeatures(
                        stock_returns,
                        benchmark_returns);


            // ================================================================
            // Phase 4.4 Stock / NIFTY statistics
            // ================================================================

            StockBenchmarkStatisticalFeatures
                stock_benchmark_statistics;


            try
            {
                stock_benchmark_statistics =
                    stock_benchmark_statistical_engine.update(
                        stock_returns,
                        benchmark_returns,
                        relative_features,
                        stock_snapshot,
                        benchmark_snapshot);
            }
            catch (const std::exception&)
            {
                ++stats.
                    stock_benchmark_statistical_exceptions;

                continue;
            }


            // ================================================================
            // Phase 4.5 Market Regime
            // ================================================================

            MarketRegimeFeatures
                market_regime;


            try
            {
                market_regime =
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


            // ================================================================
            // Phase 4.5 metadata integrity
            // ================================================================

            if (market_regime.stock_symbol !=
                    stock_symbol ||
                market_regime.benchmark_symbol !=
                    benchmark_symbol)
            {
                ++stats.symbol_violations;
            }


            if (market_regime.decision_time !=
                decision_time)
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // Independent 1m classification validation
            // ================================================================

            validateHorizon(
                market_regime.one_minute,
                trend_features.one_minute,
                volatility_features.one_minute,
                return_statistics.one_minute,
                stock_benchmark_statistics.one_minute,
                stats.one_minute);


            // ================================================================
            // Independent 5m classification validation
            // ================================================================

            validateHorizon(
                market_regime.five_minute,
                trend_features.five_minute,
                volatility_features.five_minute,
                return_statistics.five_minute,
                stock_benchmark_statistics.five_minute,
                stats.five_minute);


            // ================================================================
            // Independent 15m classification validation
            // ================================================================

            validateHorizon(
                market_regime.fifteen_minute,
                trend_features.fifteen_minute,
                volatility_features.fifteen_minute,
                return_statistics.fifteen_minute,
                stock_benchmark_statistics.fifteen_minute,
                stats.fifteen_minute);


            // ================================================================
            // Top-level readiness integrity
            // ================================================================

            const bool expected_one_ready =
                market_regime.one_minute.
                    regime_ready;


            const bool expected_five_ready =
                market_regime.five_minute.
                    regime_ready;


            const bool expected_fifteen_ready =
                market_regime.fifteen_minute.
                    regime_ready;


            const bool expected_fully_ready =
                expected_one_ready &&
                expected_five_ready &&
                expected_fifteen_ready;


            if (market_regime.one_minute_ready !=
                    expected_one_ready ||

                market_regime.five_minute_ready !=
                    expected_five_ready ||

                market_regime.fifteen_minute_ready !=
                    expected_fifteen_ready ||

                market_regime.fully_ready !=
                    expected_fully_ready)
            {
                ++stats.
                    top_level_readiness_mismatches;
            }


            if (market_regime.fully_ready)
            {
                ++stats.fully_ready;
            }
        }


        // ====================================================================
        // Reports
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "1-MINUTE MARKET REGIME VALIDATION\n"
            << "============================================================\n";


        printHorizon(
            "1-MINUTE REGIME",
            stats.one_minute);


        std::cout
            << "\n"
            << "============================================================\n"
            << "5-MINUTE MARKET REGIME VALIDATION\n"
            << "============================================================\n";


        printHorizon(
            "5-MINUTE REGIME",
            stats.five_minute);


        std::cout
            << "\n"
            << "============================================================\n"
            << "15-MINUTE MARKET REGIME VALIDATION\n"
            << "============================================================\n";


        printHorizon(
            "15-MINUTE REGIME",
            stats.fifteen_minute);


        // ====================================================================
        // Runtime report
        // ====================================================================

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

            << "Fully-ready Phase 4.5 points        : "
            << stats.fully_ready
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

            << "Top-level readiness mismatches      : "
            << stats.top_level_readiness_mismatches
            << "\n"

            << "Price-return feature exceptions     : "
            << stats.price_feature_exceptions
            << "\n"

            << "Trend feature exceptions            : "
            << stats.trend_feature_exceptions
            << "\n"

            << "Volatility feature exceptions       : "
            << stats.volatility_feature_exceptions
            << "\n"

            << "Return-statistical exceptions       : "
            << stats.return_statistical_exceptions
            << "\n"

            << "Stock/NIFTY statistical exceptions  : "
            << stats.stock_benchmark_statistical_exceptions
            << "\n"

            << "Market-regime exceptions            : "
            << stats.market_regime_exceptions
            << "\n";


        // ====================================================================
        // Pass / fail
        // ====================================================================

        const bool one_passed =
            horizonPassed(
                stats.one_minute);


        const bool five_passed =
            horizonPassed(
                stats.five_minute);


        const bool fifteen_passed =
            horizonPassed(
                stats.fifteen_minute);


        const bool runtime_passed =
            stats.synchronized_points > 0 &&

            stats.fully_ready > 0 &&

            stats.lookahead_violations == 0 &&

            stats.decision_time_violations == 0 &&

            stats.symbol_violations == 0 &&

            stats.top_level_readiness_mismatches == 0 &&

            stats.price_feature_exceptions == 0 &&

            stats.trend_feature_exceptions == 0 &&

            stats.volatility_feature_exceptions == 0 &&

            stats.return_statistical_exceptions == 0 &&

            stats.stock_benchmark_statistical_exceptions == 0 &&

            stats.market_regime_exceptions == 0;


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
                << "PHASE 4.5 REAL MARKET REGIME VALIDATION FAILED\n"
                << "============================================================\n"

                << "1m Market Regime              : "
                << (one_passed ?
                    "PASSED" :
                    "FAILED")
                << "\n"

                << "5m Market Regime              : "
                << (five_passed ?
                    "PASSED" :
                    "FAILED")
                << "\n"

                << "15m Market Regime             : "
                << (fifteen_passed ?
                    "PASSED" :
                    "FAILED")
                << "\n"

                << "Runtime / Safety              : "
                << (runtime_passed ?
                    "PASSED" :
                    "FAILED")
                << "\n"

                << "============================================================\n";


            return 1;
        }


        std::cout
            << "PHASE 4.5 REAL MARKET REGIME VALIDATION PASSED\n"
            << "============================================================\n"

            << "1m Trend Classification          : PASSED\n"
            << "1m Volatility Classification     : PASSED\n"
            << "1m Statistical Stability         : PASSED\n"
            << "1m Final Regime                  : PASSED\n"

            << "5m Trend Classification          : PASSED\n"
            << "5m Volatility Classification     : PASSED\n"
            << "5m Statistical Stability         : PASSED\n"
            << "5m Final Regime                  : PASSED\n"

            << "15m Trend Classification         : PASSED\n"
            << "15m Volatility Classification    : PASSED\n"
            << "15m Statistical Stability        : PASSED\n"
            << "15m Final Regime                 : PASSED\n"

            << "Evidence Propagation             : PASSED\n"
            << "Readiness Integrity              : PASSED\n"
            << "Look-Ahead Protection            : PASSED\n"
            << "Decision-Time Integrity          : PASSED\n"
            << "Symbol Integrity                 : PASSED\n"
            << "Numeric Integrity                : PASSED\n"

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