#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    DevOOSLedgerConfig config{};
    config.brokerage_rate = 0.0003;
    config.slippage_rate = 0.0005;
    config.other_cost_rate = 0.0002;
    DevRealHistoricalMultiStockOOS ledger(config);
    constexpr std::int64_t day = 86400;
    const std::int64_t base = 20000 * day;
    const std::int64_t entry_time = base + 4*3600; // 09:30 IST
    const auto price = [entry_time](const std::string& symbol, std::int64_t t) -> double {
        if (symbol == "AAA") return t == entry_time ? 100.0 : 103.0;
        if (symbol == "BBB") return 50.0;
        throw std::runtime_error("No price for symbol");
    };
    const auto first = ledger.execute(entry_time, "AAA", price);
    assert(first.executed_legs == 1);
    assert(ledger.state().held_symbol == "AAA");
    assert(ledger.state().quantity > 0);
    assert(ledger.state().quantity * 100.0 <= 5250.0);
    const auto held = ledger.execute(entry_time + 60, "AAA", price);
    assert(held.executed_legs == 0);
    assert(held.gross_pnl > 0.0);
    const auto rotated = ledger.execute(entry_time + 120, "BBB", price);
    assert(rotated.executed_legs == 2);
    assert(ledger.state().held_symbol == "BBB");
    assert(ledger.state().quantity > 0);
    assert(ledger.state().cash >= 0.0);
    assert(std::fabs(rotated.net_pnl - (rotated.gross_pnl - rotated.brokerage -
           rotated.slippage - rotated.other_costs)) < 1e-8);
    const auto exited = ledger.execute(entry_time + 180, "", price);
    assert(exited.executed_legs == 1);
    assert(ledger.state().quantity == 0);
    assert(ledger.state().held_symbol.empty());
    bool threw = false;
    try { (void)ledger.execute(entry_time + 180, "AAA", price); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    // 15:15 IST (09:45 UTC): square off LONG and reject new entries.
    // Use a fresh ledger and timestamps from a single illustrative UTC day.
    DevRealHistoricalMultiStockOOS cutoff_ledger(config);
    const auto open_long = cutoff_ledger.execute(base + 9*3600 + 43*60, "AAA", price);
    assert(open_long.executed_legs == 1);
    const auto square_off = cutoff_ledger.execute(base + 9*3600 + 45*60, "AAA", price);
    assert(square_off.executed_legs == 1);
    assert(cutoff_ledger.state().quantity == 0);
    const auto no_reentry = cutoff_ledger.execute(base + 9*3600 + 46*60, "BBB", price);
    assert(no_reentry.executed_legs == 0);
    assert(cutoff_ledger.state().quantity == 0);
    bool short_blocked = false;
    DevRealHistoricalMultiStockOOS short_ledger(config);
    try { (void)short_ledger.execute(base + 4*3600, "SHORT:AAA", price); }
    catch (const std::invalid_argument&) { short_blocked = true; }
    assert(short_blocked);
    std::cout << "3.19B shared-cash ledger contract: PASSED (fixture prices, not historical OOS)\n";
}
