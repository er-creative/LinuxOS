#include "devai/strategy/dev_ai_strategy/reward/DevDualModeTransition_D9F4C.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::d9f4c;
using devai::strategy::d9f::Action;
static bool eq(double a,double b){return std::abs(a-b)<1e-12;}
template<class F> static void mustThrow(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(){
    TransitionInput a{};
    a.decision_time=1000;a.next_decision_time=1060;
    a.equity_before=7000;a.equity_after=6995;
    a.action=Action::OPEN;a.legal_actions={Action::WAIT,Action::OPEN};
    a.next_legal_actions={Action::HOLD,Action::EXIT};
    a.actual_costs={5.0,0.0,true};
    auto net=DevDualModeTransition::build(a,RewardMode::NET_EXECUTION_REWARD);
    auto gross=DevDualModeTransition::build(a,RewardMode::GROSS_MARKET_REWARD);
    assert(eq(net.reward,-5.0/7000.0));assert(eq(gross.reward,0.0));
    assert(eq(net.gross_reward,gross.gross_reward));
    assert(eq(net.net_reward,gross.net_reward));
    TransitionInput b=a;b.decision_time=1060;b.next_decision_time=1120;
    b.equity_before=6995;b.equity_after=7025;b.action=Action::HOLD;
    b.legal_actions={Action::HOLD,Action::EXIT};b.next_legal_actions={Action::EXIT};
    b.actual_costs={0,0,true};
    TransitionInput c=b;c.decision_time=1120;c.next_decision_time=0;
    c.equity_before=7025;c.equity_after=7021;c.action=Action::EXIT;
    c.legal_actions={Action::EXIT};c.next_legal_actions={};c.terminal=true;
    c.actual_costs={4,0,true};
    const auto ns=DevDualModeTransition::buildSequence({a,b,c},RewardMode::NET_EXECUTION_REWARD);
    const auto gs=DevDualModeTransition::buildSequence({a,b,c},RewardMode::GROSS_MARKET_REWARD);
    double nr=0,gr=0;for(const auto& t:ns)nr+=t.reward;for(const auto& t:gs)gr+=t.reward;
    assert(eq(nr,21.0/7000.0));assert(eq(gr,30.0/7000.0));
    auto bad=a;bad.actual_costs.reconciled_to_equity=false;
    mustThrow([&]{(void)DevDualModeTransition::build(bad,RewardMode::GROSS_MARKET_REWARD);});
    bad=a;bad.next_decision_time=999;
    mustThrow([&]{(void)DevDualModeTransition::build(bad,RewardMode::NET_EXECUTION_REWARD);});
    bad=a;bad.action=Action::SWITCH;
    mustThrow([&]{(void)DevDualModeTransition::build(bad,RewardMode::NET_EXECUTION_REWARD);});
    bad=a;bad.actual_costs.brokerage_and_statutory=-1;
    mustThrow([&]{(void)DevDualModeTransition::build(bad,RewardMode::NET_EXECUTION_REWARD);});
    bad=b;bad.equity_before=7000;
    mustThrow([&]{(void)DevDualModeTransition::buildSequence({a,bad,c},RewardMode::NET_EXECUTION_REWARD);});
    devai::strategy::DevHistoricalExperience h{};
    h.decision_time=a.decision_time;h.next_decision_time=a.next_decision_time;
    h.equity_before=a.equity_before;h.equity_after=a.equity_after;
    h.portfolio_reward=-5.0/7000.0;
    const auto ht=DevDualModeTransition::fromHistorical(h,a,RewardMode::GROSS_MARKET_REWARD);
    assert(eq(ht.reward,0));
    h.portfolio_reward=0;
    mustThrow([&]{(void)DevDualModeTransition::fromHistorical(h,a,RewardMode::NET_EXECUTION_REWARD);});
    std::cout<<"D9F4C_DUAL_MODE_TRANSITION_PASS gross_net=1 historical=1 chronology=1 legal_mask=1 cost_provenance=1 continuity=1 terminal=1\n";
    std::cout<<"D9F4C_CONTRACT_PASS model_trained=0 oos_evaluated=0 production_policy_modified=0\n";
}
