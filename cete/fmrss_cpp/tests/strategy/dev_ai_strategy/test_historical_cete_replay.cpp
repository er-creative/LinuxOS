#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace
{

using devai::strategy::HistoricalCETEReplay;
using devai::strategy::HistoricalCETEReplayConfig;


void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(
            message);
    }
}


void validateSnapshot(
    const devai::ranking::CrossSectionalUniverseSnapshot&
        snapshot,
    const HistoricalCETEReplayConfig& config,
    std::int64_t expected_decision_time)
{
    require(
        snapshot.numerical_integrity,
        "Snapshot numerical integrity failed.");

    require(
        snapshot.decision_time ==
            expected_decision_time,
        "Snapshot decision time mismatch.");

    require(
        snapshot.benchmark_symbol ==
            config.benchmark_symbol,
        "Snapshot benchmark mismatch.");

    require(
        snapshot.stock_count >=
            config.minimum_universe_size,
        "Snapshot universe is too small.");

    require(
        snapshot.stocks.size() ==
            snapshot.stock_count,
        "Snapshot stock_count does not match stocks.size().");

    std::unordered_set<std::string>
        seen;

    for (const auto& stock :
         snapshot.stocks)
    {
        require(
            stock.symbol !=
                config.benchmark_symbol,
            "Benchmark entered tradable universe.");

        require(
            stock.decision_time ==
                expected_decision_time,
            "Stock decision time mismatch.");

        require(
            seen.insert(
                stock.symbol).
                second,
            "Duplicate stock in snapshot.");
    }
}


void compareDeterministicIdentity(
    const devai::ranking::CrossSectionalUniverseSnapshot& a,
    const devai::ranking::CrossSectionalUniverseSnapshot& b)
{
    require(
        a.decision_time ==
            b.decision_time,
        "Determinism failure: decision time changed.");

    require(
        a.benchmark_symbol ==
            b.benchmark_symbol,
        "Determinism failure: benchmark changed.");

    require(
        a.stock_count ==
            b.stock_count,
        "Determinism failure: stock count changed.");

    require(
        a.stocks.size() ==
            b.stocks.size(),
        "Determinism failure: stock vector size changed.");

    for (std::size_t index = 0;
         index < a.stocks.size();
         ++index)
    {
        require(
            a.stocks[index].symbol ==
                b.stocks[index].symbol,
            "Determinism failure: stock ordering changed.");

        require(
            a.stocks[index].decision_time ==
                b.stocks[index].decision_time,
            "Determinism failure: stock decision time changed.");
    }
}

} // namespace


int main()
{
    try
    {
        std::cout
            << "\n"
            << "==========================================================================\n"
            << "DEVAI PHASE 2.1 — HISTORICAL CETE STATE REPLAY VALIDATION\n"
            << "==========================================================================\n";

        HistoricalCETEReplayConfig config;

        // Keep the validation runtime bounded. Production may set this to 0
        // to use the complete discovered universe.
        config.maximum_stock_count = 3;
        config.minimum_universe_size = 2;

        HistoricalCETEReplay replay(
            config);

        const auto symbols =
            replay.discoverSymbols();

        require(
            symbols.size() >=
                config.minimum_universe_size,
            "Insufficient symbols discovered.");

        for (const auto& symbol :
             symbols)
        {
            require(
                symbol !=
                    config.benchmark_symbol,
                "Benchmark discovered as tradable stock.");
        }

        std::cout
            << "[PASS] Symbol discovery / benchmark exclusion\n";

        const std::int64_t decision_time =
            replay.latestCommonDecisionTime();

        require(
            decision_time > 0,
            "Invalid common decision time.");

        std::cout
            << "[PASS] Common historical decision time\n";

        const auto first =
            replay.replayAt(
                decision_time);

        validateSnapshot(
            first.snapshot,
            config,
            decision_time);

        require(
            first.stats.passed(),
            "Replay statistics failed.");

        require(
            first.stats.lookahead_violations == 0,
            "Look-ahead violation detected.");

        require(
            first.stats.decision_time_violations == 0,
            "Decision-time violation detected.");

        require(
            first.stats.benchmark_contamination == 0,
            "Benchmark contamination detected.");

        require(
            first.stats.duplicate_symbol_violations == 0,
            "Duplicate symbol violation detected.");

        std::cout
            << "[PASS] Real CETE Phase-5 snapshot replay\n"
            << "[PASS] Decision-time causality\n"
            << "[PASS] Look-ahead protection\n"
            << "[PASS] Benchmark exclusion\n"
            << "[PASS] Duplicate-symbol protection\n"
            << "[PASS] Numerical integrity\n";

        const auto second =
            replay.replayAt(
                decision_time);

        validateSnapshot(
            second.snapshot,
            config,
            decision_time);

        compareDeterministicIdentity(
            first.snapshot,
            second.snapshot);

        require(
            second.stats.passed(),
            "Second deterministic replay failed.");

        std::cout
            << "[PASS] Deterministic historical replay\n";

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Discovered stocks      : "
            << first.stats.discovered_stock_count
            << '\n'
            << "Processed stocks       : "
            << first.stats.stocks_processed
            << '\n'
            << "Skipped stocks         : "
            << first.stats.stocks_skipped
            << '\n'
            << "Look-ahead violations  : "
            << first.stats.lookahead_violations
            << '\n'
            << "Decision-time violations: "
            << first.stats.decision_time_violations
            << '\n'
            << "Benchmark contamination: "
            << first.stats.benchmark_contamination
            << '\n'
            << "Phase-4 exceptions     : "
            << first.stats.phase4_exceptions
            << '\n'
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.1 HISTORICAL CETE STATE REPLAY VALIDATION PASSED\n"
            << "==========================================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nDEVAI PHASE 2.1 VALIDATION FAILED\n"
            << exception.what()
            << '\n';

        return 1;
    }
}
