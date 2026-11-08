#include "devai/strategy/dev_ai_strategy/reward/DevDualModeTransition_D9F4C.hpp"
#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::d9f4c {
namespace {
using Action = devai::strategy::d9f::Action;
bool close(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a-b) <= 1e-10*std::max({1.0,std::abs(a),std::abs(b)});
}
void validAction(Action a) {
    if (static_cast<std::size_t>(a)>static_cast<std::size_t>(Action::SWITCH))
        throw std::invalid_argument("D9F4C invalid action");
}
void legal(const std::vector<Action>& actions) {
    if(actions.empty()) throw std::invalid_argument("D9F4C empty legal action mask");
    for(std::size_t i=0;i<actions.size();++i) {
        validAction(actions[i]);
        if(std::find(actions.begin(),actions.begin()+static_cast<std::ptrdiff_t>(i),actions[i])!=actions.begin()+static_cast<std::ptrdiff_t>(i))
            throw std::invalid_argument("D9F4C duplicate legal action");
    }
}
void stateValid(const devai::strategy::d9f::State& s) {
    if(!std::isfinite(s.candidate_score)||!std::isfinite(s.held_score)||!std::isfinite(s.advantage)||
       s.position_age<0||s.minutes_to_close<0||s.entries_remaining<0)
        throw std::invalid_argument("D9F4C invalid causal state");
}
}
Transition DevDualModeTransition::build(const TransitionInput& in, RewardMode mode) {
    if(mode!=RewardMode::GROSS_MARKET_REWARD && mode!=RewardMode::NET_EXECUTION_REWARD)
        throw std::invalid_argument("D9F4C invalid reward mode");
    stateValid(in.state);stateValid(in.next_state);validAction(in.action);legal(in.legal_actions);
    if(std::find(in.legal_actions.begin(),in.legal_actions.end(),in.action)==in.legal_actions.end())
        throw std::invalid_argument("D9F4C action not legal at decision time");
    if(in.terminal) {
        if(in.next_decision_time!=0 && in.next_decision_time<=in.decision_time)
            throw std::invalid_argument("D9F4C invalid terminal timestamp");
        if(!in.next_legal_actions.empty()) throw std::invalid_argument("D9F4C terminal bootstrap mask must be empty");
    } else {
        if(in.next_decision_time<=in.decision_time) throw std::invalid_argument("D9F4C look-ahead/chronology guard");
        legal(in.next_legal_actions);
    }
    if(in.decision_time<=0) throw std::invalid_argument("D9F4C missing decision timestamp");
    if(!std::isfinite(in.actual_costs.brokerage_and_statutory)||in.actual_costs.brokerage_and_statutory<0||
       !std::isfinite(in.actual_costs.slippage)||in.actual_costs.slippage<0)
        throw std::invalid_argument("D9F4C invalid actual costs");
    const double costs=in.actual_costs.brokerage_and_statutory+in.actual_costs.slippage;
    if(!std::isfinite(costs)) throw std::invalid_argument("D9F4C cost overflow");
    if(mode==RewardMode::GROSS_MARKET_REWARD && !in.actual_costs.reconciled_to_equity)
        throw std::invalid_argument("D9F4C gross mode requires reconciled actual execution costs");
    // D9F4B is the single source of truth for net-equity reward.
    devai::d9f4b::RewardInput ri{};
    ri.equity_before=in.equity_before;
    ri.equity_after=in.equity_after;
    ri.reference_capital=in.reference_capital;
    // Action is WAIT for this *equity-only* calculation: execution classification
    // belongs to the actual fill ledger, not inferred from manager intent.
    ri.action=devai::d9f4b::Action::WAIT;
    const auto net=devai::d9f4b::DevCostAwareRewardEngine::evaluate(ri).reward;
    const double gross=(in.equity_after-in.equity_before+costs)/in.reference_capital;
    if(!std::isfinite(gross)) throw std::invalid_argument("D9F4C gross reward overflow");
    Transition out{};
    out.state=in.state;out.action=in.action;out.next_state=in.next_state;
    out.next_legal_actions=in.next_legal_actions;
    out.decision_time=in.decision_time;out.next_decision_time=in.next_decision_time;
    out.terminal=in.terminal;out.net_reward=net;out.gross_reward=gross;
    out.actual_costs=costs;
    out.reward=mode==RewardMode::NET_EXECUTION_REWARD?net:gross;
    return out;
}
Transition DevDualModeTransition::fromHistorical(
    const devai::strategy::DevHistoricalExperience& h,TransitionInput in,RewardMode mode) {
    if(in.decision_time!=h.decision_time || in.next_decision_time!=h.next_decision_time ||
       in.terminal!=h.terminal || !close(in.equity_before,h.equity_before) ||
       !close(in.equity_after,h.equity_after))
        throw std::invalid_argument("D9F4C historical replay / causal input mismatch");
    const auto result=build(in,mode);
    if(!close(result.net_reward,h.portfolio_reward))
        throw std::logic_error("D9F4C historical net reward reconciliation failed");
    return result;
}
std::vector<Transition> DevDualModeTransition::buildSequence(
    const std::vector<TransitionInput>& inputs,RewardMode mode) {
    if(inputs.empty()) throw std::invalid_argument("D9F4C empty transition sequence");
    std::vector<Transition> out;out.reserve(inputs.size());
    for(std::size_t i=0;i<inputs.size();++i) {
        if(i && (inputs[i-1].terminal || inputs[i].decision_time!=inputs[i-1].next_decision_time ||
                 !close(inputs[i].equity_before,inputs[i-1].equity_after) ||
                 !close(inputs[i].reference_capital,inputs[i-1].reference_capital)))
            throw std::invalid_argument("D9F4C transition sequence continuity failed");
        if(i+1<inputs.size() && inputs[i].terminal)
            throw std::invalid_argument("D9F4C early terminal transition");
        out.push_back(build(inputs[i],mode));
    }
    return out;
}
} // namespace devai::d9f4c
