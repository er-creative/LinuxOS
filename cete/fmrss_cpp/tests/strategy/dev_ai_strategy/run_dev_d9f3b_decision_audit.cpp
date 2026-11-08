// D9F additive research runner. Includes the established D9C dataset loader and session checks.
#define main d9c_original_main_disabled
#include "run_dev_d9c_shared_rl_training.cpp"
#undef main
#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include "devai/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.hpp"
#include "devai/strategy/dev_ai_strategy/DevRewardAttribution_D9F3.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <array>
#include <optional>
#include <unordered_map>
#include <iomanip>
#include <limits>
#include "devai/strategy/dev_ai_strategy/DevForwardReturnAudit_D9F3A.hpp"

namespace {
using devai::strategy::brokerage::BrokerageChargesEngine;
using devai::strategy::brokerage::ExecutionSide;
using devai::strategy::d9f::Action;
using devai::strategy::d9f::State;
using devai::strategy::d9f::SharedManagerQ;
constexpr double CAPITAL=7000.0;
constexpr int MIN_HOLD=5, COOLDOWN=5, MAX_ENTRIES_SYMBOL=2, MAX_ENTRIES_TOTAL=3;
constexpr double SWITCH_MARGIN=0.75; // heuristic score units, NOT calibrated probability
struct Position {std::string symbol;int quantity;double fill;Key entry;double entry_fee;double entry_close;};
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
 std::size_t eval_reward_count=0,eval_reward_nonzero=0;double eval_reward_sum=0;
 std::size_t trade_rows=0;double ledger_net=0;
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
using devai::strategy::d9f3a::ForwardAudit;
using devai::strategy::d9f3a::ForwardIndex;
struct D9FPending {bool valid=false;State state;Action action=Action::WAIT;double equity_before=CAPITAL;};
Metrics replay(const Sessions& days,Key begin,Key end,SharedManagerQ& model,bool training,bool forecast,
 double slip,const BrokerageChargesEngine& broker,double score_threshold,bool training_costs,
 std::ostream* ledger,std::ostream* forecast_audit,const ForwardIndex& forward_index,const std::string& mode,int trial,
  std::ostream* decisions,std::ostream* candidates){
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
  const bool charge_costs=!training || training_costs;
  auto emit_audit=[&](const std::map<std::string,const Row*>& at,Key key,
                       const std::string& event,const std::string& traded){
   if(!forecast_audit)return;
   for(const auto& [symbol,row]:at){
    const auto result=forward_index.evaluate(day,symbol,key,row->obs.close,
      row->obs.forecast_ready,row->obs.predicted_forward_return);
    *forecast_audit<<mode<<','<<trial<<','<<(training?"TRAIN":"VALIDATION")<<','
      <<day<<','<<key<<','<<event<<','<<traded<<','<<symbol<<','
      <<(symbol==traded?1:0)<<','<<(row->selected?1:0)<<','
      <<(row->obs.forecast_ready?1:0)<<',';
    devai::strategy::d9f3a::writeAuditValues(*forecast_audit,result);
    *forecast_audit<<'\n';
   }
  };
  const std::map<std::string,const Row*>* current_at=nullptr;
  auto sell=[&](const std::map<std::string,const Row*>& at,Key key,bool forced,bool switching=false){
   ensure(held.has_value(),"sell without holding");
   const double close=at.at(held->symbol)->obs.close;
   const std::string closing_symbol=held->symbol;
   emit_audit(at,key,forced?"FORCED_EXIT":(switching?"SWITCH_EXIT":"EXIT"),closing_symbol);
   const auto exit_audit=forward_index.evaluate(day,closing_symbol,key,close,
     at.at(closing_symbol)->obs.forecast_ready,at.at(closing_symbol)->obs.predicted_forward_return);
   const auto entry_row=forward_index.lookup(day,closing_symbol,held->entry);
   ensure(entry_row.has_value(),"missing entry snapshot in audit index");
   const auto entry_forecast=forward_index.evaluate(day,closing_symbol,held->entry,
     held->entry_close,entry_row->ready,entry_row->predicted_return);
   const double fill=close*(1-(charge_costs?slip:0.0)/10000.0);
   const double turnover=held->quantity*fill;
   const double fee=charge_costs?broker.calculate(ExecutionSide::SELL,turnover).total:0.0;
   cash+=turnover-fee;
   m.fees+=fee;session_fees+=fee;
   const double sl=held->quantity*(close-fill);m.slippage+=sl;session_slippage+=sl;
   const double gross=devai::strategy::d9f3::tradeGross(held->quantity,held->fill,fill);
   const double net=devai::strategy::d9f3::tradeNet(gross,held->entry_fee,fee);
   if(ledger){
    *ledger<<mode<<','<<trial<<','<<(training?"TRAIN":"VALIDATION")<<','<<day<<','
     <<held->symbol<<','<<held->entry<<','<<key<<','<<(key-held->entry)<<','
     <<held->quantity<<','<<held->entry_close<<','<<held->fill<<','<<close<<','<<fill<<','
     <<held->entry_fee<<','<<fee<<','<<gross<<','<<net<<','
     <<(forced?"FORCED_1515":(switching?"SWITCH_EXIT":"EXIT"))<<',';
    devai::strategy::d9f3a::writeAuditValues(*ledger,entry_forecast);
    *ledger<<',';
    devai::strategy::d9f3a::writeAuditValues(*ledger,exit_audit);
    *ledger<<'\n';
   }
   ++m.trade_rows;m.ledger_net+=net;
   last_exit[held->symbol]=key;held.reset();++m.exits;if(forced)++m.forced;
  };
  auto buy=[&](const Row& r,Key key,bool switching=false)->bool{
   const int n=affordable(cash,r.obs.close,charge_costs?slip:0.0,broker,!charge_costs);
   if(n<=0){++m.unaffordable;return false;}
   const double fill=r.obs.close*(1+(charge_costs?slip:0.0)/10000.0);
   const double t=n*fill,fee=charge_costs?broker.calculate(ExecutionSide::BUY,t).total:0.0;
   ensure(t+fee<=cash+1e-8,"buy exceeds available cash");
   cash-=t+fee;m.fees+=fee;session_fees+=fee;
   const double sl=n*(fill-r.obs.close);m.slippage+=sl;session_slippage+=sl;
   held=Position{r.obs.symbol,n,fill,key,fee,r.obs.close};++per_symbol[r.obs.symbol];++entries_total;++m.entries;
   ensure(current_at!=nullptr,"missing audit market snapshot");
   emit_audit(*current_at,key,switching?"SWITCH_OPEN":"OPEN",r.obs.symbol);
   return true;
  };
  for(std::size_t i=0;i<rows.size();){
   const Key key=rows[i].obs.decision_key;
   std::map<std::string,const Row*> at;
   std::size_t j=i;
   for(;j<rows.size()&&rows[j].obs.decision_key==key;++j)at.emplace(rows[j].obs.symbol,&rows[j]);
   ensure(at.size()==10,"expected ten symbols per minute");
   current_at=&at;
   ensure(!prev||key==prev+1,"minute gap");prev=key;
   const bool terminal=key%1440>=915;
   // Portfolio equity BEFORE this minute's action. Reflects mark-to-market since prior decision.
   const double pre_equity=cash+(held?held->quantity*at.at(held->symbol)->obs.close:0.0);
   ensure(std::isfinite(pre_equity),"nonfinite equity");
   auto attribute=[&](double next_equity,const State& next_state,const std::vector<Action>& next_legal,bool done){
    if(!pending.valid)return;
    const double reward=devai::strategy::d9f3::transitionReward(pending.equity_before,next_equity,CAPITAL);
    if(training){recordReward(m,reward);model.learn(pending.state,pending.action,reward,next_state,next_legal,done);++m.updates;}
    else{++m.eval_reward_count;m.eval_reward_sum+=reward;if(reward!=0.0)++m.eval_reward_nonzero;}
   };
   if(terminal){
    if(held)sell(at,key,true); // include final close, sell fee in terminal reward
    attribute(cash,State{}, {},true);
    pending.valid=false;
    ensure(!held,"overnight exposure");
    i=j;continue;
   }
   Decision d=decide(at,held,per_symbol,last_exit,key,entries_total,forecast,cash,broker,charge_costs?slip:0.0,!charge_costs,score_threshold,m);
   attribute(pre_equity,d.state,d.legal,false);
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
   // D9F.3B read-only attribution: Q-value reads do not advance the RNG.
   const auto legal_has=[&](Action a){return std::find(d.legal.begin(),d.legal.end(),a)!=d.legal.end();};
   const double q_wait=legal_has(Action::WAIT)?model.qValue(d.state,Action::WAIT):std::numeric_limits<double>::quiet_NaN();
   const double q_open=legal_has(Action::OPEN)?model.qValue(d.state,Action::OPEN):std::numeric_limits<double>::quiet_NaN();
   const double q_hold=legal_has(Action::HOLD)?model.qValue(d.state,Action::HOLD):std::numeric_limits<double>::quiet_NaN();
   const double q_exit=legal_has(Action::EXIT)?model.qValue(d.state,Action::EXIT):std::numeric_limits<double>::quiet_NaN();
   const double q_switch=legal_has(Action::SWITCH)?model.qValue(d.state,Action::SWITCH):std::numeric_limits<double>::quiet_NaN();
   // Candidate rows show eligibility and scores for ALL ten symbols. The current
   // policy only offers the highest-scoring eligible candidate to the RL model.
   if(candidates){
    for(const auto& [sym,r]:at){
     const bool is_held=held&&held->symbol==sym;
     const auto count=per_symbol.find(sym);
     const bool symbol_limit=count!=per_symbol.end()&&count->second>=MAX_ENTRIES_SYMBOL;
     const auto last=last_exit.find(sym);
     const bool cooldown=last!=last_exit.end()&&key-last->second<COOLDOWN;
     const double buying_power=cash+(held?held->quantity*at.at(held->symbol)->obs.close:0.0);
     const bool can_afford=affordable(buying_power,r->obs.close,charge_costs?slip:0.0,broker,!charge_costs)>0;
     const double statistical=.5*r->obs.zscore_1m+.3*r->obs.zscore_5m+.2*r->obs.zscore_15m;
     const double forecast_component=forecast&&r->obs.forecast_ready?100.0*r->obs.predicted_forward_return:0.0;
     const bool candidate_eligible=r->selected&&!is_held&&!symbol_limit&&!cooldown&&can_afford;
     *candidates<<mode<<','<<trial<<','<<(training?"TRAIN":"VALIDATION")<<','<<day<<','<<key<<','<<sym<<','
       <<r->selected<<','<<r->obs.forecast_ready<<','<<r->obs.predicted_forward_return<<','
       <<r->obs.zscore_1m<<','<<r->obs.zscore_5m<<','<<r->obs.zscore_15m<<','
       <<statistical<<','<<forecast_component<<','<<(statistical+forecast_component)<<','
       <<is_held<<','<<symbol_limit<<','<<cooldown<<','<<can_afford<<','<<candidate_eligible<<','
       <<(d.candidate&&d.candidate->obs.symbol==sym)<<','<<score_threshold<<'\n';
    }
   }
   const Action selected=model.choose(d.state,d.legal,training);
   if(decisions){
    auto value=[&](double q){if(std::isfinite(q))*decisions<<q;};
    *decisions<<mode<<','<<trial<<','<<(training?"TRAIN":"VALIDATION")<<','<<day<<','<<key<<','
      <<(held?held->symbol:std::string(""))<<','<<(d.candidate?d.candidate->obs.symbol:std::string(""))<<','
      <<d.state.candidate_score<<','<<d.state.advantage<<','<<d.state.minutes_to_close<<','
      <<d.state.entries_remaining<<','<<d.state.position_age<<','<<d.state.held_score<<','
      <<legal_has(Action::WAIT)<<','<<legal_has(Action::OPEN)<<','<<legal_has(Action::HOLD)<<','
      <<legal_has(Action::EXIT)<<','<<legal_has(Action::SWITCH)<<',';
    value(q_wait);*decisions<<',';value(q_open);*decisions<<',';value(q_hold);*decisions<<',';
    value(q_exit);*decisions<<',';value(q_switch);*decisions<<',';
    if(std::isfinite(q_open)&&std::isfinite(q_wait))*decisions<<(q_open-q_wait);
    *decisions<<','<<static_cast<int>(greedy)<<','<<static_cast<int>(selected)<<','
      <<(selected==greedy)<<','<<(d.candidate?d.candidate->obs.predicted_forward_return:0.0)<<'\n';
   }

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
     sell(at,key,false,true);
     if(buy(*d.candidate,key,true))++m.switches;
     else {executed=Action::EXIT;++m.blocked;}
     break;
   }
   ensure(cash>=-1e-6,"negative cash");
   const double after=cash+(held?held->quantity*at.at(held->symbol)->obs.close:0.0);
   m.min_equity=std::min(m.min_equity,after);
   pending={true,d.state,executed,pre_equity};
   i=j;
  }
  ensure(!held,"unclosed session position");
  const double pnl=cash-CAPITAL;
  ensure(std::abs((m.ledger_net /* cumulative */)-(m.net+pnl))<0.02,"trade ledger does not reconcile to portfolio net PnL");
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
  <<" reward_units=action_equity_delta_including_immediate_costs_divided_by_7000"<<'\n';
 std::cout<<"D9F3_AUDIT mode="<<mode<<" trial="<<trial<<" phase="<<phase
  <<" evaluation_reward_observations="<<m.eval_reward_count
  <<" evaluation_reward_nonzero="<<m.eval_reward_nonzero
  <<" evaluation_reward_sum="<<std::setprecision(12)<<m.eval_reward_sum
  <<" training_reward_sum="<<m.reward_sum
  <<" training_reward_mean="<<(m.reward_count?m.reward_sum/m.reward_count:0.0)
  <<" ledger_trades="<<m.trade_rows<<" ledger_net="<<m.ledger_net
  <<" portfolio_net="<<m.net<<std::setprecision(6)<<'\n';
}
}
int main(int argc,char**argv){
 try{
  if(argc!=9)throw std::invalid_argument("Usage: runner causal.csv TRAIN_START VAL_START OOS_START forecast_on|forecast_off slippage_bps train_zero|train_brokerage ledger_directory");
  const Key train=dateKey(argv[2]),val=dateKey(argv[3]),oos=dateKey(argv[4]);
  ensure(train<val&&val<oos,"invalid split");
  const std::string mode=argv[5];ensure(mode=="forecast_on"||mode=="forecast_off","invalid mode");
  const bool forecast=mode=="forecast_on";
  const double slip=std::stod(argv[6]);ensure(std::isfinite(slip)&&slip>=0&&slip<=100,"invalid slippage");
  Diagnostics diagnostics;auto data=load(argv[1]);auto accepted=sessions(data,diagnostics,train,val,oos);
  printDiagnostics(diagnostics);
  ensure(diagnostics.accepted_train>0&&diagnostics.accepted_val>0,"empty train/validation");
  const std::string training_mode=argv[7];
  ensure(training_mode=="train_zero"||training_mode=="train_brokerage","invalid training mode");
  const bool training_costs=training_mode=="train_brokerage";
  const std::filesystem::path ledger_dir=argv[8];
  std::filesystem::create_directories(ledger_dir);
  // Only accepted TRAIN and VALIDATION rows are indexed. OOS never enters audit.
  const ForwardIndex forward_index=devai::strategy::d9f3a::buildIndex(accepted,train,val,oos);
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
  std::cout<<"D9F3_CONFIG training="<<training_mode<<" mode="<<mode<<" threshold="<<score_threshold
   <<" calibration=train_only selected_top5_rows percentile=0.80"
   <<" train_cost="<<training_mode<<" validation_cost=brokerage_engine"
   <<" oos_evaluated=no"<<'\n';
  int i=0;
  for(const auto& t:grid){
   SharedManagerQ model({t.alpha,t.gamma,t.epsilon,17});
   const int trial=i+1;
   const auto decision_path=ledger_dir/(mode+"_"+training_mode+"_trial"+std::to_string(trial)+"_D9F3B_decisions.csv");
    const auto candidate_path=ledger_dir/(mode+"_"+training_mode+"_trial"+std::to_string(trial)+"_D9F3B_candidates.csv");
    std::ofstream decisions(decision_path), candidates(candidate_path);
    ensure(decisions.good()&&candidates.good(),"cannot open D9F3B decision/candidate audit");
    decisions<<"mode,trial,phase,day,decision_key,held_symbol,manager_candidate,candidate_score,advantage,minutes_to_close,entries_remaining,position_age,held_score,legal_wait,legal_open,legal_hold,legal_exit,legal_switch,q_wait,q_open,q_hold,q_exit,q_switch,q_open_minus_wait,greedy_action_code,chosen_action_code,chosen_is_greedy,candidate_predicted_return\n";
    candidates<<"mode,trial,phase,day,decision_key,symbol,selected_top5,forecast_ready,predicted_return,zscore_1m,zscore_5m,zscore_15m,statistical_score,forecast_component,combined_score,is_held,symbol_limit,cooldown,affordable,candidate_eligible,manager_candidate,score_threshold\n";
    decisions<<std::setprecision(17);candidates<<std::setprecision(17);
    const auto ledger_path=ledger_dir/(mode+"_"+training_mode+"_trial"+std::to_string(trial)+".csv");
   std::ofstream ledger(ledger_path);
   ensure(ledger.good(),"cannot open trade ledger");
   ledger<<"mode,trial,phase,day,symbol,entry_key,exit_key,holding_minutes,quantity,entry_close,entry_fill,exit_close,exit_fill,entry_fee,exit_fee,gross_pnl,net_pnl,exit_reason,entry_close_for_audit,entry_predicted_5bar_price,entry_predicted_5bar_return,entry_actual_5bar_price,entry_actual_5bar_return,entry_forecast_error_return,entry_horizon_available,exit_close_for_audit,exit_predicted_5bar_price,exit_predicted_5bar_return,exit_actual_5bar_price,exit_actual_5bar_return,exit_forecast_error_return,exit_horizon_available\n";
   ledger<<std::fixed<<std::setprecision(10);
   const auto audit_path=ledger_dir/(mode+"_"+training_mode+"_trial"+std::to_string(trial)+"_forward_audit.csv");
   std::ofstream audit(audit_path);
   ensure(audit.good(),"cannot open forward audit");
   audit<<"mode,trial,phase,day,decision_key,event,traded_symbol,symbol,is_traded,selected_top5,forecast_ready,close,predicted_5bar_price,predicted_5bar_return,actual_5bar_price,actual_5bar_return,forecast_error_return,horizon_available\n";
   audit<<std::fixed<<std::setprecision(10);
   auto tr=replay(accepted,train,val,model,true,forecast,slip,broker,score_threshold,training_costs,&ledger,&audit,forward_index,mode,trial,&decisions,&candidates);
   auto vr=replay(accepted,val,oos,model,false,forecast,slip,broker,score_threshold,training_costs,&ledger,&audit,forward_index,mode,trial,&decisions,&candidates);
   ledger.flush();ensure(ledger.good(),"trade ledger write failed");
   audit.flush();ensure(audit.good(),"forward audit write failed");
   std::cout<<"D9F3A_FORWARD_AUDIT mode="<<mode<<" trial="<<trial<<" training="<<training_mode
    <<" path="<<audit_path.string()<<" horizon=5_same_session_minutes oos_evaluated=no\n";
   std::cout<<"D9F3_LEDGER mode="<<mode<<" trial="<<trial<<" training="<<training_mode<<" path="<<ledger_path.string()<<'\n';
   print(mode.c_str(),++i,training_costs?"TRAIN_NET":"TRAIN_GROSS",tr,model);
   print(mode.c_str(),i,"VALIDATION_NET",vr,model);
  }
  std::cout<<"D9F3B_DECISION_AUDIT_COMPLETE training="<<training_mode<<" model_saved=no model_approved=no oos_evaluated=no\n";
 }catch(const std::exception& e){std::cerr<<"D9F_FAIL "<<e.what()<<'\n';return 1;}
 return 0;
}
