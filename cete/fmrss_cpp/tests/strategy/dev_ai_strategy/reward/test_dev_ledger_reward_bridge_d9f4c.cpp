#include "devai/strategy/dev_ai_strategy/reward/DevLedgerRewardBridge_D9F4C.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy::d9f4c;
int main() {
    LedgerTransitionInput x{};
    x.decision_time=1000; x.next_decision_time=1060;
    x.manager_action=2; x.legal_actions[0]=true; x.legal_actions[2]=true;
    x.equity_before=7000; x.equity_after=7004;
    x.gross_pnl=10; x.net_pnl=4; x.brokerage=3; x.slippage=2; x.other_costs=1;
    x.executed_ledger_reconciled=true;
    auto g=DevLedgerRewardBridge::build(x,LedgerRewardMode::GROSS_MARKET_REWARD);
    auto n=DevLedgerRewardBridge::build(x,LedgerRewardMode::NET_EXECUTION_REWARD);
    assert(std::fabs(g.selected_reward-10.0/7000)<1e-12);
    assert(std::fabs(n.selected_reward-4.0/7000)<1e-12);
    assert(g.execution_costs==6);
    auto bad=x; bad.net_pnl=5;
    try { (void)DevLedgerRewardBridge::build(bad,LedgerRewardMode::NET_EXECUTION_REWARD); assert(false); } catch(const std::invalid_argument&) {}
    bad=x; bad.manager_action=3;
    try { (void)DevLedgerRewardBridge::build(bad,LedgerRewardMode::NET_EXECUTION_REWARD); assert(false); } catch(const std::invalid_argument&) {}
    bad=x; bad.executed_ledger_reconciled=false;
    try { (void)DevLedgerRewardBridge::build(bad,LedgerRewardMode::GROSS_MARKET_REWARD); assert(false); } catch(const std::invalid_argument&) {}
    std::cout << "D9F4C_LEDGER_BRIDGE_PASS gross_net=1 equity_reconciliation=1 seven_action_mask=1 provenance=1 model_trained=0\n";
}
