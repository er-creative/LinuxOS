#include "devai/strategy/dev_ai_strategy/DevSharedModelMultiManagerRuntime.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevSharedModelMultiManagerRuntime::DevSharedModelMultiManagerRuntime(
    const DevQNetwork& agent, const DevManagerQNetwork& manager,
    DevinderjeetSupervisor& supervisor,
    const devai::execution::DevSecureExecutionPath& execution)
    : agent_model_(agent), manager_model_(manager), supervisor_(supervisor),
      execution_(execution) {}

void DevSharedModelMultiManagerRuntime::addManager(
    std::string id, std::array<std::string, DEV_AGENT_SLOT_COUNT> agents) {
    if(managers_.count(id)) throw std::invalid_argument("duplicate manager");
    auto bridge=std::make_unique<DevManagerSupervisorBridge>(id,std::move(agents),supervisor_,execution_);
    managers_.emplace(std::move(id),std::move(bridge));
}
DevSharedManagerCycle DevSharedModelMultiManagerRuntime::evaluate(
    const std::string& id, std::int64_t time,
    const std::vector<DevSharedCandidate>& candidates) const {
    const auto it=managers_.find(id);
    if(it==managers_.end()) throw std::invalid_argument("unregistered runtime manager");
    if(time<=0 || candidates.size()>DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument("invalid decision time or candidate count");
    DevSharedManagerCycle result{};
    result.manager_id=id;
    result.decision_time=time;
    std::array<bool,DEV_AGENT_SLOT_COUNT> seen{};
    for(const auto& c:candidates) {
        if(c.slot>=DEV_AGENT_SLOT_COUNT || seen[c.slot])
            throw std::invalid_argument("duplicate/out-of-range local slot");
        seen[c.slot]=true;
        if(c.symbol.empty() || !std::isfinite(c.candidate_score) ||
           c.candidate_rank != c.slot + 1 ||
           !c.observation.ready || !c.observation.numerical_integrity ||
           c.observation.symbol!=c.symbol || c.observation.decision_time!=time)
            throw std::invalid_argument("invalid or stale candidate observation");
        DevAgentReport r{};
        r.slot_index=c.slot;
        r.agent_name=it->second->agentIds()[c.slot];
        r.assigned=true;
        r.symbol=c.symbol;
        r.decision_time=time;
        r.candidate_score=c.candidate_score;
        r.candidate_rank=c.candidate_rank;
        r.q_values=agent_model_.predict(c.observation);
        if(!r.q_values.numerical_integrity)
            throw std::runtime_error("invalid shared agent model output");
        r.preferred_action=r.q_values.greedyAction();
        r.preferred_q_value=r.q_values.value(r.preferred_action);
        r.numerical_integrity=std::isfinite(r.preferred_q_value);
        if(!r.numerical_integrity) throw std::runtime_error("non-finite agent output");
        result.global_agent_reports.push_back(std::move(r));
    }
    result.state=it->second->buildState(time,result.global_agent_reports);
    const auto encoded=encoder_.encode(result.state);
    result.manager_q=manager_model_.predict(encoded);
    if(!result.manager_q.numerical_integrity)
        throw std::runtime_error("invalid shared manager model output");
    result.action=result.manager_q.greedyAction(encoded.valid_actions);
    switch(result.action) {
        case DevManagerAction::NO_TRADE:
            result.decision=manager_logic_.makeNoTradeDecision(result.state);break;
        case DevManagerAction::KEEP_CURRENT:
            result.decision=manager_logic_.makeKeepCurrentDecision(result.state);break;
        case DevManagerAction::SELECT_AGENT_001:
        case DevManagerAction::SELECT_AGENT_002:
        case DevManagerAction::SELECT_AGENT_003:
        case DevManagerAction::SELECT_AGENT_004:
        case DevManagerAction::SELECT_AGENT_005:
            result.decision=manager_logic_.makeSelectAgentDecision(result.state,
                static_cast<std::size_t>(result.action)-2);break;
        default: throw std::runtime_error("invalid manager action");
    }
    manager_logic_.validateDecision(result.state,result.decision);
    return result;
}
DevOrderReport DevSharedModelMultiManagerRuntime::submit(
    const DevSharedManagerCycle& cycle, DevSupervisedAction action,
    const std::string& order_id, std::int64_t shares, double price) {
    const auto it=managers_.find(cycle.manager_id);
    if(it==managers_.end()) throw std::invalid_argument("unknown manager");
    if(cycle.decision_time!=cycle.state.decision_time ||
       cycle.decision_time!=cycle.decision.decision_time)
        throw std::invalid_argument("inconsistent decision time");
    // Do not silently turn SELECT_AGENT into BUY. Cash-market action intent
    // and a separately sourced, causally valid execution price are required.
    return it->second->submit(cycle.state,cycle.decision,action,order_id,shares,price);
}
} // namespace devai::strategy
