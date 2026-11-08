// CETE D9E portfolio research runner. Single DevManager_001, one position,
// ten-stock universe, five candidates, one shared D9B Q model.
// Gross-only training; Zerodha-configured cash ledger validation. No OOS replay.
#define main d9c_original_main_disabled
#include "run_dev_d9c_shared_rl_training.cpp"
#undef main
#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"
#include <array>
#include <iomanip>
#include <optional>
#include <unordered_map>

namespace {
using devai::strategy::brokerage::BrokerageChargesEngine;
using devai::strategy::brokerage::ExecutionSide;
constexpr double INITIAL_CAPITAL=7000.0;
constexpr int MIN_HOLD=5, COOLDOWN=5, MAX_ENTRIES=2;
constexpr double SWITCH_MARGIN=0.75; // causal z-score units, NOT probability
struct Holding {std::string symbol;int qty=0;double entry_price=0;Key entry_key=0;};
struct PendingDecision {bool valid=false;DevD9BObservation obs;DevD9APosition side=DevD9APosition::FLAT;DevD9AAction action=DevD9AAction::HOLD_FLAT;};
struct PortfolioStats {
 std::size_t sessions=0,entries=0,exits=0,switches=0,holds=0,flat=0,forced=0,updates=0,blocked_cap=0,blocked_hold=0,blocked_cooldown=0,unaffordable=0;
 double realized=0,fees=0,slippage=0,ending_cash_sum=0,minimum_equity=INITIAL_CAPITAL;
};
void require(bool yes,const std::string& what){if(!yes)throw std::runtime_error("D9E_PORTFOLIO "+what);}
double opportunity(const Row& r,bool forecast){
 // Causal *ranking heuristic*; NOT a calibrated probability or a forward return label.
 const auto& o=r.obs;
 return 0.5*o.zscore_1m+0.3*o.zscore_5m+0.2*o.zscore_15m+
        (forecast&&o.forecast_ready?100.0*o.predicted_forward_return:0.0);
}
std::vector<DevD9AAction> legalFlat(const Row& r,Key key,bool allowed){
 std::vector<DevD9AAction> a{DevD9AAction::HOLD_FLAT};
 if(allowed&&r.selected&&key%1440<900)a.push_back(DevD9AAction::OPEN_LONG);
 return a;
}
std::vector<DevD9AAction> legalLong(Key key,Key entry){
 if(key%1440>=915)return {DevD9AAction::FORCE_EXIT};
 if(key-entry<MIN_HOLD)return {DevD9AAction::HOLD_LONG};
 return {DevD9AAction::HOLD_LONG,DevD9AAction::CLOSE_LONG};
}
int affordable(double cash,double price,const BrokerageChargesEngine& broker,double slip_bps,bool costs){
 if(!(cash>0&&price>0))return 0;
 const double fill=price*(1+slip_bps/10000.0*(costs?1.0:0.0));
 int n=static_cast<int>(std::floor(cash/fill));
 while(n>0){
  const double turnover=n*fill;
  const double fee=costs?broker.calculate(ExecutionSide::BUY,turnover).total:0.0;
  if(turnover+fee<=cash+1e-8)break;
  --n;
 }
 return n;
}
struct D9EForecastThresholds {double inner=0,outer=0;};
D9EForecastThresholds trainingForecastThresholds(const Sessions& all,Key begin,Key end){
 std::vector<double> v;
 for(const auto& [day,rows]:all){
  if(day*1440<begin||day*1440>=end)continue;
  for(const auto& r:rows)if(r.selected&&r.obs.forecast_ready){
   const double x=std::abs(r.obs.predicted_forward_return);
   if(std::isfinite(x)&&x>0)v.push_back(x);
  }
 }
 require(v.size()>=100,"insufficient train forecasts");
 std::sort(v.begin(),v.end());
 const double inner=v[static_cast<std::size_t>(0.4*(v.size()-1))];
 const double outer=v[static_cast<std::size_t>(0.8*(v.size()-1))];
 require(inner>0&&outer>inner,"degenerate train forecast thresholds");
 return {inner,outer};
}
struct EvalConfig {bool training=false;bool forecast=false;double slip_bps=0;};
PortfolioStats replayPortfolio(const Sessions& all,Key begin,Key end,DevD9BSharedQModel& model,
                              EvalConfig cfg,const BrokerageChargesEngine& broker){
 PortfolioStats stats;
 for(const auto& [day,rows]:all){
  if(day*1440<begin||day*1440>=end)continue;
  double cash=INITIAL_CAPITAL;
  std::optional<Holding> holding;
  std::unordered_map<std::string,int> entries;
  std::unordered_map<std::string,Key> last_exit;
  std::unordered_map<std::string,double> last_price;
  PendingDecision pending;
  Key previous_key=0;
  auto closePosition=[&](double market,Key key,bool forced){
   require(holding.has_value(),"close without position");
   const double fill=market*(1-(cfg.training?0.0:cfg.slip_bps)/10000.0);
   const double proceeds=holding->qty*fill;
   const double charge=cfg.training?0.0:broker.calculate(ExecutionSide::SELL,proceeds).total;
   cash+=proceeds-charge;
   stats.fees+=charge;
   stats.slippage+=holding->qty*(market-fill);
   stats.realized+=holding->qty*(fill-holding->entry_price)-charge;
   last_exit[holding->symbol]=key;
   holding.reset();++stats.exits;
   if(forced)++stats.forced;
  };
  for(std::size_t i=0;i<rows.size();){
   const Key key=rows[i].obs.decision_key;
   std::size_t j=i;
   std::map<std::string,const Row*> at;
   for(;j<rows.size()&&rows[j].obs.decision_key==key;++j)at.emplace(rows[j].obs.symbol,&rows[j]);
   require(at.size()==10,"missing ten-symbol minute");
   require(!previous_key||key==previous_key+1,"minute gap");
   previous_key=key;
   const std::string prior_symbol=holding?holding->symbol:"";
   const double old_price=prior_symbol.empty()?0.0:last_price.at(prior_symbol);
   const double new_price=prior_symbol.empty()?0.0:at.at(prior_symbol)->obs.close;
   // One-minute gross return on prior position. No future candle is used for the decision.
   const double reward=prior_symbol.empty()?0.0:(new_price/old_price-1.0);
   require(std::isfinite(reward)&&std::abs(reward)<=0.20,"implausible one-minute return");
   const bool terminal=(key%1440>=915);
   // Previous action is learned only when its following minute arrives.
   // Bootstrap action set is restricted to the actual manager's portfolio state.
   if(pending.valid&&cfg.training){
    const Row* next=nullptr;
    if(holding)next=at.at(holding->symbol);
    else {
     for(const auto& [sym,r]:at)if(r->selected&&(!next||opportunity(*r,cfg.forecast)>opportunity(*next,cfg.forecast)))next=r;
    }
    require(next!=nullptr,"no next observation");
    std::vector<DevD9AAction> next_legal;
    if(terminal)next_legal={DevD9AAction::HOLD_FLAT};
    else if(holding)next_legal=legalLong(key,holding->entry_key);
    else next_legal=legalFlat(*next,key,entries[next->obs.symbol]<MAX_ENTRIES);
    model.learn(pending.obs,pending.side,pending.action,reward,next->obs,
                holding?DevD9APosition::LONG:DevD9APosition::FLAT,next_legal,terminal);
    ++stats.updates;
   }
   pending.valid=false;
   // All prices for this minute are known before selection; no future information.
   for(const auto& [sym,r]:at)last_price[sym]=r->obs.close;
   if(terminal){
    if(holding)closePosition(at.at(holding->symbol)->obs.close,key,true);
    require(!holding,"terminal exposure");
    i=j;continue;
   }
   // Highest causal opportunity among current top-five candidates.
   const Row* best=nullptr;
   for(const auto& [sym,r]:at)if(r->selected && (!best||opportunity(*r,cfg.forecast)>opportunity(*best,cfg.forecast)))best=r;
   require(best!=nullptr,"no top-five candidates");
   if(holding){
    const auto held=at.at(holding->symbol);
    const auto options=legalLong(key,holding->entry_key);
    if(options.size()==1)++stats.blocked_hold;
    const auto chosen=model.choose(held->obs,DevD9APosition::LONG,options,cfg.training);
    const bool switch_candidate=best->obs.symbol!=holding->symbol && best->selected &&
       opportunity(*best,cfg.forecast)>opportunity(*held,cfg.forecast)+SWITCH_MARGIN &&
       entries[best->obs.symbol]<MAX_ENTRIES && key%1440<900;
    const bool can_close=(key-holding->entry_key>=MIN_HOLD);
    // Switch only if the current policy permits exit; new stock must independently pass OPEN.
    bool switch_now=false;
    if(can_close&&switch_candidate){
      auto enter_options=legalFlat(*best,key,true);
      switch_now=model.choose(best->obs,DevD9APosition::FLAT,enter_options,false)==DevD9AAction::OPEN_LONG;
    }
    const bool exit=(chosen==DevD9AAction::CLOSE_LONG)||(can_close&&switch_now);
    const DevD9AAction executed=exit?DevD9AAction::CLOSE_LONG:DevD9AAction::HOLD_LONG;
    pending={true,held->obs,DevD9APosition::LONG,executed};
    if(exit){
     const bool switching=switch_now;
     closePosition(held->obs.close,key,false);
     if(switching)++stats.switches; // exit now; candidate is reconsidered next minute
    }else ++stats.holds;
   }else{
    bool allowed=entries[best->obs.symbol]<MAX_ENTRIES;
    if(!allowed)++stats.blocked_cap;
    if(last_exit.count(best->obs.symbol)&&key-last_exit[best->obs.symbol]<COOLDOWN){allowed=false;++stats.blocked_cooldown;}
    const auto options=legalFlat(*best,key,allowed);
    const auto chosen=model.choose(best->obs,DevD9APosition::FLAT,options,cfg.training);
    const bool opening=chosen==DevD9AAction::OPEN_LONG;
    pending={true,best->obs,DevD9APosition::FLAT,chosen};
    if(opening){
     const int n=affordable(cash,best->obs.close,broker,cfg.slip_bps,!cfg.training);
     if(n>0){
      const double fill=best->obs.close*(1+(cfg.training?0.0:cfg.slip_bps)/10000.0);
      const double turnover=n*fill;
      const double fee=cfg.training?0.0:broker.calculate(ExecutionSide::BUY,turnover).total;
      cash-=turnover+fee;stats.fees+=fee;stats.slippage+=n*(fill-best->obs.close);
      stats.realized-=fee;
      holding=Holding{best->obs.symbol,n,fill,key};
      ++entries[best->obs.symbol];++stats.entries;
     }else ++stats.unaffordable;
    }else ++stats.flat;
   }
   require(cash>=-1e-6,"negative cash");
   const double equity=cash+(holding?holding->qty*at.at(holding->symbol)->obs.close:0.0);
   stats.minimum_equity=std::min(stats.minimum_equity,equity);
   i=j;
  }
  require(!holding,"overnight position");
  stats.ending_cash_sum+=cash;
  ++stats.sessions;
 }
 return stats;
}
void report(const std::string& mode,int trial,const char* phase,const PortfolioStats& r){
 std::cout<<"D9E_PORTFOLIO mode="<<mode<<" trial="<<trial<<" phase="<<phase
          <<" sessions="<<r.sessions<<" entries="<<r.entries<<" exits="<<r.exits
          <<" switches="<<r.switches<<" holds="<<r.holds<<" flat="<<r.flat
          <<" forced="<<r.forced<<" learning_updates="<<r.updates
          <<" fees_rupees="<<r.fees<<" slippage_rupees="<<r.slippage
          <<" realized_pnl_rupees="<<r.realized
          <<" avg_final_equity_rupees="<<(r.sessions?r.ending_cash_sum/r.sessions:0)
          <<" blocked_cap="<<r.blocked_cap<<" blocked_hold="<<r.blocked_hold
          <<" blocked_cooldown="<<r.blocked_cooldown<<" unaffordable="<<r.unaffordable<<'\n';
}
}
int main(int argc,char**argv){
 try{
  if(argc!=7)throw std::invalid_argument("Usage: runner causal.csv TRAIN_START VALIDATION_START OOS_START forecast_on|forecast_off slippage_bps_per_side");
  const Key train=dateKey(argv[2]),val=dateKey(argv[3]),oos=dateKey(argv[4]);
  require(train<val&&val<oos,"invalid split");
  const std::string mode=argv[5];require(mode=="forecast_on"||mode=="forecast_off","invalid mode");
  const double slip=std::stod(argv[6]);require(std::isfinite(slip)&&slip>=0&&slip<=100,"invalid slippage");
  const bool forecast=mode=="forecast_on";
  auto raw=load(argv[1]);Diagnostics diag;
  const auto days=sessions(raw,diag,train,val,oos);printDiagnostics(diag);
  require(diag.accepted_train>0&&diag.accepted_val>0,"empty train or validation");
  const auto thresholds=forecast?trainingForecastThresholds(days,train,val):D9EForecastThresholds{};
  BrokerageChargesEngine broker;
  std::cout<<std::fixed<<std::setprecision(6)
           <<"D9E_PORTFOLIO_CONFIG manager=DevManager_001 total_capital=7000 one_position=yes top5=yes"
           <<" train_cost=0 validation_cost=brokerage_engine slippage_bps_per_side="<<slip
           <<" forecast="<<mode<<" min_hold=5 cooldown=5 entries_per_symbol=2"
           <<" switch_margin_z_units="<<SWITCH_MARGIN<<" model_saved=no oos_evaluated=no\n";
  struct Trial{double alpha,gamma,epsilon;};
  const std::array<Trial,6> grid{{{.02,.90,.05},{.02,.95,.10},{.05,.90,.05},{.05,.95,.10},{.10,.90,.05},{.10,.95,.10}}};
  int trial=0;
  for(const auto& t:grid){
   DevD9BConfig c;c.permissions.enable_long=true;c.permissions.enable_short=false;
   c.round_trip_cost_bps=0;c.enable_predicted_forward_return=forecast;
   c.forecast_inner_abs=forecast?thresholds.inner:0;
   c.forecast_outer_abs=forecast?thresholds.outer:0;
   c.q_alpha=t.alpha;c.q_gamma=t.gamma;c.epsilon=t.epsilon;c.seed=17;
   DevD9BSharedQModel model(c);
   const auto tr=replayPortfolio(days,train,val,model,{true,forecast,0},broker);
   const auto vr=replayPortfolio(days,val,oos,model,{false,forecast,slip},broker);
   report(mode,++trial,"TRAIN_GROSS",tr);report(mode,trial,"VALIDATION_NET",vr);
  }
  std::cout<<"D9E_PORTFOLIO_COMPLETE model_saved=no model_approved=no oos_evaluated=no\n";
 }catch(const std::exception&e){std::cerr<<"D9E_PORTFOLIO_FAIL "<<e.what()<<'\n';return 1;}
}
