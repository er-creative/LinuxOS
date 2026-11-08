#include "devai/strategy/dev_ai_strategy/DevDirectionTrainingPolicy_F3EF2C5A.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy;
using namespace devai::strategy::f3ef2c5a;
int main() {
    TrainingPolicy p{}; p.risk_approved=true; p.entries_allowed=true;
    auto m=agentMask(p,DEV_POSITION_FLAT);
    assert(m[0] && m[1] && !m[2] && !m[3]);
    p.allow_short=true; p.shortable=true;
    m=agentMask(p,DEV_POSITION_FLAT);
    assert(m[0] && m[1] && m[2] && !m[3]);
    m=agentMask(p,DEV_POSITION_LONG);
    assert(m[0] && !m[1] && !m[2] && m[3]);
    m=agentMask(p,DEV_POSITION_SHORT);
    assert(m[0] && !m[1] && !m[2] && m[3]);
    p.force_flat=true;
    m=agentMask(p,DEV_POSITION_SHORT);
    assert(!m[0] && !m[1] && !m[2] && m[3]);
    m=agentMask(p,DEV_POSITION_FLAT);
    assert(m[0] && !m[1] && !m[2] && !m[3]);
    p.force_flat=false;p.shortable=false;
    m=agentMask(p,DEV_POSITION_FLAT); assert(!m[2]);
    std::cout << "F3E-F2C.5A PASS (direction training masks, no atomic reversal, forced exit)\n";
}
