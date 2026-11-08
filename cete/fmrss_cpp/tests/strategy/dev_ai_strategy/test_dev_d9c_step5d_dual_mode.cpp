#include "devai/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;
namespace {
DevD9BObservation obs(double forecast, bool ready) {
    DevD9BObservation o;
    o.symbol="RELIANCE";o.decision_key=123456*1440+600;
    o.close=100;o.predicted_forward_return=forecast;o.forecast_ready=ready;
    o.zscore_1m=0.3;o.zscore_5m=-0.1;o.zscore_15m=0.2;
    return o;
}
void mustReject(DevD9BSharedQModel& m,const std::string& file){
    bool rejected=false;
    try{m.load(file);}catch(const std::exception&){rejected=true;}
    assert(rejected);
}
}
int main(){
    DevD9BConfig on;on.enable_predicted_forward_return=true;
    on.forecast_inner_abs=0.0000077050967946938912;
    on.forecast_outer_abs=0.000020939389077852836;
    on.permissions.enable_short=false;
    DevD9BConfig off=on;off.enable_predicted_forward_return=false;
    off.forecast_inner_abs=off.forecast_outer_abs=0;
    DevD9BSharedQModel a(on),b(off);
    const auto pos=DevD9APosition::FLAT;
    const auto neg=obs(-0.00001,true),positive=obs(0.00001,true),missing=obs(0,false);
    assert(a.stateKey(neg,pos)!=a.stateKey(positive,pos));
    assert(b.stateKey(neg,pos)==b.stateKey(positive,pos));
    assert(a.stateKey(missing,pos)!=a.stateKey(positive,pos));
    std::cout<<"STEP5D_FORECAST_SEPARATION PASS signed_forecasts_distinct=yes\n";
    DevD9BEnvironment envA(on),envB(off);
    auto actionsA=envA.legalActions(missing);
    auto actionsB=envB.legalActions(missing);
    assert(actionsA.size()==1&&actionsA[0]==DevD9AAction::HOLD_FLAT);
    assert(actionsB.size()==2&&actionsB[1]==DevD9AAction::OPEN_LONG);
    std::cout<<"STEP5D_DIRECT_MODE_ENTRY PASS forecast_ready_not_required=yes\n";
    const std::string f="/tmp/cete_d9d_step5d_test_model.txt";
    a.save(f);a.load(f);mustReject(b,f);
    DevD9BConfig wrong=on;wrong.forecast_inner_abs*=1.01;
    DevD9BSharedQModel c(wrong);mustReject(c,f);
    {FILE* fp=std::fopen(f.c_str(),"w");assert(fp);std::fputs("CETE_D9B_Q_V1\n0\n",fp);std::fclose(fp);}
    mustReject(a,f);std::remove(f.c_str());
    std::cout<<"STEP5D_MODEL_VERSION PASS incompatible_modes_and_legacy_rejected=yes\n";
    std::cout<<"STEP5D_TESTS_PASS all=3\n";
}
