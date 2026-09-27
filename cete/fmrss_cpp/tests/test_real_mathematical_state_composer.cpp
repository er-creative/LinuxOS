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
#include <filesystem>
#include <iomanip>
#include <iostream>
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

constexpr std::size_t RETURN_WINDOW =
    60;

constexpr std::size_t VOLUME_WINDOW =
    60;

constexpr std::size_t REGRESSION_WINDOW =
    60;

constexpr std::size_t RESIDUAL_WINDOW =
    60;


// ============================================================================
// Phase 4.3 — Volume statistical thresholds
//
// IMPORTANT:
//
// VolumeStatisticalEngine constructor:
//
//     VolumeStatisticalEngine(
//         symbol,
//         window_size,
//         low_z_threshold,
//         high_z_threshold,
//         extreme_z_threshold)
//
// Therefore:
//
//     low     MUST be negative
//     high    MUST be positive
//     extreme MUST be greater than high
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
    // ------------------------------------------------------------------------
    // Runtime
    // ------------------------------------------------------------------------

    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t synchronized_points{0};

    std::size_t incomplete_pairs{0};

    std::size_t new_sessions{0};


    // ------------------------------------------------------------------------
    // Phase 4.6
    // ------------------------------------------------------------------------

    std::size_t composer_observations{0};

    std::size_t one_minute_ready{0};

    std::size_t five_minute_ready{0};

    std::size_t fifteen_minute_ready{0};

    std::size_t fully_ready{0};


    // ------------------------------------------------------------------------
    // Numerical integrity
    // ------------------------------------------------------------------------

    std::size_t numerical_integrity_passed{0};

    std::size_t numerical_integrity_failed{0};


    // ------------------------------------------------------------------------
    // Runtime safety
    // ------------------------------------------------------------------------

    std::size_t lookahead_violations{0};

    std::size_t decision_time_violations{0};

    std::size_t symbol_violations{0};

    std::size_t timestamp_alignment_violations{0};


    // ------------------------------------------------------------------------
    // Lossless composition
    // ------------------------------------------------------------------------

    std::size_t return_composition_mismatches{0};

    std::size_t volume_composition_mismatches{0};

    std::size_t stock_benchmark_composition_mismatches{0};

    std::size_t regime_composition_mismatches{0};


    // ------------------------------------------------------------------------
    // Readiness
    // ------------------------------------------------------------------------

    std::size_t one_minute_readiness_mismatches{0};

    std::size_t five_minute_readiness_mismatches{0};

    std::size_t fifteen_minute_readiness_mismatches{0};

    std::size_t global_integrity_mismatches{0};

    std::size_t global_readiness_mismatches{0};


    // ------------------------------------------------------------------------
    // Exceptions
    // ------------------------------------------------------------------------

    std::size_t price_return_exceptions{0};

    std::size_t trend_exceptions{0};

    std::size_t volatility_exceptions{0};

    std::size_t volume_feature_exceptions{0};

    std::size_t return_statistical_exceptions{0};

    std::size_t volume_statistical_exceptions{0};

    std::size_t stock_benchmark_statistical_exceptions{0};

    std::size_t market_regime_exceptions{0};

    std::size_t composer_exceptions{0};


    // ------------------------------------------------------------------------
    // Final validation
    // ------------------------------------------------------------------------

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
//
// Phase 4.4 requires StockBenchmarkFeatures.
//
// This validator only needs the relative-return portion.
//
// No statistical calculation occurs here.
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


    // ------------------------------------------------------------------------
    // 1-minute
    // ------------------------------------------------------------------------

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


    // ------------------------------------------------------------------------
    // 5-minute
    // ------------------------------------------------------------------------

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


    // ------------------------------------------------------------------------
    // 15-minute
    // ------------------------------------------------------------------------

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


    return output;
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
        sameDouble(
            lhs.return_value,
            rhs.return_value) &&

        sameDouble(
            lhs.absolute_return,
            rhs.absolute_return) &&

        sameDouble(
            lhs.rolling_mean,
            rhs.rolling_mean) &&

        sameDouble(
            lhs.rolling_standard_deviation,
            rhs.rolling_standard_deviation) &&

        sameDouble(
            lhs.z_score,
            rhs.z_score) &&

        sameDouble(
            lhs.rolling_median,
            rhs.rolling_median) &&

        sameDouble(
            lhs.rolling_mad,
            rhs.rolling_mad) &&

        sameDouble(
            lhs.robust_z_score,
            rhs.robust_z_score) &&

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
        sameDouble(
            lhs.value,
            rhs.value) &&

        sameDouble(
            lhs.rolling_mean,
            rhs.rolling_mean) &&

        sameDouble(
            lhs.rolling_standard_deviation,
            rhs.rolling_standard_deviation) &&

        sameDouble(
            lhs.z_score,
            rhs.z_score) &&

        sameDouble(
            lhs.rolling_median,
            rhs.rolling_median) &&

        sameDouble(
            lhs.rolling_mad,
            rhs.rolling_mad) &&

        sameDouble(
            lhs.robust_z_score,
            rhs.robust_z_score) &&

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
        sameDouble(
            lhs.stock_return,
            rhs.stock_return) &&

        sameDouble(
            lhs.benchmark_return,
            rhs.benchmark_return) &&

        sameDouble(
            lhs.relative_return,
            rhs.relative_return) &&

        sameDouble(
            lhs.rolling_beta,
            rhs.rolling_beta) &&

        sameDouble(
            lhs.rolling_alpha,
            rhs.rolling_alpha) &&

        sameDouble(
            lhs.rolling_correlation,
            rhs.rolling_correlation) &&

        sameDouble(
            lhs.residual_return,
            rhs.residual_return) &&

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
// Report helper
// ============================================================================

void printLine(
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
    // Header
    // ========================================================================

    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.6 — REAL MATHEMATICAL STATE VALIDATION\n"
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
    // Load real 1-minute data
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
    // Aggregate 1m -> 5m / 15m
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
        << "\n"
        << "Market data\n"
        << "------------------------------------------------------------\n";


    printLine(
        "Stock 1m",
        stock_load.candles.size());

    printLine(
        "Stock 5m",
        stock_5m.candles.size());

    printLine(
        "Stock 15m",
        stock_15m.candles.size());

    printLine(
        "Stock incomplete 5m",
        stock_5m.incomplete_buckets);

    printLine(
        "Stock incomplete 15m",
        stock_15m.incomplete_buckets);


    printLine(
        "NIFTY 1m",
        benchmark_load.candles.size());

    printLine(
        "NIFTY 5m",
        benchmark_5m.candles.size());

    printLine(
        "NIFTY 15m",
        benchmark_15m.candles.size());

    printLine(
        "NIFTY incomplete 5m",
        benchmark_5m.incomplete_buckets);

    printLine(
        "NIFTY incomplete 15m",
        benchmark_15m.incomplete_buckets);


    // ========================================================================
    // Historical availability
    // ========================================================================

    MultiTimeframeSynchronizerConfig synchronizer_config;


    synchronizer_config.mode =
        AvailabilityMode::ZERO_LATENCY;


    MultiTimeframeSynchronizer synchronizer(
        synchronizer_config);


    // ------------------------------------------------------------------------
    // Stock
    // ------------------------------------------------------------------------

    const auto stock_1m_timed =
        synchronizer.prepareOneMinute(
            stock_load.candles);


    const auto stock_5m_timed =
        synchronizer.prepareFiveMinute(
            stock_5m.candles);


    const auto stock_15m_timed =
        synchronizer.prepareFifteenMinute(
            stock_15m.candles);


    // ------------------------------------------------------------------------
    // Benchmark
    // ------------------------------------------------------------------------

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
    // Runtime cursors
    // ========================================================================

    MultiTimeframeCursor stock_cursor(
        stock_1m_timed,
        stock_5m_timed,
        stock_15m_timed);


    MultiTimeframeCursor benchmark_cursor(
        benchmark_1m_timed,
        benchmark_5m_timed,
        benchmark_15m_timed);


    // ========================================================================
    // Session state
    // ========================================================================

    SessionState stock_session;

    SessionState benchmark_session;


    // ========================================================================
    // Snapshot builders
    // ========================================================================

    MarketSnapshotBuilder stock_builder(
        stock_symbol);


    MarketSnapshotBuilder benchmark_builder(
        benchmark_symbol);


    // ========================================================================
    // Phase 3.1 — Price / Return
    // ========================================================================

    PriceReturnFeatureEngine stock_return_engine(
        stock_symbol,
        64);


    PriceReturnFeatureEngine benchmark_return_engine(
        benchmark_symbol,
        64);


    // ========================================================================
    // Phase 3.2 — Trend / Momentum
    // ========================================================================

    TrendMomentumFeatureEngine trend_engine(
        stock_symbol);


    // ========================================================================
    // Phase 3.3 — Volume
    // ========================================================================

    VolumeFeatureEngine volume_engine(
        stock_symbol);


    // ========================================================================
    // Phase 3.4 — Volatility
    // ========================================================================

    VolatilityFeatureEngine volatility_engine(
        stock_symbol);


    // ========================================================================
    // Phase 4.2 — Return Statistical Engine
    // ========================================================================

    ReturnStatisticalEngine return_statistical_engine(
        stock_symbol,
        RETURN_WINDOW,
        REGIME_EXTREME_Z_THRESHOLD);


    // ========================================================================
    // Phase 4.3 — Volume Statistical Engine
    //
    // RECTIFIED:
    //
    // Previous validator incorrectly supplied only the extreme threshold.
    //
    // Correct constructor:
    //
    //     symbol
    //     window
    //     low Z
    //     high Z
    //     extreme Z
    // ========================================================================

    VolumeStatisticalEngine volume_statistical_engine(
        stock_symbol,
        VOLUME_WINDOW,
        VOLUME_LOW_Z_THRESHOLD,
        VOLUME_HIGH_Z_THRESHOLD,
        VOLUME_EXTREME_Z_THRESHOLD);


    // ========================================================================
    // Phase 4.4 — Stock / NIFTY Statistical Engine
    // ========================================================================

    StockBenchmarkStatisticalEngine
        stock_benchmark_engine(
            stock_symbol,
            benchmark_symbol,
            REGRESSION_WINDOW,
            RESIDUAL_WINDOW,
            REGIME_EXTREME_Z_THRESHOLD);


    // ========================================================================
    // Phase 4.5 — Market Regime Engine
    // ========================================================================

    MarketRegimeEngine market_regime_engine(
        stock_symbol,
        benchmark_symbol,
        REGIME_ELEVATED_Z_THRESHOLD,
        REGIME_EXTREME_Z_THRESHOLD);


    // ========================================================================
    // Phase 4.6 — Mathematical State Composer
    // ========================================================================

    MathematicalStateComposer composer(
        stock_symbol,
        benchmark_symbol);


    // ========================================================================
    // Real historical replay
    //
    // Each real stock 1-minute candle completion is a decision point.
    // ========================================================================

    for (const Candle& source :
         stock_load.candles)
    {
        ++stats.decision_points;


        const std::int64_t decision_time =
            source.timestamp +
            ONE_MINUTE_SECONDS;


        // --------------------------------------------------------------------
        // Advance legally available runtime state
        // --------------------------------------------------------------------

        stock_cursor.advanceTo(
            decision_time);


        benchmark_cursor.advanceTo(
            decision_time);


        // --------------------------------------------------------------------
        // Build snapshots
        // --------------------------------------------------------------------

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
        // Active session
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
        // Look-ahead protection
        // ====================================================================

        if (hasLookahead(
                stock_snapshot) ||
            hasLookahead(
                benchmark_snapshot))
        {
            ++stats.lookahead_violations;
        }


        // ====================================================================
        // Require stock + benchmark 1m observations
        // ====================================================================

        if (!stock_snapshot.one_minute.has_value() ||
            !benchmark_snapshot.one_minute.has_value())
        {
            ++stats.incomplete_pairs;

            continue;
        }


        // ====================================================================
        // Exact 1m alignment
        // ====================================================================

        if (stock_snapshot.one_minute->timestamp !=
            benchmark_snapshot.one_minute->timestamp)
        {
            ++stats.timestamp_alignment_violations;

            continue;
        }


        ++stats.synchronized_points;


        // ====================================================================
        // Phase 3.1 — Price / Return
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
        // Phase 3.2 — Trend / Momentum
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
        // Phase 3.3 — Volume
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
        // Phase 3.4 — Volatility
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
        // Phase 4.2 — Return statistics
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
        // Phase 4.3 — Volume statistics
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
        // Phase 4.4 — Stock / NIFTY statistics
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
        // Phase 4.5 — Market regime
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
        // Phase 4.6 — Mathematical state
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
        // Metadata integrity
        // ====================================================================

        if (mathematical_state.stock_symbol !=
                stock_symbol ||

            mathematical_state.benchmark_symbol !=
                benchmark_symbol)
        {
            ++stats.symbol_violations;
        }


        // ====================================================================
        // Decision-time integrity
        // ====================================================================

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
        // Phase 4.2 lossless composition
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
        // Phase 4.3 lossless composition
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
        // Phase 4.4 lossless composition
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
        // Phase 4.5 lossless composition
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
        // Expected 1-minute readiness
        // ====================================================================

        const bool expected_one_minute =
            expectedHorizonReady(
                return_statistics.one_minute,
                volume_statistics.one_minute,
                stock_benchmark_statistics.one_minute,
                regime_features.one_minute,
                mathematical_state.one_minute.
                    readiness.numerical_integrity);


        // ====================================================================
        // Expected 5-minute readiness
        // ====================================================================

        const bool expected_five_minute =
            expectedHorizonReady(
                return_statistics.five_minute,
                volume_statistics.five_minute,
                stock_benchmark_statistics.five_minute,
                regime_features.five_minute,
                mathematical_state.five_minute.
                    readiness.numerical_integrity);


        // ====================================================================
        // Expected 15-minute readiness
        // ====================================================================

        const bool expected_fifteen_minute =
            expectedHorizonReady(
                return_statistics.fifteen_minute,
                volume_statistics.fifteen_minute,
                stock_benchmark_statistics.fifteen_minute,
                regime_features.fifteen_minute,
                mathematical_state.fifteen_minute.
                    readiness.numerical_integrity);


        // ====================================================================
        // Validate 1m readiness
        // ====================================================================

        if (mathematical_state.one_minute_ready !=
                expected_one_minute ||

            mathematical_state.one_minute.
                readiness.fully_ready !=
                expected_one_minute)
        {
            ++stats.one_minute_readiness_mismatches;
        }


        // ====================================================================
        // Validate 5m readiness
        // ====================================================================

        if (mathematical_state.five_minute_ready !=
                expected_five_minute ||

            mathematical_state.five_minute.
                readiness.fully_ready !=
                expected_five_minute)
        {
            ++stats.five_minute_readiness_mismatches;
        }


        // ====================================================================
        // Validate 15m readiness
        // ====================================================================

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


        if (mathematical_state.fully_ready)
        {
            ++stats.fully_ready;
        }
    }


    // ========================================================================
    // Phase 4.6 report
    // ========================================================================

    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 4.6 COMPOSITION VALIDATION\n"
        << "============================================================\n";


    printLine(
        "Composer observations",
        stats.composer_observations);


    printLine(
        "1m ready",
        stats.one_minute_ready);


    printLine(
        "5m ready",
        stats.five_minute_ready);


    printLine(
        "15m ready",
        stats.fifteen_minute_ready);


    printLine(
        "Fully-ready Phase 4 observations",
        stats.fully_ready);


    printLine(
        "Numerical integrity passed",
        stats.numerical_integrity_passed);


    printLine(
        "Numerical integrity failed",
        stats.numerical_integrity_failed);


    // ========================================================================
    // Composition integrity
    // ========================================================================

    std::cout
        << "\n"
        << "Composition integrity\n"
        << "------------------------------------------------------------\n";


    printLine(
        "Return composition mismatches",
        stats.return_composition_mismatches);


    printLine(
        "Volume composition mismatches",
        stats.volume_composition_mismatches);


    printLine(
        "Stock/NIFTY composition mismatches",
        stats.stock_benchmark_composition_mismatches);


    printLine(
        "Regime composition mismatches",
        stats.regime_composition_mismatches);


    // ========================================================================
    // Readiness integrity
    // ========================================================================

    std::cout
        << "\n"
        << "Readiness integrity\n"
        << "------------------------------------------------------------\n";


    printLine(
        "1m readiness mismatches",
        stats.one_minute_readiness_mismatches);


    printLine(
        "5m readiness mismatches",
        stats.five_minute_readiness_mismatches);


    printLine(
        "15m readiness mismatches",
        stats.fifteen_minute_readiness_mismatches);


    printLine(
        "Global integrity mismatches",
        stats.global_integrity_mismatches);


    printLine(
        "Global readiness mismatches",
        stats.global_readiness_mismatches);


    // ========================================================================
    // Runtime / safety
    // ========================================================================

    std::cout
        << "\n"
        << "Runtime / safety\n"
        << "------------------------------------------------------------\n";


    printLine(
        "Decision points",
        stats.decision_points);


    printLine(
        "Active-session points",
        stats.active_session_points);


    printLine(
        "Synchronized points",
        stats.synchronized_points);


    printLine(
        "Incomplete pairs",
        stats.incomplete_pairs);


    printLine(
        "New sessions",
        stats.new_sessions);


    printLine(
        "Look-ahead violations",
        stats.lookahead_violations);


    printLine(
        "Decision-time violations",
        stats.decision_time_violations);


    printLine(
        "Symbol violations",
        stats.symbol_violations);


    printLine(
        "Timestamp-alignment violations",
        stats.timestamp_alignment_violations);


    // ========================================================================
    // Exceptions
    // ========================================================================

    std::cout
        << "\n"
        << "Exceptions\n"
        << "------------------------------------------------------------\n";


    printLine(
        "Price-return exceptions",
        stats.price_return_exceptions);


    printLine(
        "Trend exceptions",
        stats.trend_exceptions);


    printLine(
        "Volatility exceptions",
        stats.volatility_exceptions);


    printLine(
        "Volume-feature exceptions",
        stats.volume_feature_exceptions);


    printLine(
        "Return-statistical exceptions",
        stats.return_statistical_exceptions);


    printLine(
        "Volume-statistical exceptions",
        stats.volume_statistical_exceptions);


    printLine(
        "Stock/NIFTY statistical exceptions",
        stats.stock_benchmark_statistical_exceptions);


    printLine(
        "Market-regime exceptions",
        stats.market_regime_exceptions);


    printLine(
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
            << "PHASE 4.6 REAL MATHEMATICAL STATE VALIDATION PASSED\n"
            << "============================================================\n"
            << "Lossless Phase 4.2 Composition     : PASSED\n"
            << "Lossless Phase 4.3 Composition     : PASSED\n"
            << "Lossless Phase 4.4 Composition     : PASSED\n"
            << "Lossless Phase 4.5 Composition     : PASSED\n"
            << "1m Readiness Integrity             : PASSED\n"
            << "5m Readiness Integrity             : PASSED\n"
            << "15m Readiness Integrity            : PASSED\n"
            << "Global Numerical Integrity         : PASSED\n"
            << "Final Phase-4 Readiness            : PASSED\n"
            << "Look-Ahead Protection              : PASSED\n"
            << "Decision-Time Integrity            : PASSED\n"
            << "Symbol Integrity                   : PASSED\n"
            << "============================================================\n";


        return
            EXIT_SUCCESS;
    }


    std::cout
        << "PHASE 4.6 REAL MATHEMATICAL STATE VALIDATION FAILED\n"
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
            << "PHASE 4.6 REAL VALIDATION ERROR\n"
            << exception.what()
            << '\n';


        return
            EXIT_FAILURE;
    }
}