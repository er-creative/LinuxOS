#include "devai/strategy/dev_ai_strategy/DevCausalForwardReturnD8.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace devai::strategy;
int main(){
    constexpr std::int64_t split=3000;
    DevD8TrainingConfig c; c.oos_start_key=split;c.horizon_minutes=2;c.minimum_training_labels=15;
    std::vector<DevD8Observation> rows;
    for(std::int64_t k=1510;k<1590;++k){
        // Synthetic close sequence; labels strictly offline.
        rows.push_back({"TEST",k,100.0+0.01*(k-1510),{1.0,0.1,0.2,-0.1}});
    }
    DevCausalForwardReturnD8 model(c);
    const auto r=model.fitOffline(rows);
    assert(r.training_labels>15 && model.fitted());
    DevHistoricalManagerFrame f;f.decision_key=1575;
    DevHistoricalStockFrame s;s.symbol="TEST";s.one_minute.completed_key=1575;
    s.five_minute.completed_key=1575;s.fifteen_minute.completed_key=1575;
    s.one_minute.close=101;s.close_1m.ready=s.close_5m.ready=s.close_15m.ready=true;
    s.close_1m.zscore=0.1;s.close_5m.zscore=0.2;s.close_15m.zscore=-0.1;
    auto p=model.predict(s,f);assert(p.ready&&p.as_of_key==f.decision_key&&std::isfinite(p.expected_forward_return));
    s.five_minute.completed_key=1580;
    bool caught=false;try{(void)model.predict(s,f);}catch(const std::invalid_argument&){caught=true;}
    assert(caught); // future 5m candle must be rejected
    // Pre-OOS samples with labels crossing the split are never trained.
    DevD8TrainingConfig c2=c;c2.oos_start_key=1550;c2.minimum_training_labels=15;
    DevCausalForwardReturnD8 m2(c2);auto r2=m2.fitOffline(rows);
    assert(r2.excluded_crossing_split==2);
    std::cout<<"PASS 5C-3I-D8 causal training/forecast contract\n";
}
