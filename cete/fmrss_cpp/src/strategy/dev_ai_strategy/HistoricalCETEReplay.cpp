#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"

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

#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"
#include "devai/ranking/CrossSectionalMathematicalStateComposer.hpp"
#include "devai/ranking/CrossSectionalUniverseSnapshotBuilder.hpp"

#include "devai/statistics/MarketRegimeEngine.hpp"
#include "devai/statistics/MathematicalStateComposer.hpp"
#include "devai/statistics/ReturnStatisticalEngine.hpp"
#include "devai/statistics/StockBenchmarkStatisticalEngine.hpp"
#include "devai/statistics/VolumeStatisticalEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace devai::strategy
{
namespace
{

using namespace devai::features;
using namespace devai::market;
using namespace devai::ranking;
using namespace devai::statistics;

// Frozen CETE configuration used by the validated Phase-5.5 pipeline.
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


bool hasLookahead(
    const MarketSnapshot& snapshot) noexcept
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


// This helper is intentionally identical in semantics to the validated
// CETE Phase-5.5 real-market validation path.
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


std::optional<MathematicalStateFeatures>
buildMathematicalState(
    const HistoricalCETEReplayConfig& config,
    const std::string& stock_symbol,
    std::int64_t target_decision_time,
    HistoricalCETEReplayStats& stats)
{
    MarketDataLoader loader(
        config.data_folder);

    const auto stock_load =
        loader.loadPath(
            config.data_folder /
            (stock_symbol + "_1min.txt"));

    const auto benchmark_load =
        loader.loadPath(
            config.data_folder /
            (config.benchmark_symbol +
             "_1min.txt"));

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
        config.benchmark_symbol);

    PriceReturnFeatureEngine
        stock_return_engine(
            stock_symbol,
            64);

    PriceReturnFeatureEngine
        benchmark_return_engine(
            config.benchmark_symbol,
            64);

    TrendMomentumFeatureEngine
        trend_engine(
            stock_symbol);

    VolumeFeatureEngine
        volume_engine(
            stock_symbol);

    VolatilityFeatureEngine
        volatility_engine(
            stock_symbol);

    ReturnStatisticalEngine
        return_engine(
            stock_symbol,
            RETURN_WINDOW,
            REGIME_EXTREME_Z_THRESHOLD);

    VolumeStatisticalEngine
        volume_stat_engine(
            stock_symbol,
            VOLUME_WINDOW,
            VOLUME_LOW_Z_THRESHOLD,
            VOLUME_HIGH_Z_THRESHOLD,
            VOLUME_EXTREME_Z_THRESHOLD);

    StockBenchmarkStatisticalEngine
        benchmark_stat_engine(
            stock_symbol,
            config.benchmark_symbol,
            REGRESSION_WINDOW,
            RESIDUAL_WINDOW,
            REGIME_EXTREME_Z_THRESHOLD);

    MarketRegimeEngine
        regime_engine(
            stock_symbol,
            config.benchmark_symbol,
            REGIME_ELEVATED_Z_THRESHOLD,
            REGIME_EXTREME_Z_THRESHOLD);

    MathematicalStateComposer
        composer(
            stock_symbol,
            config.benchmark_symbol);

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
                continue;
            }

            if (state.benchmark_symbol !=
                config.benchmark_symbol)
            {
                ++stats.symbol_violations;
                continue;
            }

            if (state.decision_time !=
                decision_time)
            {
                ++stats.decision_time_violations;
                continue;
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

} // namespace


HistoricalCETEReplay::HistoricalCETEReplay(
    HistoricalCETEReplayConfig config)
    :
    config_(
        std::move(config))
{
    if (config_.data_folder.empty())
    {
        throw std::invalid_argument(
            "HistoricalCETEReplay data folder cannot be empty.");
    }

    if (config_.benchmark_symbol.empty())
    {
        throw std::invalid_argument(
            "HistoricalCETEReplay benchmark symbol cannot be empty.");
    }

    if (config_.minimum_universe_size < 2)
    {
        throw std::invalid_argument(
            "HistoricalCETEReplay minimum universe size must be at least 2.");
    }

    if (!std::filesystem::exists(
            config_.data_folder) ||
        !std::filesystem::is_directory(
            config_.data_folder))
    {
        throw std::invalid_argument(
            "HistoricalCETEReplay data folder is invalid: " +
            config_.data_folder.string());
    }
}


const HistoricalCETEReplayConfig&
HistoricalCETEReplay::config() const noexcept
{
    return config_;
}


std::vector<std::string>
HistoricalCETEReplay::discoverSymbols() const
{
    std::vector<std::string> symbols;

    constexpr const char* suffix =
        "_1min.txt";

    const std::string suffix_string =
        suffix;

    for (const auto& entry :
         std::filesystem::directory_iterator(
             config_.data_folder))
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
            symbol ==
                config_.benchmark_symbol)
        {
            continue;
        }

        symbols.push_back(
            symbol);
    }

    std::sort(
        symbols.begin(),
        symbols.end());

    symbols.erase(
        std::unique(
            symbols.begin(),
            symbols.end()),
        symbols.end());

    if (config_.maximum_stock_count > 0 &&
        symbols.size() >
            config_.maximum_stock_count)
    {
        symbols.resize(
            config_.maximum_stock_count);
    }

    return symbols;
}


std::int64_t
HistoricalCETEReplay::latestCommonDecisionTime() const
{
    const auto symbols =
        discoverSymbols();

    if (symbols.size() <
        config_.minimum_universe_size)
    {
        throw std::runtime_error(
            "Fewer stocks discovered than minimum universe size.");
    }

    MarketDataLoader loader(
        config_.data_folder);

    std::int64_t target =
        std::numeric_limits<std::int64_t>::max();

    auto inspect =
        [&](const std::string& symbol)
        {
            const auto loaded =
                loader.loadPath(
                    config_.data_folder /
                    (symbol + "_1min.txt"));

            if (loaded.candles.empty())
            {
                throw std::runtime_error(
                    "No candles for symbol: " +
                    symbol);
            }

            const std::int64_t final_decision =
                loaded.candles.back().timestamp +
                ONE_MINUTE_SECONDS;

            target =
                std::min(
                    target,
                    final_decision);
        };

    inspect(
        config_.benchmark_symbol);

    for (const auto& symbol :
         symbols)
    {
        inspect(
            symbol);
    }

    if (target ==
        std::numeric_limits<std::int64_t>::max())
    {
        throw std::runtime_error(
            "Unable to determine common decision time.");
    }

    return target;
}


HistoricalCETEReplayPoint
HistoricalCETEReplay::replayAt(
    std::int64_t decision_time) const
{
    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "Historical replay decision time must be positive.");
    }

    HistoricalCETEReplayPoint result;

    const auto symbols =
        discoverSymbols();

    result.stats.discovered_stock_count =
        symbols.size();

    if (symbols.size() <
        config_.minimum_universe_size)
    {
        throw std::runtime_error(
            "Fewer stocks discovered than minimum universe size.");
    }

    std::vector<MathematicalStateFeatures>
        phase4_states;

    phase4_states.reserve(
        symbols.size());

    std::unordered_set<std::string>
        seen_symbols;

    for (const auto& symbol :
         symbols)
    {
        try
        {
            auto state =
                buildMathematicalState(
                    config_,
                    symbol,
                    decision_time,
                    result.stats);

            if (!state)
            {
                ++result.stats.stocks_skipped;
                continue;
            }

            if (state->decision_time !=
                decision_time)
            {
                ++result.stats.decision_time_violations;
                ++result.stats.stocks_skipped;
                continue;
            }

            if (state->stock_symbol ==
                config_.benchmark_symbol)
            {
                ++result.stats.benchmark_contamination;
                ++result.stats.stocks_skipped;
                continue;
            }

            if (!seen_symbols.insert(
                    state->stock_symbol).
                    second)
            {
                ++result.stats.
                    duplicate_symbol_violations;

                ++result.stats.stocks_skipped;
                continue;
            }

            phase4_states.push_back(
                std::move(*state));

            ++result.stats.stocks_processed;
        }
        catch (const std::exception&)
        {
            ++result.stats.phase4_exceptions;
            ++result.stats.stocks_skipped;
        }
    }

    if (phase4_states.size() <
        config_.minimum_universe_size)
    {
        throw std::runtime_error(
            "Fewer than the minimum stocks reached the requested decision time.");
    }

    std::vector<
        CrossSectionalMathematicalFeatures>
        phase52;

    try
    {
        CrossSectionalMathematicalRanker ranker;

        phase52 =
            ranker.rank(
                phase4_states);
    }
    catch (const std::exception& exception)
    {
        ++result.stats.phase52_exceptions;

        throw std::runtime_error(
            std::string(
                "CETE Phase 5.2 failed: ") +
            exception.what());
    }

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
        ++result.stats.phase53_exceptions;

        throw std::runtime_error(
            std::string(
                "CETE Phase 5.3 failed: ") +
            exception.what());
    }

    try
    {
        CrossSectionalUniverseSnapshotBuilder
            builder(
                config_.minimum_universe_size);

        result.snapshot =
            builder.build(
                phase53);
    }
    catch (const std::exception& exception)
    {
        ++result.stats.phase54_exceptions;

        throw std::runtime_error(
            std::string(
                "CETE Phase 5.4 failed: ") +
            exception.what());
    }

    if (result.snapshot.decision_time !=
        decision_time)
    {
        ++result.stats.decision_time_violations;

        throw std::runtime_error(
            "Final CETE snapshot decision-time mismatch.");
    }

    if (result.snapshot.benchmark_symbol !=
        config_.benchmark_symbol)
    {
        ++result.stats.symbol_violations;

        throw std::runtime_error(
            "Final CETE snapshot benchmark mismatch.");
    }

    if (!result.snapshot.numerical_integrity)
    {
        throw std::runtime_error(
            "Final CETE snapshot failed numerical integrity.");
    }

    if (result.snapshot.stock_count <
        config_.minimum_universe_size)
    {
        throw std::runtime_error(
            "Final CETE snapshot contains too few stocks.");
    }

    for (const auto& stock :
         result.snapshot.stocks)
    {
        if (stock.symbol ==
            config_.benchmark_symbol)
        {
            ++result.stats.benchmark_contamination;

            throw std::runtime_error(
                "Benchmark contaminated tradable universe.");
        }

        if (stock.decision_time !=
            decision_time)
        {
            ++result.stats.decision_time_violations;

            throw std::runtime_error(
                "Stock state decision-time mismatch.");
        }
    }

    if (!result.stats.passed())
    {
        throw std::runtime_error(
            "Historical CETE replay integrity validation failed.");
    }

    return result;
}

} // namespace devai::strategy
