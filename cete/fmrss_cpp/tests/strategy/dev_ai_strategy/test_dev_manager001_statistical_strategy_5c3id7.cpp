#include "devai/strategy/dev_ai_strategy/DevManager001StatisticalStrategyD7.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static DevHistoricalStockFrame stock(const char* name, std::int64_t key, double z) {
    DevHistoricalStockFrame s; s.symbol=name;
    s.one_minute.completed_key=s.five_minute.completed_key=s.fifteen_minute.completed_key=key;
    s.close_1m={20,100,2,z,true};s.close_5m={20,100,2,z,true};s.close_15m={20,100,2,z,true};
    return s;
}
int main(){
    const std::int64_t key=1000;
    DevHistoricalManagerFrame frame;frame.decision_key=key;
    auto provider=[](const DevHistoricalStockFrame& s,const DevHistoricalManagerFrame& f) {
        return DevD7Forecast{s.symbol=="SBIN"?0.02:0.01,f.decision_key,true};
    };
    DevD7StrategyConfig c;c.min_expected_return=0.005;c.min_abs_z15=1.0;
    DevManager001StatisticalStrategyD7 strategy(c,provider);
    DevManager001HistoricalRanking manager({"DevManager_001",1},strategy.rankingPolicy());
    frame.stocks={stock("RELIANCE",key,-1.5),stock("SBIN",key,-2)};
    auto ranked=manager.evaluate(frame);
    assert(ranked.evaluated==2 && ranked.selected.size()==1 && ranked.selected[0].symbol=="SBIN");
    auto weak=stock("WEAK",key,0.2);
    assert(!strategy.assess(weak,frame).ranking.eligible);
    DevManager001StatisticalStrategyD7 unavailable(c,[](const auto&,const auto&) {return DevD7Forecast{};});
    assert(!unavailable.assess(frame.stocks[0],frame).ranking.eligible);
    DevManager001StatisticalStrategyD7 leaked(c,[](const auto&,const auto& f) {
        return DevD7Forecast{0.03,f.decision_key+1,true};});
    bool rejected=false;
    try { (void)leaked.assess(frame.stocks[0],frame); }
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    auto stale=frame.stocks[0];stale.fifteen_minute.completed_key=key-15;
    rejected=false;
    try {(void)strategy.assess(stale,frame);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout<<"PASS 5C-3I-D7 causal statistical strategy adapter contract (synthetic forecast)\n";
}
