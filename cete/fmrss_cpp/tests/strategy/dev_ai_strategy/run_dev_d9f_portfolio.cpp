// D9F additive research runner. Includes the established D9C dataset loader and session checks.
#define main d9c_original_main_disabled
#include "run_dev_d9c_shared_rl_training.cpp"
#undef main
#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include "devai/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.hpp"
#include <array>
#include <optional>
#include <unordered_map>
#include <iomanip>
#include <limits>

namespace {
using devai::strategy::brokerage::BrokerageChargesEngine;
using devai::strategy::brokerage::ExecutionSide;
using devai::strategy::d9f::Action;
using devai::strategy::d9f::State;
using devai::strategy::d9f::SharedManagerQ;
constexpr double CAPITAL=7000.0;
constexpr int MIN_HOLD=5, COOLDOWN=5, MAX_ENTRIES_SYMBOL=2, MAX_ENTRIES_TOTAL=3;
constexpr double SWITCH_MARGIN=0.75; // heuristic score units, NOT calibrated probability
struct Position {std::string symbol;int quantity;double fill;Key entry;};
struct Metrics {
 std::size_t sessions=0,entries=0,exits=0,switches=0,holds=0,waits=0,forced=0,updates=0,blocked=0,unaffordable=0;
 double gross=0,fees=0,slippage=0,net=0,equity_sum=0,min_equity=CAPITAL;
 std::size_t exploratory=0, greedy=0, entry_days_at_limit=0, days_with_entry=0;
 std::size_t eligible_open=0, eligible_switch=0, candidate_positive=0, candidate_nonpositive=0;
 std::size_t chosen_wait=0,chosen_open=0,chosen_hold=0,chosen_exit=0,chosen_switch=0;
 double sum_candidate_score=0, sum_open_score=0, sum_open_q_advantage=0;
 std::size_t open_score_count=0, open_q_count=0;
 std::size_t entry_score_rejected=0, switch_score_rejected=0, eligible_score_open=0;
 double reward_sum=0, reward_abs_sum=0, reward_min=0, reward_max=0;
 std::size_t reward_count=0, reward_nonzero=0;
};
void ensure(bool ok,const char* msg){if(!ok)throw std::runtime_error(std::string("D9F ")+msg);}
double score(const Row& r,bool forecast){
 const auto& o=r.obs;
 return .5*o.zscore_1m+.3*o.zscore_5m+.2*o.zscore_15m+
   (forecast&&o.forecast_ready?100.0*o.predicted_forward_return:0.0);
}
int affordable(double cash,double close,double slip,const BrokerageChargesEngine& b,bool training){
 const double fill=close*(1+(training?0.0:slip)/10000.0);
 if(!(cash>0&&fill>0))return 0;
 int n=static_cast<int>(std::floor(cash/fill));
 while(n>0){const double t=n*fill;const double fee=training?0.0:b.calculate(ExecutionSide::BUY,t).total;
  if(t+fee<=cash+1e-8)break;
  --n;}
 return n;
}
void recordReward(Metrics& m,double r){
 ensure(std::isfinite(r),"nonfinite learning reward");
 m.reward_sum+=r;m.reward_abs_sum+=std::abs(r);
 if(m.reward_count==0){m.reward_min=r;m.reward_max=r;}
 else{m.reward_min=std::min(m.reward_min,r);m.reward_max=std::max(m.reward_max,r);}
 ++m.reward_count;if(r!=0.0)++m.reward_nonzero;
}
struct Decision {
 State state;
 std::vector<Action> legal;
 const Row* candidate=nullptr;
};
Decision decide(const std::map<std::string,const Row*>& at,const std::optional<Position>& held,
 const std::unordered_map<std::string,int>& per_symbol,const std::unordered_map<std::string,Key>& last_exit,
 Key key,int entries_total,bool forecast,double cash,const BrokerageChargesEngine& b,double slip,bool training,double score_threshold,Metrics& m){
 Decision d;
 d.state.forecast_mode=forecast;
 d.state.invested=held.has_value();
 d.state.minutes_to_close=std::max(0,915-static_cast<int>(key%1440));
 d.state.entries_remaining=std::max(0,MAX_ENTRIES_TOTAL-entries_total);
 if(held){d.state.position_age=static_cast<int>(key-held->entry);d.state.held_score=score(*at.at(held->symbol),forecast);}
 for(const auto& [sym,r]:at){
  if(!r->selected || (held&&sym==held->symbol))continue;
  const auto e=per_symbol.find(sym);
  if(e!=per_symbol.end()&&e->second>=MAX_ENTRIES_SYMBOL)continue;
  const auto x=last_exit.find(sym);
  if(x!=last_exit.end()&&key-x->second<COOLDOWN)continue;
  if(affordable(cash+(held?held->quantity*at.at(held->symbol)->obs.close:0.0),r->obs.close,slip,b,training)==0)continue;
  if(!d.candidate || score(*r,forecast)>score(*d.candidate,forecast))d.candidate=r;
 }
 d.state.candidate_available=d.candidate!=nullptr;
 d.state.candidate_score=d.candidate?score(*d.candidate,forecast):0;
 d.state.advantage=held?(d.state.candidate_score-d.state.held_score):d.state.candidate_score;
 const bool budget_ok=entries_total<MAX_ENTRIES_TOTAL;
 const bool time_ok=key%1440<900;
 const auto gate=devai::strategy::d9f2::entryEligibility(
  d.candidate!=nullptr,d.state.candidate_score,score_threshold,time_ok,budget_ok);
 if(d.candidate&&budget_ok&&time_ok&&!gate.score_pass)++m.entry_score_rejected;
 const bool can_enter=gate.accepted;
 if(can_enter)++m.eligible_score_open;
 if(!held){d.legal={Action::WAIT};if(can_enter)d.legal.push_back(Action::OPEN);}
 else {
  d.legal={Action::HOLD};
  if(key-held->entry>=MIN_HOLD){
   d.legal.push_back(Action::EXIT);
   const auto sw=devai::strategy::d9f2::switchEligibility(
    d.candidate!=nullptr,d.state.candidate_score,score_threshold,d.state.advantage,SWITCH_MARGIN,time_ok,budget_ok);
   if(d.candidate&&time_ok&&budget_ok&&!sw.accepted)++m.switch_score_rejected;
   if(sw.accepted)d.legal.push_back(Action::SWITCH);
  }
 }
 return d;
}
struct D9FPending {bool valid=false;State state;Action action=Action::WAIT;double equity_after=CAPITAL;};
Metrics replay(const Sessions& days,Key begin,Key end,SharedManagerQ& model,bool training,bool forecast,
 double slip,const BrokerageChargesEngine& broker,double score_threshold){
 Metrics m;
 for(const auto& [day,rows]:days){
  if(day*1440<begin||day*1440>=end)continue;
  double cash=CAPITAL;
  std::optional<Position> held;
  std::unordered_map<std::string,int> per_symbol;
  std::unordered_map<std::string,Key> last_exit;
  int entries_total=0;
  D9FPending pending;
  Key prev=0;
  double session_fees=0,session_slippage=0;
  auto sell=[&](const std::map<std::string,const Row*>& at,Key key,bool forced){
   ensure(held.has_value(),"sell without holding");
   const double close=at.at(held->symbol)->obs.close;
   const double fill=close*(1-(training?0.0:slip)/10000.0);
   const double turnover=held->quantity*fill;
   const double fee=training?0.0:broker.calculate(ExecutionSide::SELL,turnover).total;
   cash+=turnover-fee;
   m.fees+=fee;session_fees+=fee;
   const double sl=held->quantity*(close-fill);m.slippage+=sl;session_slippage+=sl;
   last_exit[held->symbol]=key;held.reset();++m.exits;if(forced)++m.forced;
  };
  auto buy=[&](const Row& r,Key key)->bool{
   const int n=affordable(cash,r.obs.close,slip,broker,training);
   if(n<=0){++m.unaffordable;return false;}
   const double fill=r.obs.close*(1+(training?0.0:slip)/10000.0);
   const double t=n*fill,fee=training?0.0:broker.calculate(ExecutionSide::BUY,t).total;
   ensure(t+fee<=cash+1e-8,"buy exceeds available cash");
   cash-=t+fee;m.fees+=fee;session_fees+=fee;
   const double sl=n*(fill-r.obs.close);m.slippage+=sl;session_slippage+=sl;
   held=Position{r.obs.symbol,n,fill,key};++per_symbol[r.obs.symbol];++entries_total;++m.entries;
   return true;
  };
  for(std::size_t i=0;i<rows.size();){
   const Key key=rows[i].obs.decision_key;
   std::map<std::string,const Row*> at;
   std::size_t j=i;
   for(;j<rows.size()&&rows[j].obs.decision_key==key;++j)at.emplace(rows[j].obs.symbol,&rows[j]);
   ensure(at.size()==10,"expected ten symbols per minute");
   ensure(!prev||key==prev+1,"minute gap");prev=key;
   const bool terminal=key%1440>=915;
   // Portfolio equity BEFORE this minute's action. Reflects mark-to-market since prior decision.
   const double pre_equity=cash+(held?held->quantity*at.at(held->symbol)->obs.close:0.0);
   ensure(std::isfinite(pre_equity),"nonfinite equity");
   if(terminal){
    if(held)sell(at,key,true); // include final close, sell fee in terminal reward
    if(training&&pending.valid){
     const double reward=(cash-pending.equity_after)/CAPITAL;
     recordReward(m,reward);
     model.learn(pending.state,pending.action,reward,State{}, {},true);
     ++m.updates;
    }
    pending.valid=false;
    ensure(!held,"overnight exposure");
    i=j;continue;
   }
   Decision d=decide(at,held,per_symbol,last_exit,key,entries_total,forecast,cash,broker,slip,training,score_threshold,m);
   if(training&&pending.valid){
    const double reward=(pre_equity-pending.equity_after)/CAPITAL;
    recordReward(m,reward);
    model.learn(pending.state,pending.action,reward,d.state,d.legal,false);
    ++m.updates;
   }
   // Diagnostic only: peek at greedy action BEFORE exploration, without modifying model/RNG.
   Action greedy=d.legal.front();
   double greedy_q=model.qValue(d.state,greedy);
   for(Action a:d.legal){
    const double q=model.qValue(d.state,a);
    if(q>greedy_q){greedy=a;greedy_q=q;}
   }
   if(std::find(d.legal.begin(),d.legal.end(),Action::OPEN)!=d.legal.end())++m.eligible_open;
   if(std::find(d.legal.begin(),d.legal.end(),Action::SWITCH)!=d.legal.end())++m.eligible_switch;
   if(d.candidate){
    m.sum_candidate_score+=d.state.candidate_score;
    if(d.state.candidate_score>0)++m.candidate_positive;
    else ++m.candidate_nonpositive;
   }
   const Action selected=model.choose(d.state,d.legal,training);
   if(selected==greedy)++m.greedy;else ++m.exploratory;
   switch(selected){
    case Action::WAIT:++m.chosen_wait;break;
    case Action::OPEN:++m.chosen_open;break;
    case Action::HOLD:++m.chosen_hold;break;
    case Action::EXIT:++m.chosen_exit;break;
    case Action::SWITCH:++m.chosen_switch;break;
   }
   if(selected==Action::OPEN || selected==Action::SWITCH){
    m.sum_open_score+=d.state.candidate_score;++m.open_score_count;
    const Action alternative=(selected==Action::OPEN?Action::WAIT:Action::HOLD);
    m.sum_open_q_advantage+=model.qValue(d.state,selected)-model.qValue(d.state,alternative);
    ++m.open_q_count;
   }
   Action executed=selected;
   switch(selected){
    case Action::WAIT:++m.waits;break;
    case Action::HOLD:++m.holds;break;
    case Action::OPEN:
     ensure(!held&&d.candidate,"invalid open");
     if(!buy(*d.candidate,key)){executed=Action::WAIT;++m.blocked;}
     break;
    case Action::EXIT:
     ensure(held&&key-held->entry>=MIN_HOLD,"invalid exit");
     sell(at,key,false);break;
    case Action::SWITCH:
     ensure(held&&d.candidate&&key-held->entry>=MIN_HOLD,"invalid switch");
     // Atomic same-close research assumption; account for BOTH exit and entry fees.
     sell(at,key,false);
     if(buy(*d.candidate,key))++m.switches;
     else {executed=Action::EXIT;++m.blocked;}
     break;
   }
   ensure(cash>=-1e-6,"negative cash");
   const double after=cash+(held?held->quantity*at.at(held->symbol)->obs.close:0.0);
   m.min_equity=std::min(m.min_equity,after);
   pending={true,d.state,executed,after};
   i=j;
  }
  ensure(!held,"unclosed session position");
  const double pnl=cash-CAPITAL;
  m.net+=pnl;
  m.gross+=pnl+session_fees+session_slippage;
  m.equity_sum+=cash;
  ++m.sessions;
  if(entries_total>0)++m.days_with_entry;
  if(entries_total==MAX_ENTRIES_TOTAL)++m.entry_days_at_limit;
 }
 return m;
}
void print(const char* mode,int trial,const char* phase,const Metrics& m,const SharedManagerQ& model){
 std::cout<<"D9F_RESULT mode="<<mode<<" trial="<<trial<<" phase="<<phase
  <<" sessions="<<m.sessions<<" entries="<<m.entries<<" exits="<<m.exits
  <<" switches="<<m.switches<<" holds="<<m.holds<<" waits="<<m.waits
  <<" forced="<<m.forced<<" updates="<<m.updates<<" states="<<model.states()
  <<" gross_pnl_rupees="<<m.gross<<" fees_rupees="<<m.fees
  <<" slippage_rupees="<<m.slippage<<" net_pnl_rupees="<<m.net
  <<" avg_final_equity_rupees="<<(m.sessions?m.equity_sum/m.sessions:0)
  <<" min_equity_rupees="<<m.min_equity<<" blocked="<<m.blocked
  <<" unaffordable="<<m.unaffordable<<'\n';
 std::cout<<"D9F1_AUDIT mode="<<mode<<" trial="<<trial<<" phase="<<phase
  <<" exploratory_actions="<<m.exploratory<<" greedy_actions="<<m.greedy
  <<" eligible_open_minutes="<<m.eligible_open
  <<" eligible_switch_minutes="<<m.eligible_switch
  <<" days_with_entries="<<m.days_with_entry
  <<" days_at_entry_limit="<<m.entry_days_at_limit
  <<" chosen_wait="<<m.chosen_wait<<" chosen_open="<<m.chosen_open
  <<" chosen_hold="<<m.chosen_hold<<" chosen_exit="<<m.chosen_exit
  <<" chosen_switch="<<m.chosen_switch
  <<" candidate_positive="<<m.candidate_positive
  <<" candidate_nonpositive="<<m.candidate_nonpositive
  <<" avg_open_score="<<(m.open_score_count?m.sum_open_score/m.open_score_count:0.0)
  <<" avg_open_q_minus_wait_or_hold="<<(m.open_q_count?m.sum_open_q_advantage/m.open_q_count:0.0)
  <<" note=d9f2_entry_gate_enabled"<<'\n';
 std::cout<<"D9F2_AUDIT mode="<<mode<<" trial="<<trial<<" phase="<<phase
  <<" entry_score_rejected="<<m.entry_score_rejected
  <<" switch_rejected="<<m.switch_score_rejected
  <<" eligible_score_open="<<m.eligible_score_open
  <<" reward_updates="<<m.reward_count
  <<" reward_nonzero="<<m.reward_nonzero
  <<" reward_sum="<<m.reward_sum
  <<" reward_mean="<<(m.reward_count?m.reward_sum/m.reward_count:0)
  <<" reward_abs_mean="<<(m.reward_count?m.reward_abs_sum/m.reward_count:0)
  <<" reward_min="<<m.reward_min<<" reward_max="<<m.reward_max
  <<" reward_units=portfolio_equity_delta_divided_by_7000"<<'\n';
}
}
int main(int argc,char**argv){
 try{
  if(argc!=7)throw std::invalid_argument("Usage: runner causal.csv TRAIN_START VAL_START OOS_START forecast_on|forecast_off slippage_bps_per_side");
  const Key train=dateKey(argv[2]),val=dateKey(argv[3]),oos=dateKey(argv[4]);
  ensure(train<val&&val<oos,"invalid split");
  const std::string mode=argv[5];ensure(mode=="forecast_on"||mode=="forecast_off","invalid mode");
  const bool forecast=mode=="forecast_on";
  const double slip=std::stod(argv[6]);ensure(std::isfinite(slip)&&slip>=0&&slip<=100,"invalid slippage");
  Diagnostics diagnostics;auto data=load(argv[1]);auto accepted=sessions(data,diagnostics,train,val,oos);
  printDiagnostics(diagnostics);
  ensure(diagnostics.accepted_train>0&&diagnostics.accepted_val>0,"empty train/validation");
  const BrokerageChargesEngine broker;
  std::cout<<std::fixed<<std::setprecision(6)
   <<"D9F_CONFIG manager=DevManager_001 total_capital=7000 max_positions=1 max_daily_entries=3"
   <<" max_entries_per_symbol=2 min_hold=5 cooldown=5 switch_margin=0.75"
   <<" train_cost=zero validation_cost=brokerage_engine forecast="<<mode
   <<" slippage_bps_per_side="<<slip
   <<" reward=next_minute_portfolio_equity_delta model_saved=no model_approved=no oos_evaluated=no\n";
  struct Trial{double alpha,gamma,epsilon;};
  const std::array<Trial,6> grid{{{.02,.90,.05},{.02,.95,.10},{.05,.90,.05},{.05,.95,.10},{.10,.90,.05},{.10,.95,.10}}};
  // Fit the eligibility threshold ONLY from the training partition, using
  // candidate scores observable at each decision minute. No labels, future
  // candles, validation or OOS data participate in threshold calibration.
  std::vector<double> calibration_scores;
  for(const auto& [day,rows]:accepted){
   if(day*1440<train || day*1440>=val)continue;
   for(const auto& r:rows){
    if(r.selected && r.obs.decision_key%1440<900)
     calibration_scores.push_back(score(r,forecast));
   }
  }
  const double score_threshold=devai::strategy::d9f2::trainingScoreThreshold(
   std::move(calibration_scores),{0.80,0.0});
  std::cout<<"D9F2_CONFIG mode="<<mode<<" threshold="<<score_threshold
   <<" calibration=train_only selected_top5_rows percentile=0.80"
   <<" train_cost=zero validation_cost=brokerage_engine"
   <<" oos_evaluated=no"<<'\n';
  int i=0;
  for(const auto& t:grid){
   SharedManagerQ model({t.alpha,t.gamma,t.epsilon,17});
   auto tr=replay(accepted,train,val,model,true,forecast,0,broker,score_threshold);
   auto vr=replay(accepted,val,oos,model,false,forecast,slip,broker,score_threshold);
   print(mode.c_str(),++i,"TRAIN_GROSS",tr,model);
   print(mode.c_str(),i,"VALIDATION_NET",vr,model);
  }
  std::cout<<"D9F2_COMPLETE model_saved=no model_approved=no oos_evaluated=no\n";
 }catch(const std::exception& e){std::cerr<<"D9F_FAIL "<<e.what()<<'\n';return 1;}
 return 0;
}
