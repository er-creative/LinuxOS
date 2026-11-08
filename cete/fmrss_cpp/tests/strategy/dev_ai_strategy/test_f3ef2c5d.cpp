#include "devai/strategy/dev_ai_strategy/DevFrozenDirectionalDecision_F3EF2C5D.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy::f3ef2c5d;
int main() {
    Input x{}; x.manager_action=2;x.agent_action=1;x.candidate="RELIANCE";x.minute_ist=600;
    assert(resolve(x)=="RELIANCE");
    x.agent_action=2;assert(resolve(x).empty());
    x.short_enabled=x.candidate_shortable=x.model_short_verified=true;
    assert(resolve(x)=="SHORT:RELIANCE");
    x.model_short_verified=false;assert(resolve(x).empty());
    x.model_short_verified=true;x.minute_ist=885;assert(resolve(x).empty());
    x.minute_ist=600;x.held="RELIANCE";x.signed_quantity=-3;x.candidate="INFY";
    assert(resolve(x)=="SHORT:RELIANCE");
    x.candidate="RELIANCE";x.agent_action=3;assert(resolve(x).empty());
    x.agent_action=1;assert(resolve(x)=="SHORT:RELIANCE");
    x.minute_ist=915;assert(resolve(x).empty());
    x.minute_ist=600;x.manager_action=0;assert(resolve(x)=="SHORT:RELIANCE");
    std::cout << "F3E-F2C.5D PASS (frozen action mapping, signed holdings, SHORT gates, cutoff)\n";
}
