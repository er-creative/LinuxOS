#include "devai/strategy/dev_ai_strategy/diagnostics/DevRankingSourceAudit_D9F4CF.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <string>
using namespace devai::strategy::diagnostics::d9f4cf;
int main(){
    const std::string path="d9f4cf_test_universe.csv";
    {std::ofstream out(path);out<<"decision_time,symbol,mean_return,stddev_return,forecast_return,forecast_enabled,score,last_input_time,forecast_last_input_time\n";
        for(int t=100;t<=200;t+=100)for(int i=0;i<10;++i)out<<t<<",S"<<i<<",0.01,0.02,0,0,"<<(10-i+(t==200&&i==9?20:0))<<","<<t<<",0\n";
    }
    auto good=inspect(path);assert(good.integrityPass());assert(good.rows==20);assert(good.decisions==2);assert(good.unique_symbols==10);assert(good.static_score_decisions==0);assert(good.static_top5_decisions==0);
    {std::ofstream out(path);out<<"decision_time,symbol,mean_return,stddev_return,forecast_return,forecast_enabled,score,last_input_time,forecast_last_input_time\n";
        for(int i=0;i<10;++i)out<<100<<",S"<<i<<",0.01,0.02,0.03,1,39,"<<(i==0?101:100)<<","<<(i==1?101:100)<<"\n";
    }
    auto bad=inspect(path);assert(!bad.integrityPass());assert(bad.future_inputs==1);assert(bad.future_forecasts==1);assert(bad.score_ties==1);
    std::remove(path.c_str());
    std::cout<<"D9F4CF_RANKING_SOURCE_TEST_PASS universe=1 score_change=1 top5_change=1 future_input=1 future_forecast=1 ties=1\n";
}
