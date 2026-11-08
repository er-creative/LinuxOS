#include "devai/strategy/dev_ai_strategy/HistoricalCETESinglePassReplay.hpp"
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
#include <map>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <sstream>


namespace devai::strategy {
namespace {
using namespace devai::features;
using namespace devai::market;
using namespace devai::ranking;
using namespace devai::statistics;
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


// P1: Benchmark candle preparation is shared across all stock passes.
// Each stock pass still creates its own cursor and statistical engines.
struct PreparedBenchmarkP1 {
    std::vector<TimedCandle> one_minute;
    std::vector<TimedCandle> five_minute;
    std::vector<TimedCandle> fifteen_minute;
    std::vector<std::int64_t> source_times;
    std::map<std::int64_t, std::pair<MarketSnapshot, PriceReturnFeatures>> computed;
};

PreparedBenchmarkP1 prepareBenchmarkP1(
    const HistoricalCETEReplayConfig& config,
    std::int64_t end_decision_time)
{
    MarketDataLoader loader(config.data_folder);
    const auto loaded = loader.loadPath(
        config.data_folder /
        (config.benchmark_symbol + "_1min.txt"));

    CandleAggregator aggregator;
    const auto five = aggregator.aggregate(
        loaded.candles, Timeframe::FIVE_MINUTES);
    const auto fifteen = aggregator.aggregate(
        loaded.candles, Timeframe::FIFTEEN_MINUTES);

    MultiTimeframeSynchronizerConfig sync_config;
    sync_config.mode = AvailabilityMode::ZERO_LATENCY;
    MultiTimeframeSynchronizer sync(sync_config);

    PreparedBenchmarkP1 prepared;
    prepared.one_minute = sync.prepareOneMinute(loaded.candles);
    prepared.five_minute = sync.prepareFiveMinute(five.candles);
    prepared.fifteen_minute = sync.prepareFifteenMinute(fifteen.candles);
    prepared.source_times.reserve(loaded.candles.size());
    for (const auto& candle : loaded.candles)
        prepared.source_times.push_back(candle.timestamp);

    // Compute benchmark return features only once for all stock passes.
    MultiTimeframeCursor cursor(prepared.one_minute, prepared.five_minute,
                                prepared.fifteen_minute);
    SessionState session;
    MarketSnapshotBuilder builder(config.benchmark_symbol);
    PriceReturnFeatureEngine returns(config.benchmark_symbol, 64);
    for (const auto& candle : loaded.candles) {
        const auto t = candle.timestamp + ONE_MINUTE_SECONDS;
        if (t > end_decision_time) break;
        cursor.advanceTo(t);
        const auto snapshot = builder.build(t, cursor, session);
        if (snapshot.session_phase != SessionPhase::ACTIVE ||
            hasLookahead(snapshot) || !snapshot.one_minute) continue;
        try {
            const auto features = returns.update(snapshot);
            prepared.computed.emplace(t, std::make_pair(snapshot, features));
        } catch (const std::exception&) {
            // Missing benchmark feature disables the shared path at that time.
        }
    }
    return prepared;
}

std::map<std::int64_t, MathematicalStateFeatures>
buildMathematicalStates(
    const HistoricalCETEReplayConfig& config,
    const std::string& stock_symbol,
    const PreparedBenchmarkP1& benchmark,
    const std::unordered_set<std::int64_t>& target_decision_times,
    std::int64_t end_decision_time,
    HistoricalCETEReplayStats& stats)
{
    MarketDataLoader loader(
        config.data_folder);

    const auto stock_load =
        loader.loadPath(
            config.data_folder /
            (stock_symbol + "_1min.txt"));

    if (stock_load.candles.empty() ||
        benchmark.one_minute.empty())
    {
        return {};
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

    MultiTimeframeCursor stock_cursor(
        stock_1m_timed,
        stock_5m_timed,
        stock_15m_timed);

    MultiTimeframeCursor benchmark_cursor(
        benchmark.one_minute,
        benchmark.five_minute,
        benchmark.fifteen_minute);

    // Preserve legacy per-stock benchmark semantics if candle chronology differs.
    bool shared_benchmark = stock_load.candles.size() == benchmark.source_times.size();
    if (shared_benchmark) {
        for (std::size_t i = 0; i < stock_load.candles.size(); ++i) {
            if (stock_load.candles[i].timestamp != benchmark.source_times[i]) {
                shared_benchmark = false;
                break;
            }
        }
    }

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

    std::map<std::int64_t, MathematicalStateFeatures>
        target_states;

    for (const Candle& source :
         stock_load.candles)
    {
        const std::int64_t decision_time =
            source.timestamp +
            ONE_MINUTE_SECONDS;

        if (decision_time >
            end_decision_time)
        {
            break;
        }

        stock_cursor.advanceTo(
            decision_time);

        if (!shared_benchmark)
            benchmark_cursor.advanceTo(decision_time);

        const MarketSnapshot stock_snapshot =
            stock_builder.build(
                decision_time,
                stock_cursor,
                stock_session);

        const auto cached_benchmark = benchmark.computed.find(decision_time);
        if (shared_benchmark && cached_benchmark == benchmark.computed.end())
            continue;
        const MarketSnapshot benchmark_snapshot = shared_benchmark
            ? cached_benchmark->second.first
            : benchmark_builder.build(decision_time, benchmark_cursor, benchmark_session);

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

            const PriceReturnFeatures benchmark_returns = shared_benchmark
                ? cached_benchmark->second.second
                : benchmark_return_engine.update(benchmark_snapshot);

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

            if (target_decision_times.contains(decision_time))
            {
                target_states.emplace(decision_time, std::move(state));
            }
        }
        catch (const std::exception&)
        {
            ++stats.phase4_exceptions;
        }
    }

    return target_states;
}


} // namespace
HistoricalCETESinglePassReplay::HistoricalCETESinglePassReplay(HistoricalCETEReplayConfig config)
    : config_(std::move(config)) {
    // Reuse existing configuration validation.
    (void)HistoricalCETEReplay(config_);
}
std::vector<HistoricalCETEReplayPoint> HistoricalCETESinglePassReplay::run(
    std::int64_t start_decision_time, std::int64_t end_decision_time,
    std::int64_t step_seconds, const Progress& progress) const {
    if (start_decision_time <= 0 || end_decision_time < start_decision_time ||
        step_seconds <= 0 || step_seconds % 60 != 0)
        throw std::invalid_argument("Invalid single-pass decision interval/step");
    if ((end_decision_time - start_decision_time) / step_seconds > 1000000)
        throw std::invalid_argument("Too many decision timestamps");
    std::vector<std::int64_t> times;
    std::unordered_set<std::int64_t> targets;
    for (std::int64_t t = start_decision_time; t <= end_decision_time;) {
        times.push_back(t); targets.insert(t);
        if (end_decision_time - t < step_seconds) break;
        t += step_seconds;
    }
    HistoricalCETEReplay baseline(config_);
    const auto symbols = baseline.discoverSymbols();
    if (symbols.size() < config_.minimum_universe_size)
        throw std::runtime_error("Insufficient stock symbols");
    using State = devai::statistics::MathematicalStateFeatures;
    std::map<std::int64_t, std::vector<State>> states_by_time;
    std::map<std::int64_t, HistoricalCETEReplayStats> stats_by_time;
    for (auto t : times) stats_by_time[t].discovered_stock_count = symbols.size();
    // Prepare benchmark once, outside the per-stock loop.
    const auto benchmark = prepareBenchmarkP1(config_, end_decision_time);
    const auto replay_clock = std::chrono::steady_clock::now();
    std::cerr << "[H47][START] symbols=" << symbols.size()
              << " timestamps=" << times.size() << '\n';
    for (std::size_t i=0; i<symbols.size(); ++i) {
        const auto symbol_clock = std::chrono::steady_clock::now();
        std::cerr << "[H47][SYMBOL_BEGIN] " << (i+1) << "/" << symbols.size()
                  << " " << symbols[i] << std::endl;
        if (progress) progress(i, symbols.size(), symbols[i]);
        // Each symbol and benchmark is loaded and replayed exactly once per
        // run, rather than once per requested decision timestamp.
        HistoricalCETEReplayStats symbol_stats;
        std::map<std::int64_t, State> states;
        try {
            states = buildMathematicalStates(config_, symbols[i], benchmark, targets,
                                             end_decision_time, symbol_stats);
        } catch (const std::exception& e) {
            throw std::runtime_error("Replay failed for " + symbols[i] + ": " + e.what());
        }
        std::cerr << "[H47][SYMBOL_END] " << symbols[i]
                  << " states=" << states.size()
                  << " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - symbol_clock).count()
                  << std::endl;
        // An integrity violation in a stock traversal invalidates the run.
        if (symbol_stats.lookahead_violations || symbol_stats.symbol_violations ||
            symbol_stats.decision_time_violations || symbol_stats.phase4_exceptions)
            throw std::runtime_error("Phase-4 integrity violation for " + symbols[i]);
        for (auto t : times) {
            auto& st = stats_by_time[t];
            auto it = states.find(t);
            if (it == states.end()) {
                ++st.stocks_skipped;
                // A missing state is an ineligible decision, not a fabricated
                // zero feature. Report per-time summaries after stock traversal.
                continue;
            }
            if (it->second.stock_symbol != symbols[i] ||
                it->second.benchmark_symbol != config_.benchmark_symbol ||
                it->second.decision_time != t)
                throw std::runtime_error("State symbol/time mismatch");
            states_by_time[t].push_back(std::move(it->second));
            ++st.stocks_processed;
        }
    }
    if (progress) progress(symbols.size(), symbols.size(), "Phase-5 ranking");
    std::vector<HistoricalCETEReplayPoint> output;
    output.reserve(times.size());
    std::size_t skipped_incomplete_times = 0;
    std::int64_t first_incomplete_time = 0;
    std::size_t first_incomplete_count = 0;
    for (auto decision_time : times) {
        HistoricalCETEReplayPoint point;
        point.stats = stats_by_time.at(decision_time);
        auto& phase4_states = states_by_time[decision_time];
        // Never rank or trade a partially populated universe. A missing
        // exact-time state (e.g. at the opening boundary) makes only this
        // decision timestamp ineligible, not the entire replay session.
        if (phase4_states.size() < config_.minimum_universe_size) {
            ++skipped_incomplete_times;
            if (first_incomplete_time == 0) {
                first_incomplete_time = decision_time;
                first_incomplete_count = phase4_states.size();
            }
            continue;
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
        ++point.stats.phase52_exceptions;

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
        ++point.stats.phase53_exceptions;

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

        point.snapshot =
            builder.build(
                phase53);
    }
    catch (const std::exception& exception)
    {
        ++point.stats.phase54_exceptions;

        throw std::runtime_error(
            std::string(
                "CETE Phase 5.4 failed: ") +
            exception.what());
    }

    if (point.snapshot.decision_time !=
        decision_time)
    {
        ++point.stats.decision_time_violations;

        throw std::runtime_error(
            "Final CETE snapshot decision-time mismatch.");
    }

    if (point.snapshot.benchmark_symbol !=
        config_.benchmark_symbol)
    {
        ++point.stats.symbol_violations;

        throw std::runtime_error(
            "Final CETE snapshot benchmark mismatch.");
    }

    if (!point.snapshot.numerical_integrity)
    {
        throw std::runtime_error(
            "Final CETE snapshot failed numerical integrity.");
    }

    if (point.snapshot.stock_count <
        config_.minimum_universe_size)
    {
        throw std::runtime_error(
            "Final CETE snapshot contains too few stocks.");
    }

    for (const auto& stock :
         point.snapshot.stocks)
    {
        if (stock.symbol ==
            config_.benchmark_symbol)
        {
            ++point.stats.benchmark_contamination;

            throw std::runtime_error(
                "Benchmark contaminated tradable universe.");
        }

        if (stock.decision_time !=
            decision_time)
        {
            ++point.stats.decision_time_violations;

            throw std::runtime_error(
                "Stock state decision-time mismatch.");
        }
    }

    if (!point.stats.passed())
    {
        throw std::runtime_error(
            "Historical CETE replay integrity validation failed.");
    }


        output.push_back(std::move(point));
    }
    std::cerr << "[H47][ELAPSED_MS] "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - replay_clock).count()
              << '\n';
    std::cerr << "[REPLAY][SUMMARY] requested=" << times.size()
              << " eligible=" << output.size()
              << " skipped_incomplete=" << skipped_incomplete_times;
    if (skipped_incomplete_times != 0) {
        std::cerr << " first_incomplete_time=" << first_incomplete_time
                  << " first_incomplete_stocks=" << first_incomplete_count;
    }
    std::cerr << '\n';
    if (output.empty()) {
        throw std::runtime_error(
            "No complete, integrity-valid historical decision timestamps in requested interval");
    }
    return output;
}
} // namespace devai::strategy
