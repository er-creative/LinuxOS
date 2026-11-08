#include "devai/strategy/dev_ai_strategy/reward/DevProductionTransitionIntegration_D9F4C.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy::d9f4c {
namespace {
void require(bool condition, const char* msg) {
    if (!condition) throw std::invalid_argument(msg);
}
}
ProductionTransition DevProductionTransitionIntegration::build(
    const ProductionExecution& event, LedgerRewardMode mode, double capital) {
    // The actual shared-manager action mask is derived from the real manager state.
    DevManagerStateEncoder encoder;
    const auto encoded = encoder.encode(event.state);
    (void)encoder.encode(event.next_state);
    const auto action = static_cast<std::size_t>(event.action);
    require(action < DEV_MANAGER_ACTION_COUNT && encoded.valid_actions[action],
            "D9F4C: action not legal for causal manager state");
    const auto& e = event.execution;
    require(e.numerical_integrity, "D9F4C: unverified ledger execution");
    require(e.decision_time == event.state.decision_time,
            "D9F4C: execution timestamp must match state decision");
    require(event.next_state.decision_time > event.state.decision_time,
            "D9F4C: next state must follow execution");
    // Require actual ledger-generated gross/net/cost fields, not inferred costs.
    LedgerTransitionInput input{};
    input.decision_time = event.state.decision_time;
    input.next_decision_time = event.next_state.decision_time;
    input.manager_action = action;
    input.legal_actions = encoded.valid_actions;
    input.equity_before = e.equity_before;
    input.equity_after = e.equity_after;
    input.gross_pnl = e.gross_pnl;
    input.net_pnl = e.net_pnl;
    input.brokerage = e.brokerage;
    input.slippage = e.slippage;
    input.other_costs = e.other_costs;
    input.reference_capital = capital;
    input.terminal = event.terminal;
    input.executed_ledger_reconciled = true; // actual typed ledger execution
    auto audit = DevLedgerRewardBridge::build(input, mode);
    DevManagerTransition manager{};
    manager.state = event.state;
    manager.action = event.action;
    manager.reward = audit.selected_reward;
    manager.next_state = event.next_state;
    manager.terminal = event.terminal;
    manager.decision_time = event.state.decision_time;
    manager.next_decision_time = event.next_state.decision_time;
    manager.numerical_integrity = true;
    DevManagerTrainer::validateTransition(manager);
    return {std::move(manager), std::move(audit)};
}
std::vector<ProductionTransition> DevProductionTransitionIntegration::buildSequence(
    const std::vector<ProductionExecution>& events, LedgerRewardMode mode,
    double capital) {
    std::vector<ProductionTransition> out;
    out.reserve(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i) {
            require(!events[i-1].terminal, "D9F4C: event after terminal");
            require(events[i-1].next_state.decision_time == events[i].state.decision_time,
                    "D9F4C: non-contiguous manager state times");
            require(events[i-1].execution.equity_after == events[i].execution.equity_before,
                    "D9F4C: ledger equity discontinuity (mark-to-market must be explicit)");
        }
        out.push_back(build(events[i], mode, capital));
    }
    return out;
}
} // namespace devai::strategy::d9f4c
