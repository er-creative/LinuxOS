// H4.7 Stage 5E.2K.6B: deterministic mathematical-only, LONG-only OOS baseline.
// Observation values are centered cross-sectional PERCENTILES, not raw returns/Z scores.
// No future outcomes are accessed by the trading policy.
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace ds = devai::strategy;
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int minuteIST(std::int64_t t) {
    auto s = (t + 19800) % 86400;
    if (s < 0) s += 86400;
    return static_cast<int>(s / 60);
}
bool positiveReturnPercentiles(const ds::DevAIObservation& o) {
    if (!o.ready || !o.numerical_integrity) return false;
    for (const auto tf : {ds::DevTimeframe::ONE_MINUTE,
                          ds::DevTimeframe::FIVE_MINUTE,
                          ds::DevTimeframe::FIFTEEN_MINUTE}) {
        const auto i = ds::marketIndex(tf, ds::DevMetric::RETURN_VALUE);
        if (o.availability[i] != 1.0 || !std::isfinite(o.market[i]) || o.market[i] <= 0.0)
            return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument(
            "Usage: run_cete_h47_stage6b_math_only_baseline <validation_enriched.cache> <minute_folder> <ledger.csv>");
        const auto dataset = ds::DevHistoricalTrainingCache::load(argv[1]);
        ds::DevHistoricalTrainingDatasetValidator::validate(dataset);
        require(!dataset.records.empty(), "Empty historical dataset");
        std::set<std::string> symbols;
        std::size_t available_targets = 0;
        for (const auto& record : dataset.records) {
            for (const auto& c : record.candidates) {
                symbols.insert(c.symbol);
                for (const auto& o : c.outcomes)
                    if (o.available && o.numerical_integrity) ++available_targets;
            }
        }
        require(!symbols.empty(), "No candidate symbols");
        ds::DevHistoricalCloseStore prices_store;
        prices_store.loadMinuteFolder(argv[2], {symbols.begin(), symbols.end()});
        const ds::DevOOSPriceProvider prices = [&](const std::string& symbol, std::int64_t t) {
            return prices_store.completedClose(symbol, t);
        };
        std::size_t proposed_entries = 0, proposed_exits = 0, held_decisions = 0, wait_decisions = 0;
        const ds::DevFrozenManagerPolicy policy = [&](const ds::DevOOSDecisionPoint& point,
                                                       const ds::DevOOSLedgerState& ledger) {
            const int now = minuteIST(point.decision_time);
            if (now >= 15 * 60 + 15) {
                if (ledger.quantity != 0) ++proposed_exits;
                return std::string{};
            }
            // Hold the current stock while it retains bullish relative-return rankings.
            // If it falls out of Top-5, exit; do not rotate in the same minute.
            if (ledger.quantity != 0) {
                for (const auto& c : point.candidates) {
                    if (c.assignment.symbol == ledger.held_symbol &&
                        positiveReturnPercentiles(c.observation)) {
                        ++held_decisions;
                        return ledger.held_symbol;
                    }
                }
                ++proposed_exits;
                return std::string{};
            }
            // Do not open new positions in the last 30 minutes of trading.
            if (now >= 14 * 60 + 45) { ++wait_decisions; return std::string{}; }
            // F1 ranking is by absolute Z magnitude. Only bullish percentile
            // observations pass this *separate* entry filter.
            for (const auto& c : point.candidates) {
                if (positiveReturnPercentiles(c.observation)) {
                    ++proposed_entries;
                    return c.assignment.symbol;
                }
            }
            ++wait_decisions;
            return std::string{};
        };
        ds::DevOOSLedgerConfig config{};
        config.initial_capital = 7000.0;
        config.max_allocation_fraction = 0.75;
        config.allow_intraday_short = false;
        config.short_square_off_hour_ist = 15;
        config.short_square_off_minute_ist = 15;
        config.cost_mode = ds::DevOOSCostMode::ZERODHA_EQUITY_INTRADAY;
        config.exchange = ds::DevExchange::NSE;
        config.slippage_rate = 0.0005; // same 5 bps/side assumption as frozen OOS executable
        const auto report = ds::DevHistoricalOOSRunner(config).run(dataset, prices, policy);
        require(!report.steps.empty(), "No replay steps");
        require(report.steps.back().quantity == 0,
                "Replay ended with an open position; include 15:15 IST records");
        ds::DevHistoricalOOSRunner::writeCsv(argv[3], report);
        std::cout << "[CETE][H47][5E2K6B] MATHEMATICAL-ONLY BASELINE PASS (HISTORICAL, NOT LIVE)\n"
                  << "Policy: 1m/5m/15m RETURN_VALUE centered percentiles > 0; top-ranked eligible candidate\n"
                  << "Offline usable forward-return targets (not used by policy): " << available_targets << '\n'
                  << "Records: " << report.steps.size() << " Proposed entries: " << proposed_entries
                  << " exits: " << proposed_exits << " holds: " << held_decisions
                  << " waits: " << wait_decisions << '\n'
                  << "Executed legs: " << report.executed_legs << " Rotations: " << report.rotations
                  << " Final equity: " << report.final_equity << " Net PnL: " << report.net_pnl
                  << " Max drawdown: " << report.max_drawdown << '\n'
                  << "Ledger: " << argv[3] << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[CETE][H47][5E2K6B] MATHEMATICAL BASELINE FAILED: " << e.what() << '\n';
        return 1;
    }
}
