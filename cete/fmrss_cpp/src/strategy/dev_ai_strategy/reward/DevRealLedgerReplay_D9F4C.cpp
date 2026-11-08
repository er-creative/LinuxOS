#include "devai/strategy/dev_ai_strategy/reward/DevRealLedgerReplay_D9F4C.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy::d9f4c {
namespace {
void check(bool ok, const char* msg) {
    if (!ok) throw std::invalid_argument(msg);
}
int minuteIST(std::int64_t t) {
    std::int64_t local = (t + 19800) % 86400;
    if (local < 0) local += 86400;
    return static_cast<int>(local / 60);
}
bool near(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
           std::abs(a-b) <= 1e-7 * (1.0 + std::abs(a) + std::abs(b));
}
} // namespace
ReplayResult DevRealLedgerReplay::run(
    const std::vector<ReplayDecision>& decisions,
    const DevOOSPriceProvider& prices, DevOOSLedgerConfig config) {
    check(static_cast<bool>(prices), "D9F4C replay: missing price provider");
    check(std::isfinite(config.initial_capital) && config.initial_capital > 0,
          "D9F4C replay: invalid capital");
    check(config.max_allocation_fraction > 0 && config.max_allocation_fraction <= 0.75,
          "D9F4C replay: allocation above 75%");
    check(!config.allow_intraday_short,
          "D9F4C replay: set allow_intraday_short=false");
    check(config.short_square_off_hour_ist == 15 &&
          config.short_square_off_minute_ist == 15,
          "D9F4C replay: cutoff must be 15:15 IST");
    DevRealHistoricalMultiStockOOS ledger(config);
    ReplayResult result{};
    result.steps.reserve(decisions.size());
    for (std::size_t i = 0; i < decisions.size(); ++i) {
        const auto& d = decisions[i];
        check(d.decision_time > 0, "D9F4C replay: invalid timestamp");
        check(i == 0 || d.decision_time > decisions[i-1].decision_time,
              "D9F4C replay: nonchronological decisions");
        check(i == 0 || !decisions[i-1].terminal,
              "D9F4C replay: event after terminal");
        check(d.manager_action < DEV_MANAGER_ACTION_COUNT &&
              d.legal_actions[d.manager_action],
              "D9F4C replay: illegal manager action");
        // H4.6: action 6 is the fifth candidate slot (LONG in this ledger).
        // SHORT remains prohibited by allow_intraday_short=false and SHORT: guard.
        check(d.desired_symbol.rfind("SHORT:", 0) != 0,
              "D9F4C replay: SHORT disabled");
        // Action 0 is FLAT/EXIT in the current historical driver; action 1 KEEP.
        check(d.manager_action != 0 || d.desired_symbol.empty(),
              "D9F4C replay: FLAT action requested a position");
        check(d.manager_action != 1 ||
                  (!ledger.state().held_symbol.empty() &&
                   d.desired_symbol == ledger.state().held_symbol),
              "D9F4C replay: KEEP must retain existing symbol");
        const bool cutoff = minuteIST(d.decision_time) >= 15*60+15;
        check(!cutoff || d.desired_symbol.empty(),
              "D9F4C replay: positions forbidden at/after 15:15 IST");
        check(!d.terminal || d.desired_symbol.empty(),
              "D9F4C replay: terminal decision must flatten");
        const auto before = ledger.state();
        auto execution = ledger.execute(d.decision_time, d.desired_symbol, prices);
        const auto after = ledger.state();
        check(execution.numerical_integrity, "D9F4C replay: ledger integrity failed");
        check(after.quantity >= 0, "D9F4C replay: negative position");
        check(near(execution.equity_before, before.equity) &&
              near(execution.equity_after, after.equity),
              "D9F4C replay: equity snapshots disagree");
        check(near(execution.equity_after-execution.equity_before, execution.net_pnl),
              "D9F4C replay: net equity reconciliation failed");
        const double costs = execution.brokerage+execution.slippage+execution.other_costs;
        check(near(execution.gross_pnl-execution.net_pnl, costs),
              "D9F4C replay: gross/net cost reconciliation failed");
        check(!d.terminal || after.quantity == 0,
              "D9F4C replay: terminal position not flat");
        const auto next_time = i+1 < decisions.size() ?
            decisions[i+1].decision_time : d.decision_time + 60;
        LedgerTransitionInput input{};
        input.decision_time=d.decision_time;
        input.next_decision_time=next_time;
        input.manager_action=d.manager_action;
        input.legal_actions=d.legal_actions;
        input.equity_before=execution.equity_before;
        input.equity_after=execution.equity_after;
        input.gross_pnl=execution.gross_pnl;
        input.net_pnl=execution.net_pnl;
        input.brokerage=execution.brokerage;
        input.slippage=execution.slippage;
        input.other_costs=execution.other_costs;
        input.reference_capital=config.initial_capital;
        input.terminal=d.terminal;
        input.executed_ledger_reconciled=true;
        ReplayStep step{};
        step.decision=d;
        step.execution=execution;
        step.portfolio=after;
        step.gross=DevLedgerRewardBridge::build(input,LedgerRewardMode::GROSS_MARKET_REWARD);
        step.net=DevLedgerRewardBridge::build(input,LedgerRewardMode::NET_EXECUTION_REWARD);
        result.steps.push_back(std::move(step));
    }
    result.final_portfolio=ledger.state();
    return result;
}
} // namespace devai::strategy::d9f4c
