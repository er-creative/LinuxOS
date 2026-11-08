#include "devai/strategy/dev_ai_strategy/DevSixAgentArchitecture_F3EF2C5E.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy::f3ef2c5e;
int main() {
    std::vector<RankedStock> stocks;
    for(int i=0;i<10;++i) stocks.push_back({"STOCK"+std::to_string(i),double(i),true});
    const auto selected=select(stocks);
    assert(selected.agents[0].stock.symbol=="STOCK9");
    assert(selected.agents[4].stock.symbol=="STOCK5");
    assert(selected.agents[5].stock.symbol=="STOCK0");
    assert(selected.agents[5].statistical_rank==10);
    static_assert(manager_action_count==8 && manager_input_size==73);
    std::array<AgentAssessment,agent_count> reports{};
    for(std::size_t i=0;i<agent_count;++i) {
        reports[i].assignment=selected.agents[i];
        reports[i].q_direction=2; reports[i].q_wait=0;
        reports[i].estimated_net_profit=1.0; reports[i].probability=.8; reports[i].valid=true;
    }
    reports[5].estimated_net_profit=4.0;
    Policy policy{};
    auto decision=decide(reports,{},policy);
    assert(decision.kind==DecisionKind::OPEN_SHORT && decision.agent_slot==5);
    ExistingPosition held{true,"STOCK9",Direction::LONG,5.0};
    assert(decide(reports,held,policy).kind==DecisionKind::KEEP);
    held.estimated_hold_net_profit=1.0;
    assert(decide(reports,held,policy).kind==DecisionKind::EXIT_THEN_OPEN_SHORT);
    policy.allow_short=false;
    assert(decide(reports,{},policy).kind==DecisionKind::OPEN_LONG);
    policy.allow_short=true; reports[5].assignment.stock.shortable=false;
    assert(decide(reports,{},policy).kind==DecisionKind::OPEN_LONG);
    policy.entry_window_open=false;
    assert(decide(reports,held,policy).kind==DecisionKind::KEEP);
    std::cout<<"F3E-F2C.5E PASS (Top5 LONG, rank10 SHORT, 6 slots, net switch guard)\n";
}
