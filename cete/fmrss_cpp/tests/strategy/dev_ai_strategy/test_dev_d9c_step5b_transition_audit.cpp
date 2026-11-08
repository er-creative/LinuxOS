#include "devai/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;
namespace {
void require(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
void near(double a,double b,const std::string& msg){require(std::abs(a-b)<1e-9,msg+" expected="+std::to_string(b)+" actual="+std::to_string(a));}
std::string read(const std::string&path){std::ifstream f(path);if(!f)throw std::runtime_error("Cannot read "+path);std::ostringstream s;s<<f.rdbuf();return s.str();}
std::map<std::string,std::vector<double>> qtable(const std::string&path){std::ifstream f(path);std::string magic;std::size_t n=0;require(bool(f>>magic>>n)&&magic=="CETE_D9B_Q_V1","Bad Q file");std::map<std::string,std::vector<double>> t;for(std::size_t i=0;i<n;++i){std::string k;std::vector<double> v(8);require(bool(f>>k),"Missing Q key");for(auto&x:v)require(bool(f>>x),"Missing Q value");t[k]=v;}return t;}
DevD9BObservation obs(std::int64_t key,double price,double forecast){DevD9BObservation o;o.symbol="AUDIT";o.decision_key=key;o.close=price;o.predicted_forward_return=forecast;o.forecast_ready=true;return o;}
}
int main(int argc,char**argv){
 try{
  if(argc!=5)throw std::runtime_error("Usage: test_dev_d9c_step5b_transition_audit RUNNER_CPP D8_MODEL_CPP D8_HEADER_HPP D8_CALLER_CPP");
  const std::string runner=read(argv[1]),model=read(argv[2]),header=read(argv[3]),caller=read(argv[4]);
  require(runner.find("step.reward + (step.terminal ? 0.0 : step.total_cost)")!=std::string::npos,"Runner attribution expression changed: manual review required");
  require(runner.find("p={true,obs,pos,action,-step.total_cost}")!=std::string::npos,"Runner pending action/cost expression changed: manual review required");
  require(runner.find("model.learn(p.obs,p.pos,p.action,prior_reward")!=std::string::npos,"Runner Q-learning call changed: manual review required");
  std::cout<<"STEP5B_RUNNER_SOURCE_CONTRACT PASS expected_pending_attribution_pattern=yes\n";
  require(model.find("key+config_.horizon_minutes")!=std::string::npos,"D8 label horizon expression not found");
  require(model.find("future->second->close/ptr->close-1.0")!=std::string::npos,"D8 forward label expression not found");
  require(header.find("horizon_minutes{5}")!=std::string::npos,"D8 default horizon is not 5: inspect config");
  require(caller.find("tc.horizon_minutes=5")!=std::string::npos,"D8 runner does not explicitly set horizon=5: inspect config");
  require(caller.find("model.fitOffline(observations)")!=std::string::npos,"D8 training call not found");
  std::cout<<"STEP5B_FORECAST_SOURCE_CONTRACT PASS default_horizon=5 runner_horizon=5 label=close_t_plus_5/close_t_minus_1\n";
  DevD9BConfig cfg;cfg.permissions.enable_long=true;cfg.permissions.enable_short=false;cfg.round_trip_cost_bps=20;cfg.q_alpha=1;cfg.q_gamma=0;cfg.epsilon=0;
  DevD9BEnvironment env(cfg);DevD9BSharedQModel q(cfg);
  const auto a=obs(600,100,0.0005),b=obs(601,101,0.0005),c=obs(602,102,0.0005),d=obs(915,102,0.0005);
  const auto pos0=env.position(a.symbol).side;const auto s0=env.step(a,DevD9AAction::OPEN_LONG);
  const auto pos1=env.position(b.symbol).side;const auto legal1=env.legalActions(b);const auto s1=env.step(b,DevD9AAction::HOLD_LONG);
  const double entry_reward=-s0.total_cost+s1.reward+s1.total_cost;
  near(entry_reward,0.009,"Entry reward attribution");
  q.learn(a,pos0,DevD9AAction::OPEN_LONG,entry_reward,b,pos1,legal1,false);
  const auto pos2=env.position(c.symbol).side;const auto legal2=env.legalActions(c);const auto s2=env.step(c,DevD9AAction::CLOSE_LONG);
  const double hold_reward=-s1.total_cost+s2.reward+s2.total_cost;
  near(hold_reward,102.0/101.0-1.0,"Hold reward attribution");
  q.learn(b,pos1,DevD9AAction::HOLD_LONG,hold_reward,c,pos2,legal2,false);
  const auto pos3=env.position(d.symbol).side;const auto legal3=env.legalActions(d);const auto s3=env.step(d,DevD9AAction::HOLD_FLAT);
  const double close_reward=-s2.total_cost+s3.reward;
  near(close_reward,-0.001,"Close reward attribution");
  q.learn(c,pos2,DevD9AAction::CLOSE_LONG,close_reward,d,pos3,legal3,true);
  require(s3.terminal&&s3.position_after==DevD9APosition::FLAT,"Terminal flat");
  const std::string file="/tmp/cete_d9c_step5b_q_audit.txt";q.save(file);const auto tab=qtable(file);
  const int open_idx=static_cast<int>(DevD9AAction::OPEN_LONG),hold_idx=static_cast<int>(DevD9AAction::HOLD_LONG),close_idx=static_cast<int>(DevD9AAction::CLOSE_LONG);
  bool open=false,hold=false,close=false;
  for(const auto&[k,v]:tab){(void)k;if(std::abs(v.at(open_idx)-0.009)<1e-9)open=true;if(std::abs(v.at(hold_idx)-(102.0/101.0-1.0))<1e-9)hold=true;if(std::abs(v.at(close_idx)+0.001)<1e-9)close=true;}
  require(open&&hold&&close,"Q-table values do not match attributed rewards");
  std::cout<<"STEP5B_NONTERMINAL_ATTRIBUTION PASS entry_reward=0.009 hold_reward="<<(102.0/101.0-1.0)<<" close_reward=-0.001\n";
  // Terminal force-exit: prior HOLD_LONG must receive the final price move and exit cost.
  DevD9BConfig terminal_cfg=cfg;terminal_cfg.permissions.new_entry_cutoff_minutes=915; // Synthetic terminal boundary test only.
  DevD9BEnvironment env2(terminal_cfg);DevD9BSharedQModel q2(terminal_cfg);
  const auto x=obs(913,100,0.0005),y=obs(914,101,0.0005),z=obs(915,102,0.0005);
  const auto t0=env2.step(x,DevD9AAction::OPEN_LONG);(void)t0;
  const auto yp=env2.position(y.symbol).side;const auto yl=env2.legalActions(y);const auto t1=env2.step(y,DevD9AAction::HOLD_LONG);
  const auto zp=env2.position(z.symbol).side;const auto zl=env2.legalActions(z);const auto t2=env2.step(z,DevD9AAction::FORCE_EXIT);
  require(t2.terminal&&t2.position_after==DevD9APosition::FLAT,"Force exit terminal flat");
  const double final_reward=-t1.total_cost+t2.reward;near(final_reward,102.0/101.0-1.0-0.001,"Terminal reward attribution");
  q2.learn(y,yp,DevD9AAction::HOLD_LONG,final_reward,z,zp,zl,true);
  std::cout<<"STEP5B_TERMINAL_ATTRIBUTION PASS final_holding_reward="<<final_reward<<" force_exit_cost_included=yes\n";
  std::cout<<"STEP5B_AUDIT_PASS source_contract_and_synthetic_transition_checks=yes full_historical_runner_replay=NOT_TESTED\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"STEP5B_AUDIT_FAIL "<<e.what()<<'\n';return 1;}
}
