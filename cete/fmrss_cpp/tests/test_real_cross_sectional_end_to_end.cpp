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

#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"
#include "devai/ranking/CrossSectionalMathematicalStateComposer.hpp"
#include "devai/ranking/CrossSectionalUniverseSnapshotBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>


namespace
{

using namespace devai::features;
using namespace devai::market;
using namespace devai::statistics;
using namespace devai::ranking;


// ============================================================================
// Configuration
// ============================================================================

constexpr const char* DEFAULT_DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

constexpr const char* DEFAULT_BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::size_t RETURN_WINDOW = 60;
constexpr std::size_t VOLUME_WINDOW = 60;
constexpr std::size_t REGRESSION_WINDOW = 60;
constexpr std::size_t RESIDUAL_WINDOW = 60;

constexpr double VOLUME_LOW_Z_THRESHOLD = -1.0;
constexpr double VOLUME_HIGH_Z_THRESHOLD = 1.0;
constexpr double VOLUME_EXTREME_Z_THRESHOLD = 2.0;

constexpr double REGIME_ELEVATED_Z_THRESHOLD = 1.0;
constexpr double REGIME_EXTREME_Z_THRESHOLD = 2.0;

constexpr std::int64_t ONE_MINUTE_SECONDS = 60;
constexpr std::int64_t FIVE_MINUTE_SECONDS = 300;
constexpr std::int64_t FIFTEEN_MINUTE_SECONDS = 900;


// ============================================================================
// Validation counters
// ============================================================================

struct ValidationStats
{
    std::size_t files_discovered{0};
    std::size_t stocks_processed{0};
    std::size_t stocks_skipped{0};

    std::size_t lookahead_violations{0};
    std::size_t decision_time_violations{0};
    std::size_t symbol_violations{0};
    std::size_t benchmark_contamination{0};
    std::size_t duplicate_symbols{0};

    std::size_t nonfinite_available_ranks{0};
    std::size_t rank_range_violations{0};
    std::size_t percentile_violations{0};
    std::size_t centered_percentile_violations{0};
    std::size_t centered_relation_violations{0};
    std::size_t universe_size_violations{0};

    std::size_t phase4_exceptions{0};
    std::size_t phase52_exceptions{0};
    std::size_t phase53_exceptions{0};
    std::size_t phase54_exceptions{0};

    [[nodiscard]]
    bool passed() const noexcept
    {
        return
            stocks_processed >= 2 &&
            lookahead_violations == 0 &&
            decision_time_violations == 0 &&
            symbol_violations == 0 &&
            benchmark_contamination == 0 &&
            duplicate_symbols == 0 &&
            nonfinite_available_ranks == 0 &&
            rank_range_violations == 0 &&
            percentile_violations == 0 &&
            centered_percentile_violations == 0 &&
            centered_relation_violations == 0 &&
            universe_size_violations == 0 &&
            phase4_exceptions == 0 &&
            phase52_exceptions == 0 &&
            phase53_exceptions == 0 &&
            phase54_exceptions == 0;
    }
};


// ============================================================================
// Timestamp
// ============================================================================

std::string formatTimestamp(std::int64_t timestamp)
{
    const std::time_t raw_time =
        static_cast<std::time_t>(timestamp);

    std::tm local_time{};

#if defined(_WIN32)
    localtime_s(&local_time, &raw_time);
#else
    localtime_r(&raw_time, &local_time);
#endif

    std::ostringstream stream;

    stream << std::put_time(
        &local_time,
        "%Y-%m-%d %H:%M:%S");

    return stream.str();
}


// ============================================================================
// Phase 3.6 relative-return context
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
// Look-ahead validation
// ============================================================================

bool hasLookahead(
    const MarketSnapshot& snapshot)
{
    if (snapshot.one_minute &&
        snapshot.one_minute->timestamp +
            ONE_MINUTE_SECONDS >
        snapshot.decision_time)
    {
        return true;
    }

    if (snapshot.five_minute &&
        snapshot.five_minute->timestamp +
            FIVE_MINUTE_SECONDS >
        snapshot.decision_time)
    {
        return true;
    }

    if (snapshot.fifteen_minute &&
        snapshot.fifteen_minute->timestamp +
            FIFTEEN_MINUTE_SECONDS >
        snapshot.decision_time)
    {
        return true;
    }

    return false;
}


// ============================================================================
// Discover stock symbols
//
// Every:
//
//     SYMBOL_1min.txt
//
// becomes a candidate except the benchmark.
// ============================================================================

std::vector<std::string> discoverSymbols(
    const std::filesystem::path& data_folder,
    const std::string& benchmark_symbol)
{
    std::vector<std::string> symbols;

    constexpr const char* suffix =
        "_1min.txt";

    const std::string suffix_string =
        suffix;


    for (const auto& entry :
         std::filesystem::directory_iterator(data_folder))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string filename =
            entry.path().filename().string();

        if (filename.size() <=
            suffix_string.size())
        {
            continue;
        }

        if (filename.compare(
                filename.size() -
                    suffix_string.size(),
                suffix_string.size(),
                suffix_string) != 0)
        {
            continue;
        }

        const std::string symbol =
            filename.substr(
                0,
                filename.size() -
                    suffix_string.size());

        if (symbol.empty() ||
            symbol == benchmark_symbol)
        {
            continue;
        }

        symbols.push_back(symbol);
    }


    std::sort(
        symbols.begin(),
        symbols.end());

    symbols.erase(
        std::unique(
            symbols.begin(),
            symbols.end()),
        symbols.end());

    return symbols;
}


// ============================================================================
// Determine common end decision time
//
// Uses the earliest final 1m completion time across the stock universe and
// benchmark. Individual stocks still have to possess an actual candle at this
// decision point before they can enter the Phase-5 snapshot.
// ============================================================================

std::int64_t determineTargetDecisionTime(
    const std::filesystem::path& data_folder,
    const std::vector<std::string>& symbols,
    const std::string& benchmark_symbol)
{
    MarketDataLoader loader(data_folder);

    std::int64_t target =
        std::numeric_limits<std::int64_t>::max();


    auto inspect =
        [&](const std::string& symbol)
        {
            const auto file =
                data_folder /
                (symbol + "_1min.txt");

            const auto loaded =
                loader.loadPath(file);

            if (loaded.candles.empty())
            {
                throw std::runtime_error(
                    "No candles for " + symbol);
            }

            const std::int64_t final_decision =
                loaded.candles.back().timestamp +
                ONE_MINUTE_SECONDS;

            target =
                std::min(
                    target,
                    final_decision);
        };


    inspect(benchmark_symbol);

    for (const auto& symbol :
         symbols)
    {
        inspect(symbol);
    }


    if (target ==
        std::numeric_limits<std::int64_t>::max())
    {
        throw std::runtime_error(
            "Unable to determine target decision time.");
    }

    return target;
}


// ============================================================================
// Build latest real Phase-4 state for one stock
//
// The entire history up to target_decision_time is replayed so every rolling
// Phase-3 / Phase-4 engine has genuine historical state.
//
// No future candle is passed to an engine.
// ============================================================================

std::optional<MathematicalStateFeatures>
buildRealMathematicalState(
    const std::filesystem::path& data_folder,
    const std::string& stock_symbol,
    const std::string& benchmark_symbol,
    std::int64_t target_decision_time,
    ValidationStats& stats)
{
    MarketDataLoader loader(data_folder);

    const auto stock_load =
        loader.loadPath(
            data_folder /
            (stock_symbol + "_1min.txt"));

    const auto benchmark_load =
        loader.loadPath(
            data_folder /
            (benchmark_symbol + "_1min.txt"));


    if (stock_load.candles.empty() ||
        benchmark_load.candles.empty())
    {
        return std::nullopt;
    }


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


    MultiTimeframeSynchronizerConfig config;

    config.mode =
        AvailabilityMode::ZERO_LATENCY;

    MultiTimeframeSynchronizer synchronizer(
        config);


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
    // Phase 3
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
    // Phase 4
    // ========================================================================

    ReturnStatisticalEngine return_engine(
        stock_symbol,
        RETURN_WINDOW,
        REGIME_EXTREME_Z_THRESHOLD);

    VolumeStatisticalEngine volume_stat_engine(
        stock_symbol,
        VOLUME_WINDOW,
        VOLUME_LOW_Z_THRESHOLD,
        VOLUME_HIGH_Z_THRESHOLD,
        VOLUME_EXTREME_Z_THRESHOLD);

    StockBenchmarkStatisticalEngine
        benchmark_stat_engine(
            stock_symbol,
            benchmark_symbol,
            REGRESSION_WINDOW,
            RESIDUAL_WINDOW,
            REGIME_EXTREME_Z_THRESHOLD);

    MarketRegimeEngine regime_engine(
        stock_symbol,
        benchmark_symbol,
        REGIME_ELEVATED_Z_THRESHOLD,
        REGIME_EXTREME_Z_THRESHOLD);

    MathematicalStateComposer composer(
        stock_symbol,
        benchmark_symbol);


    std::optional<MathematicalStateFeatures>
        target_state;


    for (const Candle& source :
         stock_load.candles)
    {
        const std::int64_t decision_time =
            source.timestamp +
            ONE_MINUTE_SECONDS;


        if (decision_time >
            target_decision_time)
        {
            break;
        }


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


        if (stock_snapshot.session_phase !=
            SessionPhase::ACTIVE)
        {
            continue;
        }


        if (hasLookahead(stock_snapshot) ||
            hasLookahead(benchmark_snapshot))
        {
            ++stats.lookahead_violations;
            continue;
        }


        if (!stock_snapshot.one_minute ||
            !benchmark_snapshot.one_minute)
        {
            continue;
        }


        if (stock_snapshot.one_minute->timestamp !=
            benchmark_snapshot.one_minute->timestamp)
        {
            continue;
        }


        try
        {
            const PriceReturnFeatures stock_returns =
                stock_return_engine.update(
                    stock_snapshot);

            const PriceReturnFeatures benchmark_returns =
                benchmark_return_engine.update(
                    benchmark_snapshot);


            const TrendMomentumFeatures trend =
                trend_engine.update(
                    stock_snapshot);


            const VolumeFeatures volume =
                volume_engine.update(
                    stock_snapshot);


            const VolatilityFeatures volatility =
                volatility_engine.update(
                    stock_snapshot);


            const ReturnStatisticalFeatures
                return_statistics =
                    return_engine.update(
                        stock_returns,
                        stock_snapshot);


            const VolumeStatisticalFeatures
                volume_statistics =
                    volume_stat_engine.update(
                        volume,
                        stock_snapshot);


            const StockBenchmarkFeatures relative =
                makeRelativeFeatures(
                    stock_returns,
                    benchmark_returns);


            const StockBenchmarkStatisticalFeatures
                benchmark_statistics =
                    benchmark_stat_engine.update(
                        stock_returns,
                        benchmark_returns,
                        relative,
                        stock_snapshot,
                        benchmark_snapshot);


            const MarketRegimeFeatures regime =
                regime_engine.update(
                    trend,
                    volatility,
                    return_statistics,
                    benchmark_statistics);


            MathematicalStateFeatures state =
                composer.compose(
                    return_statistics,
                    volume_statistics,
                    benchmark_statistics,
                    regime);


            if (state.stock_symbol !=
                stock_symbol)
            {
                ++stats.symbol_violations;
            }


            if (state.benchmark_symbol !=
                benchmark_symbol)
            {
                ++stats.symbol_violations;
            }


            if (state.decision_time !=
                decision_time)
            {
                ++stats.decision_time_violations;
            }


            if (decision_time ==
                target_decision_time)
            {
                target_state =
                    std::move(state);
            }
        }
        catch (const std::exception&)
        {
            ++stats.phase4_exceptions;
        }
    }


    return target_state;
}


// ============================================================================
// Rank validation
// ============================================================================

void validateRank(
    const CrossSectionalRank& rank,
    std::size_t total_stocks,
    ValidationStats& stats)
{
    if (rank.universe_size >
        total_stocks)
    {
        ++stats.universe_size_violations;
    }


    if (!rank.available)
    {
        if (std::isfinite(rank.rank) ||
            std::isfinite(rank.percentile) ||
            std::isfinite(
                rank.centered_percentile))
        {
            ++stats.nonfinite_available_ranks;
        }

        return;
    }


    if (!std::isfinite(rank.value) ||
        !std::isfinite(rank.rank) ||
        !std::isfinite(rank.percentile) ||
        !std::isfinite(
            rank.centered_percentile))
    {
        ++stats.nonfinite_available_ranks;
        return;
    }


    if (rank.rank < 1.0 ||
        rank.rank >
            static_cast<double>(
                rank.universe_size))
    {
        ++stats.rank_range_violations;
    }


    constexpr double epsilon =
        1.0e-12;


    if (rank.percentile <
            -epsilon ||
        rank.percentile >
            1.0 + epsilon)
    {
        ++stats.percentile_violations;
    }


    if (rank.centered_percentile <
            -1.0 - epsilon ||
        rank.centered_percentile >
            1.0 + epsilon)
    {
        ++stats.centered_percentile_violations;
    }


    const double expected_centered =
        2.0 *
        rank.percentile -
        1.0;


    if (std::fabs(
            rank.centered_percentile -
            expected_centered) >
        epsilon)
    {
        ++stats.centered_relation_violations;
    }
}


// ============================================================================
// Validate one horizon
// ============================================================================

void validateHorizon(
    const CrossSectionalHorizonState& horizon,
    std::size_t total_stocks,
    ValidationStats& stats)
{
    const auto& r =
        horizon.rankings;


    const CrossSectionalRank*
        ranks[] =
    {
        &r.return_value,
        &r.return_z_score,
        &r.return_robust_z_score,
        &r.volume_z_score,
        &r.rvol20_z_score,
        &r.relative_return,
        &r.residual_return,
        &r.residual_z_score,
        &r.residual_robust_z_score,
        &r.atr14_percent,
        &r.realized_volatility_20,
        &r.rolling_beta,
        &r.rolling_correlation
    };


    for (const auto* rank :
         ranks)
    {
        validateRank(
            *rank,
            total_stocks,
            stats);
    }
}


// ============================================================================
// Number formatting
// ============================================================================

std::string numberText(
    double value,
    int precision = 8)
{
    if (std::isnan(value))
    {
        return "NaN";
    }

    if (!std::isfinite(value))
    {
        return value > 0.0
            ? "+Inf"
            : "-Inf";
    }

    std::ostringstream stream;

    stream
        << std::fixed
        << std::setprecision(precision)
        << value;

    return stream.str();
}


// ============================================================================
// Print one metric
// ============================================================================

void printRankRow(
    const std::string& metric,
    const CrossSectionalRank& rank)
{
    std::cout
        << std::left
        << std::setw(29)
        << metric

        << std::right
        << std::setw(16)
        << numberText(rank.value)

        << std::setw(12)
        << (rank.available
                ? "YES"
                : "NO")

        << std::setw(12)
        << numberText(rank.rank, 2)

        << std::setw(15)
        << numberText(
            rank.percentile,
            6)

        << std::setw(15)
        << numberText(
            rank.centered_percentile,
            6)

        << std::setw(12)
        << rank.universe_size

        << '\n';
}


// ============================================================================
// Print one timeframe
// ============================================================================

void printHorizon(
    const std::string& name,
    const CrossSectionalHorizonState& horizon)
{
    const auto& r =
        horizon.rankings;


    std::cout
        << "\n"
        << "  "
        << name
        << "\n"
        << "  "
        << std::string(108, '-')
        << "\n"

        << "  "
        << std::left
        << std::setw(29)
        << "Metric"

        << std::right
        << std::setw(16)
        << "Raw Value"

        << std::setw(12)
        << "Available"

        << std::setw(12)
        << "Rank"

        << std::setw(15)
        << "Percentile"

        << std::setw(15)
        << "Centered"

        << std::setw(12)
        << "Universe"

        << '\n'

        << "  "
        << std::string(108, '-')
        << '\n';


    std::cout << "  ";
    printRankRow(
        "Return Value",
        r.return_value);

    std::cout << "  ";
    printRankRow(
        "Return Z-Score",
        r.return_z_score);

    std::cout << "  ";
    printRankRow(
        "Return Robust Z",
        r.return_robust_z_score);

    std::cout << "  ";
    printRankRow(
        "Volume Z-Score",
        r.volume_z_score);

    std::cout << "  ";
    printRankRow(
        "RVOL20 Z-Score",
        r.rvol20_z_score);

    std::cout << "  ";
    printRankRow(
        "Relative Return",
        r.relative_return);

    std::cout << "  ";
    printRankRow(
        "Residual Return",
        r.residual_return);

    std::cout << "  ";
    printRankRow(
        "Residual Z-Score",
        r.residual_z_score);

    std::cout << "  ";
    printRankRow(
        "Residual Robust Z",
        r.residual_robust_z_score);

    std::cout << "  ";
    printRankRow(
        "ATR14 %",
        r.atr14_percent);

    std::cout << "  ";
    printRankRow(
        "Realized Volatility 20",
        r.realized_volatility_20);

    std::cout << "  ";
    printRankRow(
        "Rolling Beta",
        r.rolling_beta);

    std::cout << "  ";
    printRankRow(
        "Rolling Correlation",
        r.rolling_correlation);


    std::cout
        << "\n"
        << "  Available metrics : "
        << horizon.readiness.available_metric_count
        << " / "
        << horizon.readiness.total_metric_count
        << '\n'

        << "  Numerical integrity : "
        << (horizon.readiness.numerical_integrity
                ? "PASSED"
                : "FAILED")
        << '\n'

        << "  Fully ready         : "
        << (horizon.readiness.fully_ready
                ? "YES"
                : "NO")
        << '\n';
}


// ============================================================================
// Print universe-size table
// ============================================================================

void printUniverseSizeRow(
    const std::string& name,
    std::size_t one,
    std::size_t five,
    std::size_t fifteen)
{
    std::cout
        << std::left
        << std::setw(31)
        << name

        << std::right
        << std::setw(12)
        << one

        << std::setw(12)
        << five

        << std::setw(12)
        << fifteen

        << '\n';
}


void printUniverseSizes(
    const CrossSectionalUniverseSnapshot&
        snapshot)
{
    const auto& m1 =
        snapshot.one_minute_universe_sizes;

    const auto& m5 =
        snapshot.five_minute_universe_sizes;

    const auto& m15 =
        snapshot.fifteen_minute_universe_sizes;


    std::cout
        << "\n"
        << "METRIC-SPECIFIC UNIVERSE SIZES\n"
        << "====================================================================\n"
        << std::left
        << std::setw(31)
        << "Metric"
        << std::right
        << std::setw(12)
        << "1m"
        << std::setw(12)
        << "5m"
        << std::setw(12)
        << "15m"
        << '\n'
        << "--------------------------------------------------------------------\n";


    printUniverseSizeRow(
        "Return Value",
        m1.return_value,
        m5.return_value,
        m15.return_value);

    printUniverseSizeRow(
        "Return Z-Score",
        m1.return_z_score,
        m5.return_z_score,
        m15.return_z_score);

    printUniverseSizeRow(
        "Return Robust Z",
        m1.return_robust_z_score,
        m5.return_robust_z_score,
        m15.return_robust_z_score);

    printUniverseSizeRow(
        "Volume Z-Score",
        m1.volume_z_score,
        m5.volume_z_score,
        m15.volume_z_score);

    printUniverseSizeRow(
        "RVOL20 Z-Score",
        m1.rvol20_z_score,
        m5.rvol20_z_score,
        m15.rvol20_z_score);

    printUniverseSizeRow(
        "Relative Return",
        m1.relative_return,
        m5.relative_return,
        m15.relative_return);

    printUniverseSizeRow(
        "Residual Return",
        m1.residual_return,
        m5.residual_return,
        m15.residual_return);

    printUniverseSizeRow(
        "Residual Z-Score",
        m1.residual_z_score,
        m5.residual_z_score,
        m15.residual_z_score);

    printUniverseSizeRow(
        "Residual Robust Z",
        m1.residual_robust_z_score,
        m5.residual_robust_z_score,
        m15.residual_robust_z_score);

    printUniverseSizeRow(
        "ATR14 %",
        m1.atr14_percent,
        m5.atr14_percent,
        m15.atr14_percent);

    printUniverseSizeRow(
        "Realized Volatility 20",
        m1.realized_volatility_20,
        m5.realized_volatility_20,
        m15.realized_volatility_20);

    printUniverseSizeRow(
        "Rolling Beta",
        m1.rolling_beta,
        m5.rolling_beta,
        m15.rolling_beta);

    printUniverseSizeRow(
        "Rolling Correlation",
        m1.rolling_correlation,
        m5.rolling_correlation,
        m15.rolling_correlation);
}


// ============================================================================
// Main validation
// ============================================================================

int runValidation(
    const std::filesystem::path& data_folder,
    const std::string& benchmark_symbol)
{
    ValidationStats stats;


    std::cout
        << "\n"
        << "==========================================================================\n"
        << "CETE PHASE 5.5 — REAL-MARKET END-TO-END CROSS-SECTIONAL VALIDATION\n"
        << "==========================================================================\n"
        << "Data folder : "
        << data_folder
        << '\n'
        << "Benchmark   : "
        << benchmark_symbol
        << '\n';


    // ========================================================================
    // Discover universe
    // ========================================================================

    const auto symbols =
        discoverSymbols(
            data_folder,
            benchmark_symbol);


    stats.files_discovered =
        symbols.size();


    if (symbols.size() < 2)
    {
        throw std::runtime_error(
            "At least two stock files are required.");
    }


    std::cout
        << "Stock files : "
        << symbols.size()
        << '\n';


    // ========================================================================
    // Common real decision time
    // ========================================================================

    const std::int64_t target_decision_time =
        determineTargetDecisionTime(
            data_folder,
            symbols,
            benchmark_symbol);


    std::cout
        << "Target time : "
        << formatTimestamp(
            target_decision_time)
        << '\n'
        << "==========================================================================\n";


    // ========================================================================
    // Build real Phase-4 states
    // ========================================================================

    std::vector<MathematicalStateFeatures>
        phase4_states;


    phase4_states.reserve(
        symbols.size());


    std::cout
        << "\nBUILDING REAL PHASE-4 STATES\n"
        << "--------------------------------------------------------------------------\n";


    for (std::size_t index = 0;
         index < symbols.size();
         ++index)
    {
        const auto& symbol =
            symbols[index];


        std::cout
            << "["
            << std::setw(3)
            << index + 1
            << "/"
            << symbols.size()
            << "] "
            << std::left
            << std::setw(15)
            << symbol
            << std::right;


        try
        {
            auto state =
                buildRealMathematicalState(
                    data_folder,
                    symbol,
                    benchmark_symbol,
                    target_decision_time,
                    stats);


            if (!state)
            {
                ++stats.stocks_skipped;

                std::cout
                    << " SKIPPED — no aligned state at target\n";

                continue;
            }


            if (state->decision_time !=
                target_decision_time)
            {
                ++stats.decision_time_violations;
                ++stats.stocks_skipped;

                std::cout
                    << " SKIPPED — decision-time mismatch\n";

                continue;
            }


            if (state->stock_symbol ==
                benchmark_symbol)
            {
                ++stats.benchmark_contamination;
                ++stats.stocks_skipped;

                std::cout
                    << " SKIPPED — benchmark contamination\n";

                continue;
            }


            phase4_states.push_back(
                std::move(*state));

            ++stats.stocks_processed;

            std::cout
                << " READY\n";
        }
        catch (const std::exception& exception)
        {
            ++stats.stocks_skipped;

            std::cout
                << " SKIPPED — "
                << exception.what()
                << '\n';
        }
    }


    if (phase4_states.size() < 2)
    {
        throw std::runtime_error(
            "Fewer than two stocks reached "
            "the common decision point.");
    }


    // ========================================================================
    // Duplicate / identity protection
    // ========================================================================

    {
        std::unordered_set<std::string>
            seen;

        for (const auto& state :
             phase4_states)
        {
            if (!seen.insert(
                    state.stock_symbol).
                    second)
            {
                ++stats.duplicate_symbols;
            }

            if (state.stock_symbol ==
                benchmark_symbol)
            {
                ++stats.benchmark_contamination;
            }

            if (state.decision_time !=
                target_decision_time)
            {
                ++stats.decision_time_violations;
            }
        }
    }


    // ========================================================================
    // Phase 5.2
    // ========================================================================

    std::vector<
        CrossSectionalMathematicalFeatures>
        phase52;


    try
    {
        CrossSectionalMathematicalRanker
            ranker;

        phase52 =
            ranker.rank(
                phase4_states);
    }
    catch (const std::exception& exception)
    {
        ++stats.phase52_exceptions;

        throw std::runtime_error(
            std::string(
                "Phase 5.2 failed: ") +
            exception.what());
    }


    // ========================================================================
    // Phase 5.3
    // ========================================================================

    std::vector<
        CrossSectionalMathematicalState>
        phase53;


    try
    {
        CrossSectionalMathematicalStateComposer
            state_composer;

        phase53 =
            state_composer.compose(
                phase52);
    }
    catch (const std::exception& exception)
    {
        ++stats.phase53_exceptions;

        throw std::runtime_error(
            std::string(
                "Phase 5.3 failed: ") +
            exception.what());
    }


    // ========================================================================
    // Phase 5.4
    // ========================================================================

    CrossSectionalUniverseSnapshot
        snapshot;


    try
    {
        CrossSectionalUniverseSnapshotBuilder
            builder;

        snapshot =
            builder.build(
                phase53);
    }
    catch (const std::exception& exception)
    {
        ++stats.phase54_exceptions;

        throw std::runtime_error(
            std::string(
                "Phase 5.4 failed: ") +
            exception.what());
    }


    // ========================================================================
    // Validate complete Phase-5 state
    // ========================================================================

    for (const auto& stock :
         snapshot.stocks)
    {
        if (stock.symbol ==
            snapshot.benchmark_symbol)
        {
            ++stats.benchmark_contamination;
        }


        if (stock.decision_time !=
            snapshot.decision_time)
        {
            ++stats.decision_time_violations;
        }


        validateHorizon(
            stock.one_minute,
            snapshot.stock_count,
            stats);

        validateHorizon(
            stock.five_minute,
            snapshot.stock_count,
            stats);

        validateHorizon(
            stock.fifteen_minute,
            snapshot.stock_count,
            stats);
    }


    // ========================================================================
    // Universe summary
    // ========================================================================

    std::cout
        << "\n"
        << "==========================================================================\n"
        << "REAL CROSS-SECTIONAL UNIVERSE\n"
        << "==========================================================================\n"
        << "Benchmark                   : "
        << snapshot.benchmark_symbol
        << '\n'
        << "Decision Time               : "
        << formatTimestamp(
            snapshot.decision_time)
        << '\n'
        << "Discovered Stock Files      : "
        << stats.files_discovered
        << '\n'
        << "Stocks Processed            : "
        << stats.stocks_processed
        << '\n'
        << "Stocks Skipped              : "
        << stats.stocks_skipped
        << '\n'
        << "Stocks in Snapshot          : "
        << snapshot.stock_count
        << '\n'
        << "Fully Ready Stocks          : "
        << snapshot.fully_ready_stock_count
        << '\n'
        << "Partially Ready Stocks      : "
        << snapshot.partially_ready_stock_count
        << '\n'
        << "Numerical Integrity         : "
        << (snapshot.numerical_integrity
                ? "PASSED"
                : "FAILED")
        << '\n'
        << "Universe Fully Ready        : "
        << (snapshot.fully_ready
                ? "YES"
                : "NO")
        << '\n';


    printUniverseSizes(
        snapshot);


    // ========================================================================
    // ALL CALCULATION RESULTS
    // ========================================================================

    std::cout
        << "\n"
        << "==========================================================================\n"
        << "ALL REAL CROSS-SECTIONAL CALCULATION RESULTS\n"
        << "==========================================================================\n";


    for (const auto& stock :
         snapshot.stocks)
    {
        std::cout
            << "\n\n"
            << "##########################################################################\n"
            << "SYMBOL       : "
            << stock.symbol
            << '\n'
            << "BENCHMARK    : "
            << stock.benchmark_symbol
            << '\n'
            << "DECISION TIME: "
            << formatTimestamp(
                stock.decision_time)
            << '\n'
            << "##########################################################################\n";


        printHorizon(
            "1-MINUTE",
            stock.one_minute);

        printHorizon(
            "5-MINUTE",
            stock.five_minute);

        printHorizon(
            "15-MINUTE",
            stock.fifteen_minute);


        std::cout
            << "\n"
            << "  Stock numerical integrity : "
            << (stock.numerical_integrity
                    ? "PASSED"
                    : "FAILED")
            << '\n'
            << "  Stock fully ready         : "
            << (stock.fully_ready
                    ? "YES"
                    : "NO")
            << '\n';
    }


    // ========================================================================
    // Integrity report
    // ========================================================================

    std::cout
        << "\n\n"
        << "==========================================================================\n"
        << "PHASE 5.5 INTEGRITY REPORT\n"
        << "==========================================================================\n"

        << std::left
        << std::setw(42)
        << "Look-ahead violations"
        << " : "
        << stats.lookahead_violations
        << '\n'

        << std::setw(42)
        << "Decision-time violations"
        << " : "
        << stats.decision_time_violations
        << '\n'

        << std::setw(42)
        << "Symbol violations"
        << " : "
        << stats.symbol_violations
        << '\n'

        << std::setw(42)
        << "Benchmark contamination"
        << " : "
        << stats.benchmark_contamination
        << '\n'

        << std::setw(42)
        << "Duplicate symbols"
        << " : "
        << stats.duplicate_symbols
        << '\n'

        << std::setw(42)
        << "Non-finite available ranks"
        << " : "
        << stats.nonfinite_available_ranks
        << '\n'

        << std::setw(42)
        << "Rank-range violations"
        << " : "
        << stats.rank_range_violations
        << '\n'

        << std::setw(42)
        << "Percentile violations"
        << " : "
        << stats.percentile_violations
        << '\n'

        << std::setw(42)
        << "Centered-percentile violations"
        << " : "
        << stats.centered_percentile_violations
        << '\n'

        << std::setw(42)
        << "Centered relation violations"
        << " : "
        << stats.centered_relation_violations
        << '\n'

        << std::setw(42)
        << "Universe-size violations"
        << " : "
        << stats.universe_size_violations
        << '\n'

        << std::setw(42)
        << "Phase-4 exceptions"
        << " : "
        << stats.phase4_exceptions
        << '\n'

        << std::setw(42)
        << "Phase-5.2 exceptions"
        << " : "
        << stats.phase52_exceptions
        << '\n'

        << std::setw(42)
        << "Phase-5.3 exceptions"
        << " : "
        << stats.phase53_exceptions
        << '\n'

        << std::setw(42)
        << "Phase-5.4 exceptions"
        << " : "
        << stats.phase54_exceptions
        << '\n';


    // ========================================================================
    // Final result
    //
    // IMPORTANT:
    //
    // snapshot.fully_ready is intentionally NOT required here.
    //
    // A mathematically valid real universe may legitimately contain partial
    // metric availability. Phase 5.4 explicitly preserves that state.
    //
    // Numerical integrity, however, MUST pass.
    // ========================================================================

    const bool passed =
        stats.passed() &&
        snapshot.numerical_integrity &&
        snapshot.stock_count >= 2 &&
        snapshot.decision_time ==
            target_decision_time &&
        snapshot.benchmark_symbol ==
            benchmark_symbol;


    std::cout
        << "\n"
        << "==========================================================================\n";


    if (passed)
    {
        std::cout
            << "PHASE 5.5 REAL-MARKET END-TO-END VALIDATION PASSED\n"
            << "==========================================================================\n"
            << "Real Market Replay                 : PASSED\n"
            << "Look-Ahead Protection              : PASSED\n"
            << "Common Decision-Time Integrity     : PASSED\n"
            << "Benchmark Exclusion                : PASSED\n"
            << "Duplicate-Symbol Protection        : PASSED\n"
            << "Phase 4 -> 5.2 Integration         : PASSED\n"
            << "Phase 5.2 Ranking Integrity        : PASSED\n"
            << "Phase 5.3 State Integrity          : PASSED\n"
            << "Phase 5.4 Universe Integrity       : PASSED\n"
            << "Metric Universe-Size Integrity     : PASSED\n"
            << "Rank Numerical Integrity           : PASSED\n"
            << "Percentile Integrity               : PASSED\n"
            << "Centered-Percentile Integrity      : PASSED\n"
            << "==========================================================================\n";

        return EXIT_SUCCESS;
    }


    std::cout
        << "PHASE 5.5 REAL-MARKET END-TO-END VALIDATION FAILED\n"
        << "==========================================================================\n";

    return EXIT_FAILURE;
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


        const std::string benchmark_symbol =
            argc >= 3
                ? argv[2]
                : DEFAULT_BENCHMARK_SYMBOL;


        return runValidation(
            data_folder,
            benchmark_symbol);
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "PHASE 5.5 REAL-MARKET VALIDATION ERROR\n"
            << exception.what()
            << '\n';

        return EXIT_FAILURE;
    }
}