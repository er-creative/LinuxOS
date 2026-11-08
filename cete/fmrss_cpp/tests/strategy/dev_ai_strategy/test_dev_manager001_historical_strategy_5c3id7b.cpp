#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;

static DevHistoricalStockFrame stock(const std::string& symbol, std::int64_t key,
                                      double z, double past_return) {
    DevHistoricalStockFrame s{};
    s.symbol=symbol;
    s.one_minute.completed_key=key;
    s.five_minute.completed_key=key;
    s.fifteen_minute.completed_key=key;
    s.five_minute.open=100.0;
    s.five_minute.close=100.0*(1.0+past_return);
    s.close_1m={20,100.0,1.0,z,true};
    s.close_5m={20,100.0,1.0,z,true};
    s.close_15m={20,100.0,1.0,z,true};
    return s;
}
int main() {
    DevD7StrategyConfig config;
    config.min_expected_return=0.0;
    DevManager001StatisticalStrategyD7 strategy(config, diagnosticPastOnlyForecast);
    DevManager001HistoricalRanking manager({"DevManager_001",1},strategy.rankingPolicy());
    DevHistoricalManagerFrame f{"DevManager_001", 1005,
        {stock("RELIANCE",1005,-1,0.002),stock("SBIN",1005,1,0.001)}};
    const auto r=manager.evaluate(f);
    assert(r.evaluated==2 && r.selected.size()==1);
    assert(r.selected[0].symbol=="RELIANCE");
    assert(std::abs(r.selected[0].ranking_score-0.002)<1e-10);
    f.stocks[0].five_minute.completed_key=1010; // future candle must fail
    bool rejected=false;
    try { (void)manager.evaluate(f); }
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    f.stocks[0].five_minute.completed_key=1005;
    DevManager001StatisticalStrategyD7 unavailable(config,
        [](const DevHistoricalStockFrame&,const DevHistoricalManagerFrame&)->DevD7Forecast{
            return {};
        });
    DevManager001HistoricalRanking no_forecast({"DevManager_001",1},
        unavailable.rankingPolicy());
    assert(no_forecast.evaluate(f).selected.empty());
    bool bad_interval=false;
    try { DevManager001HistoricalStrategyD7B bad({ "DevManager_001", 1, 7 }, strategy); }
    catch(const std::invalid_argument&) {bad_interval=true;}
    assert(bad_interval);
    std::cout<<"PASS 5C-3I-D7B causal historical strategy contract (diagnostic only)\n";
}
