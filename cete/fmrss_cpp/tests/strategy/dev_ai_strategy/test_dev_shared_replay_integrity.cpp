#include "devai/strategy/dev_ai_strategy/DevSharedReplayIntegrity.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy;
int main() {
    const std::vector<std::string> universe={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN"};
    DevUniverseRankingTick tick{};tick.decision_time=1791000000;
    for(std::size_t i=0;i<universe.size();++i) {
        DevUniverseRankedShare s{};s.symbol=universe[i];s.score=double(i);
        s.observation.symbol=s.symbol;s.observation.decision_time=tick.decision_time;
        s.observation.ready=true;s.observation.numerical_integrity=true;
        tick.universe.push_back(s);
    }
    const DevQNetwork agent(DevQNetworkConfig{123U,true});
    const DevManagerQNetwork manager(DevManagerNetworkConfig{123U,true});
    int calls=0;
    const auto fresh=[&](const std::vector<DevReplayTick>& ticks){
        ++calls;
        assert(ticks.size()==1 && ticks[0].managers[0].candidates[0].symbol=="SBIN");
        DevRealReplayResult result{};
        result.input_ticks=ticks.size();
        result.replay.initial_equity=100000;
        result.replay.final_equity=100000;
        result.replay.equity_curve={100000,100000};
        return result;
    };
    auto result=DevSharedReplayIntegrity::verify({tick},universe,agent,manager,fresh);
    assert(result.passed && calls==2 && result.ticks==1);
    auto bad=tick;bad.universe[0].observation.decision_time-=60;
    result=DevSharedReplayIntegrity::verify({bad},universe,agent,manager,fresh);
    assert(!result.passed && calls==2);
    int run=0;
    result=DevSharedReplayIntegrity::verify({tick},universe,agent,manager,[&](const auto& ticks){
        auto r=fresh(ticks);r.replay.final_equity+=run++;return r;
    });
    assert(!result.passed);
    std::cout<<"PASS shared replay integrity: immutable model parameters, deterministic replay, stale rejection, divergence detection\n";
}
