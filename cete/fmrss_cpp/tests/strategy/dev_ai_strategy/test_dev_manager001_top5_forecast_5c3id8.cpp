#include "devai/strategy/dev_ai_strategy/DevManager001Top5ForecastD8.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main() {
    DevD8TrainingConfig cfg;cfg.oos_start_key=3000;cfg.horizon_minutes=5;
    cfg.minimum_training_labels=30;
    DevCausalForwardReturnD8 model(cfg);
    std::vector<DevD8Observation> history;
    for(int i=0;i<10;++i)for(std::int64_t k=1510;k<1600;++k)
        history.push_back({"S"+std::to_string(i),k,100.0+i+0.01*(k-1510),
                           {1.0,0.1*i,0.2,-0.1}});
    model.fitOffline(history);
    DevManager001HistoricalRanking ranker({"DevManager_001",5},devD8StatisticalRankingPolicy);
    DevManager001Top5ForecastD8 engine(ranker,model,5);
    DevHistoricalManagerFrame frame;frame.manager_id="DevManager_001";frame.decision_key=1575;
    for(int i=0;i<10;++i){
        DevHistoricalStockFrame s;s.symbol="S"+std::to_string(i);
        s.one_minute.completed_key=1575;s.five_minute.completed_key=1575;
        s.fifteen_minute.completed_key=1575;s.one_minute.close=100.0+i;
        s.close_1m.ready=s.close_5m.ready=s.close_15m.ready=true;
        s.close_1m.zscore=0.1*i;s.close_5m.zscore=0.2;s.close_15m.zscore=-0.1;
        frame.stocks.push_back(s);
    }
    const auto result=engine.evaluate(frame);
    assert(result.evaluated==10 && result.selected.size()==5);
    assert(result.selected.front().symbol=="S9");
    for(const auto& p:result.selected){
        assert(p.ready && p.reason=="READY" && p.as_of_key==1575 && p.target_key==1580);
        assert(std::abs(p.predicted_future_price-p.current_price*(1+p.predicted_forward_return))<1e-10);
    }
    // Do not extrapolate a five-minute target past the session day.
    frame.decision_key=2878;
    for(auto& s:frame.stocks){s.one_minute.completed_key=2878;s.five_minute.completed_key=2875;
        s.fifteen_minute.completed_key=2865;}
    const auto last=engine.evaluate(frame);
    assert(last.selected.size()==5);
    for(const auto& p:last.selected)assert(!p.ready && p.reason=="TARGET_OUTSIDE_SESSION_DAY");
    std::cout<<"PASS 5C-3I-D8 Top-5 rank-first dual forecast contract\n";
}
