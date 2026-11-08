#include "devai/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static bool has(const std::vector<DevD9AAction>& a,DevD9AAction v) {
    return std::find(a.begin(),a.end(),v)!=a.end();
}
int main(){
    // The minute key is day*1440 + local minute; use an aligned synthetic day.
    const auto at=[](const char* s,int minute,double p,bool sh=true) {
        return DevD9BObservation{s,740000*1440+minute,p,0.004,0.5,0.4,0.3,true,sh};
    };
    DevD9BConfig c; DevD9BEnvironment env(c);
    auto a=at("RELIANCE",570,100);
    assert(has(env.legalActions(a),DevD9AAction::OPEN_LONG));
    assert(!has(env.legalActions(a),DevD9AAction::OPEN_SHORT));
    bool rejected=false;
    try { (void)env.step(a,DevD9AAction::OPEN_SHORT); } catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    auto x=env.step(a,DevD9AAction::OPEN_LONG);
    assert(x.position_after==DevD9APosition::LONG);
    assert(std::abs(x.reward+0.001)<1e-12);
    auto b=at("RELIANCE",571,101);
    x=env.step(b,DevD9AAction::HOLD_LONG);
    assert(std::abs(x.reward-0.01)<1e-12);
    auto e=at("RELIANCE",915,102);
    assert(env.legalActions(e).size()==1 && env.legalActions(e).front()==DevD9AAction::FORCE_EXIT);
    // Missing minute bars while holding are rejected (not silently bridged).
    rejected=false;
    try { (void)env.step(e,DevD9AAction::FORCE_EXIT); } catch(const std::runtime_error&){rejected=true;}
    assert(rejected);
    env.reset();
    (void)env.step(at("SBIN",914,200),DevD9AAction::OPEN_LONG);
    x=env.step(at("SBIN",915,201),DevD9AAction::FORCE_EXIT);
    assert(x.terminal && x.position_after==DevD9APosition::FLAT);
    assert(std::abs(x.reward-(0.005-0.001))<1e-12);
    assert(env.legalActions(at("SBIN",916,201)).front()==DevD9AAction::HOLD_FLAT);
    rejected=false;
    try {(void)env.step(at("SBIN",916,201),DevD9AAction::OPEN_LONG);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    DevD9BConfig both; both.permissions.enable_short=true;
    DevD9BEnvironment env2(both);
    assert(has(env2.legalActions(at("ITC",600,100)),DevD9AAction::OPEN_SHORT));
    assert(!has(env2.legalActions(at("ITC",600,100,false)),DevD9AAction::OPEN_SHORT));
    (void)env2.step(at("ITC",600,100),DevD9AAction::OPEN_SHORT);
    x=env2.step(at("ITC",601,99),DevD9AAction::CLOSE_SHORT);
    assert(x.position_after==DevD9APosition::FLAT && x.reward>0);
    DevD9BSharedQModel model(c);
    auto legal=env.legalActions(at("NTPC",600,100));
    const auto selected=model.choose(at("NTPC",600,100),DevD9APosition::FLAT,legal,false);
    assert(has(legal,selected));
    model.learn(at("NTPC",600,100),DevD9APosition::FLAT,DevD9AAction::OPEN_LONG,
        0.01,at("NTPC",601,101),DevD9APosition::LONG,
        {DevD9AAction::HOLD_LONG,DevD9AAction::CLOSE_LONG},false);
    assert(model.stateCount()==1);
    model.save("/tmp/cete_d9b_unit_model.txt");
    DevD9BSharedQModel restored(c); restored.load("/tmp/cete_d9b_unit_model.txt");
    assert(restored.stateCount()==1);
    std::cout<<"PASS 5C-3I-D9B position-aware shared RL environment and mandatory exit tests\n";
}
