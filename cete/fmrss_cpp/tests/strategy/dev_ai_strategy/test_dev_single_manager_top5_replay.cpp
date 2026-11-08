#include "devai/strategy/dev_ai_strategy/DevSingleManagerTop5Replay.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    const std::vector<std::string> symbols={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN"};
    DevUniverseRankingTick t{};t.decision_time=1791000000;
    for(std::size_t i=0;i<symbols.size();++i) {
        DevUniverseRankedShare s{};s.symbol=symbols[i];s.score=static_cast<double>(i);
        s.observation.symbol=s.symbol;s.observation.decision_time=t.decision_time;
        s.observation.ready=true;s.observation.numerical_integrity=true;
        t.universe.push_back(s);
    }
    const auto ticks=DevSingleManagerTop5Replay::prepare({t},symbols);
    assert(ticks.size()==1 && ticks[0].managers.size()==1);
    const auto& m=ticks[0].managers[0];
    assert(m.manager_id=="DevManager_001" && m.candidates.size()==5);
    assert(m.candidates[0].symbol=="SBIN" && m.candidates[4].symbol=="ICICIBANK");
    for(std::size_t i=0;i<5;++i) assert(m.candidates[i].slot==i && m.candidates[i].candidate_rank==i+1);
    bool rejected=false;
    try {auto bad=t;bad.universe.pop_back();(void)DevSingleManagerTop5Replay::prepare({bad},symbols);}
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    rejected=false;
    try {auto bad=t;bad.universe[0].observation.decision_time-=60;(void)DevSingleManagerTop5Replay::prepare({bad},symbols);}
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    std::cout<<"PASS single-manager top5: 10 ranked, five slots, one manager, stale and incomplete rejected\n";
}
