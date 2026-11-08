#include "devai/strategy/dev_ai_strategy/DevRLTrainingReadiness.hpp"
#include <cassert>
#include <iostream>
using namespace devai::readiness;
int main() {
    TrainingDecision a{};
    a.decision_time=100; a.next_decision_time=200;
    a.manager_id="DevManager_001";
    a.candidates={{"RELIANCE","DevAgent_001",0,true,true,false}};
    a.manager_action_valid=true; a.supervisor_authorized=true;
    a.initial_equity=100000; a.equity_before=100000; a.equity_after=101000;
    a.manager_reward=.01;
    TrainingDecision b=a;
    b.decision_time=200; b.next_decision_time=300; b.terminal=true;
    b.equity_before=101000; b.equity_after=101500; b.manager_reward=.005;
    auto ok=validate({a,b},301);
    assert(ok.ready);
    b.candidates[0].has_realized_forward_return_in_observation=true;
    assert(!validate({a,b},301).ready);
    b.candidates[0].has_realized_forward_return_in_observation=false;
    b.manager_reward=-.1;
    assert(!validate({a,b},301).ready);
    b.manager_reward=.005;
    b.held_symbol="RELIANCE";
    b.held_position_visible=false;
    assert(!validate({a,b},301).ready);
    b.held_position_visible=true;
    assert(validate({a,b},301).ready);
    std::cout << "PASS 5C-3I-A training readiness contract\n";
}
