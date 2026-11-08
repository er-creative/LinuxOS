#include "devai/strategy/dev_ai_strategy/reward/DevLedgerRewardBridge_D9F4C.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy::d9f4c {
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
bool close(double a, double b) { return std::fabs(a-b) <= 1e-7 * std::max({1.0,std::fabs(a),std::fabs(b)}); }
}
LedgerRewardTransition DevLedgerRewardBridge::build(const LedgerTransitionInput& x, LedgerRewardMode mode) {
    check(x.executed_ledger_reconciled, "D9F4C: execution ledger provenance required");
    check(x.decision_time > 0 && (x.terminal ? (x.next_decision_time==0 || x.next_decision_time>x.decision_time) : x.next_decision_time>x.decision_time), "D9F4C: invalid chronology");
    check(x.manager_action < x.legal_actions.size() && x.legal_actions[x.manager_action], "D9F4C: invalid manager action/mask");
    for (double v : {x.equity_before,x.equity_after,x.gross_pnl,x.net_pnl,x.brokerage,x.slippage,x.other_costs,x.reference_capital})
        check(std::isfinite(v), "D9F4C: non-finite ledger input");
    check(x.reference_capital>0 && x.equity_before>0 && x.equity_after>0, "D9F4C: invalid capital/equity");
    check(x.brokerage>=0 && x.slippage>=0 && x.other_costs>=0, "D9F4C: negative execution cost");
    const double costs=x.brokerage+x.slippage+x.other_costs;
    check(close(x.gross_pnl-costs,x.net_pnl), "D9F4C: gross/net ledger mismatch");
    check(close(x.equity_after-x.equity_before,x.net_pnl), "D9F4C: equity/net ledger mismatch");
    LedgerRewardTransition result{};
    result.decision_time=x.decision_time;
    result.next_decision_time=x.next_decision_time;
    result.manager_action=x.manager_action;
    result.execution_costs=costs;
    result.gross_reward=x.gross_pnl/x.reference_capital;
    result.net_reward=x.net_pnl/x.reference_capital;
    result.selected_reward=mode==LedgerRewardMode::GROSS_MARKET_REWARD ? result.gross_reward : result.net_reward;
    result.terminal=x.terminal;
    return result;
}
}
