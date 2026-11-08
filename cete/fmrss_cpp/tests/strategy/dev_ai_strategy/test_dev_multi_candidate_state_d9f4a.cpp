#include "devai/strategy/dev_ai_strategy/DevMultiCandidateState_D9F4A.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace devai::strategy::d9f4a;
namespace {
template<class F> void mustThrow(F&& f){bool thrown=false;try{f();}catch(const std::invalid_argument&){thrown=true;}assert(thrown);}
}
int main(){
    PortfolioInput p; p.decision_key=123456; p.minutes_to_close=120; p.entries_remaining=3;p.forecast_mode=true;
    std::vector<CandidateInput> v={
        {"AAA",123456,1,1.0,0.8,0.6,true,.0010,true},
        {"BBB",123456,2,1.0,0.8,0.6,true,-.0010,true},
        {"CCC",123456,3,.4,.3,.2,false,0.0,false},
        {"DDD",123456,4,.2,.1,.1,true,.0020,true},
        {"EEE",123456,5,-.5,-.4,-.3,true,-.0020,true}
    };
    auto states=DevMultiCandidateState_D9F4A::build(p,v);
    assert(states.size()==5 && states[0].symbol=="AAA" && states[4].symbol=="EEE");
    assert(std::abs(states[0].statistical_score-.86)<1e-12);
    assert(std::abs(states[0].forecast_component-.1)<1e-12);
    assert(std::abs(states[1].forecast_component+.1)<1e-12);
    assert(states[0].state_key!=states[1].state_key);
    assert(states[2].forecast_component==0.0 && !states[2].eligible);
    auto off=p;off.forecast_mode=false;
    auto without=DevMultiCandidateState_D9F4A::build(off,v);
    assert(without[0].forecast_component==0 && without[0].combined_score==without[1].combined_score);
    assert(without[0].state_key!=states[0].state_key);
    // Identity is audit metadata, not a stock-specific learned parameter.
    auto renamed=v;renamed[0].symbol="ZZZ";
    assert(DevMultiCandidateState_D9F4A::build(p,renamed)[0].state_key==states[0].state_key);
    auto stale=v;stale[0].decision_key--;
    mustThrow([&]{(void)DevMultiCandidateState_D9F4A::build(p,stale);});
    auto dup=v;dup[1].rank=1;
    mustThrow([&]{(void)DevMultiCandidateState_D9F4A::build(p,dup);});
    auto bad=v;bad[2].predicted_forward_return=.001;
    mustThrow([&]{(void)DevMultiCandidateState_D9F4A::build(p,bad);});
    auto held=p;held.invested=true;held.held_symbol="AAA";
    mustThrow([&]{(void)DevMultiCandidateState_D9F4A::build(held,v);});
    std::cout<<"D9F4A_MULTI_CANDIDATE_PASS candidates=5 distinct_forecast_states=1 causal_timestamp_guard=1"
             <<" rank_guard=1 shared_model_symbol_independent=1 forecast_off_guard=1\n";
    std::cout<<"D9F4A_CONTRACT_PASS production_policy_modified=0 model_trained=0 oos_evaluated=0\n";
}
