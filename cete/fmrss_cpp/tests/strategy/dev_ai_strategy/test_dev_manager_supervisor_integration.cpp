#include "devai/strategy/dev_ai_strategy/DevManagerSupervisorBridge.hpp"
#include "Dev5C3HTestFixture.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
namespace fx=cete_5c3h_test;

static DevAgentReport report(std::string name,std::string symbol,std::int64_t t) {
    DevAgentReport r{};
    r.slot_index=0;r.agent_name=std::move(name);r.assigned=true;
    r.symbol=std::move(symbol);r.decision_time=t;r.candidate_rank=1;
    r.candidate_score=0.9;r.q_values.values={0.1,0.5,0.0,-0.1};
    r.q_values.numerical_integrity=true;r.preferred_action=DevAction::LONG;
    r.preferred_q_value=0.5;r.numerical_integrity=true;
    return r;
}
int main() {
    DevinderjeetSupervisor s({100000.0,0.75,5.0,2.0,2});
    auto a=fx::ids(1),b=fx::ids(6);
    s.registerManager({"DevManager_001",{a.begin(),a.end()},50000.0});
    s.registerManager({"DevManager_002",{b.begin(),b.end()},50000.0});
    fx::Fixture f(std::filesystem::temp_directory_path()/"cete_5c3h_bridge_test",s);
    f.add("DevManager_001",{"RELIANCE"});
    f.add("DevManager_002",{"HDFCBANK"});
    DevManagerSupervisorBridge m1("DevManager_001",a,s,f.execution);
    DevManagerSupervisorBridge m2("DevManager_002",b,s,f.execution);
    DevManager logic;
    auto state1=m1.buildState(1000,{report(a[0],"RELIANCE",1000)});
    auto state2=m2.buildState(1000,{report(b[0],"HDFCBANK",1000)});
    auto d1=logic.makeSelectAgentDecision(state1,0);
    auto d2=logic.makeSelectAgentDecision(state2,0);
    auto x=m1.submit(state1,d1,DevSupervisedAction::BUY,"order-1",10,2500.0);
    auto y=m2.submit(state2,d2,DevSupervisedAction::BUY,"order-2",20,1500.0);
    assert(x.status==DevOrderStatus::FILLED && y.status==DevOrderStatus::FILLED);
    assert(s.snapshot().open_positions==2);
    assert(s.managerAccount("DevManager_002").positions.at("HDFCBANK").agent_id=="DevAgent_006");
    bool rejected=false;
    try { (void)m2.submit(state2,d2,DevSupervisedAction::BUY,"order-3",1,1500.0); }
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    auto next=m1.buildState(1060,{report(a[0],"RELIANCE",1060)});
    auto exit=logic.makeNoTradeDecision(next);
    auto z=m1.submit(next,exit,DevSupervisedAction::EXIT,"order-4",0,2510.0);
    assert(z.status==DevOrderStatus::FILLED);
    assert(s.snapshot().open_positions==1);
    assert(m1.reports().size()==2 && m2.reports().size()==1);
    f.authorized=false;
    assert(!f.execution.eligible("DevManager_002","HDFCBANK"));
    bool blocked=false;
    try {
        (void)f.execution.submit({"blocked","DevManager_002","DevAgent_006",
                                   "HDFCBANK",DevSupervisedAction::BUY,1200,1,1500.0});
    } catch(const std::runtime_error&) {blocked=true;}
    assert(blocked);
    std::cout<<"PASS secure DevManagerSupervisorBridge two-manager BUY/EXIT and authorization\n";
}
