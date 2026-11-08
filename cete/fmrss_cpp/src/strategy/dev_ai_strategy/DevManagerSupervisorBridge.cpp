#include "devai/strategy/dev_ai_strategy/DevManagerSupervisorBridge.hpp"
#include "devai/execution/DevHoldDecision.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevManagerSupervisorBridge::DevManagerSupervisorBridge(
    std::string id, std::array<std::string, DEV_AGENT_SLOT_COUNT> agents,
    DevinderjeetSupervisor& supervisor,
    const devai::execution::DevSecureExecutionPath& execution)
    : manager_id_(std::move(id)), global_agents_(std::move(agents)),
      supervisor_(supervisor), executor_(manager_id_, execution) {
    if (manager_id_.empty()) throw std::invalid_argument("manager ID required");
    for (std::size_t i=0;i<global_agents_.size();++i) {
        if(global_agents_[i].empty()) throw std::invalid_argument("empty global agent ID");
        for(std::size_t j=0;j<i;++j)
            if(global_agents_[i]==global_agents_[j]) throw std::invalid_argument("duplicate agent ID");
    }
    const auto account=supervisor_.managerAccount(manager_id_);
    if(account.registration.agents != std::vector<std::string>(global_agents_.begin(),global_agents_.end()))
        throw std::invalid_argument("bridge agent roster differs from supervisor registration");
}
DevManagerState DevManagerSupervisorBridge::buildState(
    std::int64_t time, const std::vector<DevAgentReport>& reports) const {
    auto account=supervisor_.managerAccount(manager_id_);
    if(account.positions.size()>1) throw std::logic_error("existing DevManager supports one position per manager");
    DevManagerPortfolioState portfolio{};
    portfolio.numerical_integrity=true;
    portfolio.portfolio.position=DEV_POSITION_FLAT;
    if(!account.positions.empty()) {
        portfolio.current_symbol=account.positions.begin()->first;
        portfolio.portfolio.position=DEV_POSITION_LONG;
    }
    // Other portfolio metrics are intentionally zero: supervisor foundation
    // does not expose mark-to-market drawdown or holding duration per manager.
    std::vector<DevAgentReport> local;
    local.reserve(reports.size());
    for(const auto& r:reports) {
        if(r.slot_index>=DEV_AGENT_SLOT_COUNT || r.agent_name!=global_agents_[r.slot_index])
            throw std::invalid_argument("global agent does not belong to manager slot");
        auto copy=r;
        copy.agent_name=DevTopCandidateSelector::agentNames()[r.slot_index];
        local.push_back(std::move(copy));
    }
    return manager_.buildState(time,local,portfolio);
}
DevOrderReport DevManagerSupervisorBridge::submit(
    const DevManagerState& state, const DevManagerDecision& decision,
    DevSupervisedAction action, const std::string& order_id,
    std::int64_t shares, double price) {
    manager_.validateDecision(state,decision);
    if(order_id.empty()) throw std::invalid_argument("order ID required");
    const auto account=supervisor_.managerAccount(manager_id_);
    if(account.positions.size()>1) throw std::logic_error("one position per manager bridge");
    const bool held=!account.positions.empty();
    const std::string held_symbol=held?account.positions.begin()->first:"";
    const std::string held_agent=held?account.positions.begin()->second.agent_id:"";
    const bool state_held=state.portfolio.portfolio.position!=DEV_POSITION_FLAT;
    if(state_held!=held || (held && state.portfolio.current_symbol!=held_symbol))
        throw std::invalid_argument("stale manager portfolio state");
    if(action==DevSupervisedAction::HOLD) {
        if(shares!=0) throw std::invalid_argument("HOLD quantity must be zero");
        // HOLD is a decision, not a broker order. Do not invoke the secure
        // execution gateway, mutate the supervisor or append to order reports.
        // Authorization for BUY/SELL/EXIT remains enforced by executor_.trade().
        return devai::execution::makeHoldDecision(
            order_id, manager_id_, state.decision_time);
    }
    if(!std::isfinite(price)||price<=0.0) throw std::invalid_argument("execution price must be positive");
    if(action==DevSupervisedAction::BUY) {
        if(held || decision.type!=DevManagerDecisionType::SELECT_AGENT || !decision.selected_slot_index)
            throw std::invalid_argument("BUY requires flat portfolio and SELECT_AGENT");
        const auto slot=*decision.selected_slot_index;
        const auto& report=*state.agent_reports[slot];
        if(report.preferred_action!=DevAction::LONG)
            throw std::invalid_argument("BUY requires agent LONG proposal");
        if(shares<=0) throw std::invalid_argument("BUY requires positive shares");
        return executor_.trade(order_id,global_agents_[slot],report.symbol,action,
                               state.decision_time,shares,price);
    }
    if(action!=DevSupervisedAction::SELL && action!=DevSupervisedAction::EXIT)
        throw std::invalid_argument("unsupported supervised action");
    if(!held || shares!=0) throw std::invalid_argument("exit requires position and zero quantity (full exit)");
    if(decision.type==DevManagerDecisionType::SELECT_AGENT && decision.selected_symbol!=held_symbol)
        throw std::invalid_argument("EXIT selection differs from held symbol");
    return executor_.trade(order_id,held_agent,held_symbol,action,state.decision_time,0,price);
}
} // namespace devai::strategy
