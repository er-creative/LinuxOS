#include "devai/strategy/dev_ai_strategy/DevManagerDirectionAdapter_F3EF2C5A.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace devai::strategy::f3ef2c5a {
namespace {
DevManagerDecision materialize(DevManagerAction action, const DevManagerState& state,
                               const DevManager& manager) {
    if (!isValidManagerAction(action)) throw std::invalid_argument("invalid manager action");
    if (action == DevManagerAction::NO_TRADE) return manager.makeNoTradeDecision(state);
    if (action == DevManagerAction::KEEP_CURRENT) return manager.makeKeepCurrentDecision(state);
    return manager.makeSelectAgentDecision(state, static_cast<std::size_t>(action)-2);
}
}
Result decide(const Input& input) {
    if (!input.state || !input.manager) throw std::invalid_argument("null manager/state");
    const auto& state = *input.state;
    const auto& manager = *input.manager;
    manager.validateState(state);
    if (input.context.position.open != (state.portfolio.portfolio.position != DEV_POSITION_FLAT))
        throw std::invalid_argument("portfolio/bridge position mismatch");
    if (state.decision_time != input.context.decision_key)
        throw std::invalid_argument("decision timestamp mismatch");
    Result result{};
    result.manager_decision = materialize(input.manager_action, state, manager);
    manager.validateDecision(state, result.manager_decision);
    result.plan = {f3ef2c5::PlanKind::WAIT, {}, 0, 0, "Manager did not authorize a new entry"};
    // Never interpret NO_TRADE as an EXIT. KEEP_CURRENT remains with DevManager.
    if (result.manager_decision.type != DevManagerDecisionType::SELECT_AGENT)
        return result;
    // An existing position must be managed through HOLD/EXIT/SWITCH lifecycle,
    // never through a second OPEN order.
    if (state.portfolio.portfolio.position != DEV_POSITION_FLAT || input.context.position.open)
        return result;
    const auto slot = *result.manager_decision.selected_slot_index;
    const auto& report = *state.agent_reports[slot];
    const auto& exec = input.execution[slot];
    if (!std::isfinite(exec.completed_candle_price) || exec.completed_candle_price <= 0 ||
        exec.quantity == 0) return result;
    // The same model's four agent outputs: HOLD=0 LONG=1 SHORT=2 EXIT=3.
    // Never reinterpret the manager's 2..6 indices as directional actions.
    f3ef2c5::AgentAssessment a{};
    a.symbol = report.symbol;
    a.rank = report.candidate_rank;
    a.decision_key = report.decision_time;
    a.assigned = report.assigned;
    a.numerical_integrity = report.numerical_integrity && report.q_values.numerical_integrity;
    a.shortable = exec.shortable;
    a.shared_model_supports_short = input.context.trained_shared_model_supports_short;
    a.q_wait = report.q_values.values[0];
    a.q_open_long = report.q_values.values[1];
    a.q_open_short = report.q_values.values[2];
    a.fill_price = exec.completed_candle_price;
    a.quantity = exec.quantity;
    result.plan = f3ef2c5::decide(input.context, std::vector<f3ef2c5::AgentAssessment>{a});
    return result;
}
}
