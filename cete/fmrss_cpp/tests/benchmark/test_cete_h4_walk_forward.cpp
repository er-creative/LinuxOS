#include "devai/benchmark/DevWalkForwardBenchmark.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace devai::benchmark;
void expect(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
 try{
    WFConfig c;c.min_test_days=2;
    std::vector<WFDecision> rows={
        {"Candidate0","LONG","RELIANCE",202610270930LL,100,1,1,true},
        {"Candidate0","LONG","RELIANCE",202610271000LL,105,1,3,true},
        {"Candidate0","LONG","RELIANCE",202610280930LL,110,0,0,true},
        {"Candidate0","SHORT","SBIN",202610270930LL,100,1,1,true},
        {"Candidate0","SHORT","SBIN",202610271000LL,95,1,3,true},
        {"Candidate0","SHORT","SBIN",202610280930LL,100,0,0,true}
    };
    auto reports=evaluateWalkForward(rows,c);
    expect(reports.size()==2,"expected independent long/short reports");
    for(const auto& r:reports){expect(r.trades==1,"expected one closed trade");
        expect(r.net_pnl>0,"expected positive net profit");expect(r.test_days==2,"day count");}
    bool rejected=false;
    auto bad=rows;bad[0].legal=false;
    try{(void)evaluateWalkForward(bad,c);}catch(const std::runtime_error&){rejected=true;}
    expect(rejected,"illegal action accepted");
    rejected=false;bad=rows;std::swap(bad[0],bad[1]);
    try{(void)evaluateWalkForward(bad,c);}catch(const std::runtime_error&){rejected=true;}
    expect(rejected,"out-of-order input accepted");
    rejected=false;bad=rows;bad[1].action=2;
    try{(void)evaluateWalkForward(bad,c);}catch(const std::runtime_error&){rejected=true;}
    expect(rejected,"unclosed position accepted");
    std::cout<<"CETE H4 WALK-FORWARD EVALUATOR TEST PASS\n"
             <<"Separate LONG/SHORT accounting: PASS\n"
             <<"Net P&L and trading costs: PASS\n"
             <<"Chronology, legal actions, and flat-at-end: PASS\n"
             <<"Historical model predictions: NOT SUPPLIED\n"
             <<"True walk-forward retraining: NOT IMPLEMENTED\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"CETE H4 FAIL: "<<e.what()<<'\n';return 1;}
}
