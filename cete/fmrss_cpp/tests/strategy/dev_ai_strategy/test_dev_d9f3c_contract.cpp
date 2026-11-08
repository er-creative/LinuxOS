#include "devai/strategy/dev_ai_strategy/DevCausalForwardReturnD8.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
using devai::strategy::d9f::State;
using devai::strategy::d9f::SharedManagerQ;
using devai::strategy::d9f::Action;
}
int main(){
 try {
  // State contract: inspect actual production stateKey, without modifying learning.
  SharedManagerQ model;
  State a; a.forecast_mode=true;a.candidate_available=true;a.candidate_score=1.01;
  a.advantage=1.01;a.minutes_to_close=120;a.entries_remaining=3;
  State b=a;b.candidate_score=1.90;b.advantage=1.90;
  check(model.stateKey(a)==model.stateKey(b),"expected coarse-bucket collision");
  State c=a;c.forecast_mode=false;
  check(model.stateKey(a)!=model.stateKey(c),"forecast mode missing from key");
  State d=a;d.invested=true;
  check(model.stateKey(a)!=model.stateKey(d),"invested flag missing from key");
  check(model.qValue(a,Action::WAIT)==0.0,"untrained Q must be zero");
  check(model.qValue(a,Action::OPEN)==0.0,"untrained OPEN Q must be zero");
  std::cout<<"D9F3C_STATE_PASS key="<<model.stateKey(a)
           <<" bucket_collision_confirmed=1 symbol_not_in_state=1 forecast_mode_in_key=1\n";

  using namespace devai::strategy;
  constexpr std::int64_t day=100*1440;
  check(devD8SameSession(day+600,day+605),"valid same-session pair rejected");
  check(!devD8SameSession(day+1439,day+1441),"cross-session pair accepted");
  const std::int64_t oos=200*1440;
  DevD8TrainingConfig config;config.oos_start_key=oos;config.horizon_minutes=5;
  config.minimum_training_labels=3;config.ridge_lambda=0.1;
  DevCausalForwardReturnD8 forecast(config);
  std::vector<DevD8Observation> rows;
  // Two independent pre-OOS days; exact 5-minute target available for each row.
  for(int dd: {100,101}){
   for(int t=600;t<=615;++t){
    const auto key=static_cast<std::int64_t>(dd)*1440+t;
    const double close=100.0+0.02*(t-600)+0.1*(dd-100);
    rows.push_back({"RELIANCE",key,close,{1.0,0.1*(t-600),0.2,-0.1}});
   }
  }
  // OOS observations must never contribute training labels.
  for(int t=600;t<=610;++t){
   rows.push_back({"RELIANCE",oos+t,200.0,{1.0,0.0,0.0,0.0}});
  }
  const auto report=forecast.fitOffline(rows);
  check(forecast.fitted(),"forecast fit failed");
  check(report.training_labels==22,"unexpected training label count");
  check(report.excluded_crossing_split==0,"unexpected split crossing");
  check(report.excluded_missing_future==10,"unexpected missing future count");
  for(double beta:forecast.coefficients())check(std::isfinite(beta),"nonfinite coefficient");
  std::cout<<"D9F3C_FORECAST_PASS configured_horizon=5 training_labels="<<report.training_labels
           <<" excluded_missing_future="<<report.excluded_missing_future
           <<" oos_training_rows_used=0 coefficients_finite=1\n";
  std::cout<<"D9F3C_CONTRACT_PASS production_policy_modified=0 oos_evaluated=0\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"D9F3C_CONTRACT_FAIL "<<e.what()<<'\n';return 1;}
}
