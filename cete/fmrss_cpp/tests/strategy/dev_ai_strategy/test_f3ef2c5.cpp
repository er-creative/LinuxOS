#include "devai/strategy/dev_ai_strategy/DevManagerDirectionBridge_F3EF2C5.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy;
using namespace devai::strategy::f3ef2c5;
int main() {
    Context c;
    c.decision_key = 100000;
    c.minute_of_day_ist = 10*60;
    c.risk_approved = c.model_approved = true;
    c.policy.enable_short = true;
    c.gateway_short_enabled = true;
    AgentAssessment a;
    a.symbol="RELIANCE"; a.rank=1; a.decision_key=c.decision_key;
    a.assigned=a.numerical_integrity=a.shortable=a.shared_model_supports_short=true;
    a.q_wait=0; a.q_open_long=0.5; a.q_open_short=2;
    a.fill_price=100; a.quantity=52;
    assert(decide(c,{a}).kind==PlanKind::OPEN_LONG); // untrained short is blocked
    c.trained_shared_model_supports_short=true;
    auto p=decide(c,{a});
    assert(p.kind==PlanKind::OPEN_SHORT);
    auto s=toGatewaySuggestion(p,c,"f3ef2c5-test-1");
    assert(s.intent==DevIntent::OPEN_SHORT && s.shortable && s.quantity==52);
    c.minute_of_day_ist=14*60+45;
    assert(decide(c,{a}).kind==PlanKind::WAIT);
    c.minute_of_day_ist=10*60;
    c.position.open=true;
    assert(decide(c,{a}).kind==PlanKind::WAIT); // never auto-switch
    c.position.open=false;
    c.gateway_short_enabled=false;
    assert(decide(c,{a}).kind==PlanKind::OPEN_LONG);
    a.q_open_long=-1; a.q_open_short=-1;
    assert(decide(c,{a}).kind==PlanKind::WAIT);
    std::cout << "F3E-F2C.5 PASS (Top-5 bridge, shared-model short gate, policy, WAIT, no switch)\n";
}
