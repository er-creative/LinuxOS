#include "devai/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;
namespace {
using Key=std::int64_t;
int digits(const std::string&s,int a,int n){int v=0;for(int i=a;i<a+n;++i){if(s.at(i)<'0'||s.at(i)>'9')throw std::invalid_argument("bad date");v=v*10+s[i]-'0';}return v;}
Key ordinal(int y,unsigned m,unsigned d){y-=m<=2;int era=(y>=0?y:y-399)/400;unsigned yoe=y-era*400;unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;unsigned doe=yoe*365+yoe/4-yoe/100+doy;return static_cast<Key>(era)*146097+doe;}
Key dateKey(const std::string&s){if(s.size()!=10||s[4]!='-'||s[7]!='-')throw std::invalid_argument("date YYYY-MM-DD required");int y=digits(s,0,4),m=digits(s,5,2),d=digits(s,8,2);static const int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};if(y<1900||y>2200||m<1||m>12||d<1||d>days[m]+(m==2&&y%4==0&&(y%100!=0||y%400==0)))throw std::invalid_argument("invalid calendar date");return ordinal(y,m,d)*1440;}
struct Row {DevD9BObservation obs;bool selected=false;};
using Session=std::vector<Row>;
using Sessions=std::map<Key,Session>;
std::vector<std::string> split(const std::string&s){std::stringstream in(s);std::vector<std::string> f;std::string v;while(std::getline(in,v,','))f.push_back(v);return f;}
std::vector<Row> load(const std::string&path){
 std::ifstream in(path);if(!in)throw std::runtime_error("Cannot open causal CSV");std::string line;
 if(!std::getline(in,line)||line!="symbol,decision_key,close,predicted_forward_return,zscore_1m,zscore_5m,zscore_15m,forecast_ready,shortable,selected_top5")throw std::runtime_error("D9C CSV schema mismatch");
 std::vector<Row> rows;rows.reserve(1400000);Key prev=0;std::set<std::string> symbols;
 while(std::getline(in,line)){
  if(line.empty())continue;
  auto f=split(line);
  if(f.size()!=10)throw std::runtime_error("D9C invalid CSV row");
  Row r;r.obs.symbol=f[0];r.obs.decision_key=std::stoll(f[1]);r.obs.close=std::stod(f[2]);r.obs.predicted_forward_return=std::stod(f[3]);r.obs.zscore_1m=std::stod(f[4]);r.obs.zscore_5m=std::stod(f[5]);r.obs.zscore_15m=std::stod(f[6]);
  if((f[7]!="0"&&f[7]!="1")||(f[8]!="0"&&f[8]!="1")||(f[9]!="0"&&f[9]!="1"))throw std::runtime_error("D9C invalid boolean");
  r.obs.forecast_ready=f[7]=="1";r.obs.shortable=f[8]=="1";r.selected=f[9]=="1";
  if(r.obs.symbol.empty()||r.obs.decision_key<=0||!std::isfinite(r.obs.close)||r.obs.close<=0||!std::isfinite(r.obs.predicted_forward_return)||!std::isfinite(r.obs.zscore_1m)||!std::isfinite(r.obs.zscore_5m)||!std::isfinite(r.obs.zscore_15m))throw std::runtime_error("D9C invalid numeric observation");
  if(prev&&r.obs.decision_key<prev)throw std::runtime_error("D9C CSV is not chronological");
  prev=r.obs.decision_key;symbols.insert(r.obs.symbol);rows.push_back(std::move(r));
 }
 if(symbols.size()!=10||rows.empty())throw std::runtime_error("D9C requires ten unique symbols");
 return rows;
}
struct Diagnostics {
 std::size_t source_days=0,accepted=0,rejected=0,ignored_after_cutoff_rows=0;
 std::size_t missing_1515=0,missing_stock=0,duplicate_stock=0,missing_minute=0,invalid_top5=0;
 std::size_t source_train=0,source_val=0,source_oos=0,accepted_train=0,accepted_val=0,accepted_oos=0;
};
Sessions sessions(const std::vector<Row>&rows,Diagnostics&d,Key train,Key val,Key oos){
 Sessions all,accepted;
 for(const auto&r:rows)all[(r.obs.decision_key-1)/1440].push_back(r);
 d.source_days=all.size();
 for(auto&[day,rs]:all){
  const Key day_key=day*1440;
  if(day_key>=train&&day_key<val)++d.source_train;
  else if(day_key>=val&&day_key<oos)++d.source_val;
  else if(day_key>=oos)++d.source_oos;
  bool missing_stock=false,duplicate_stock=false,missing_minute=false,invalid_top5=false,has_1515=false;
  Key previous=-1;
  Session trading;
  trading.reserve(rs.size());
  for(std::size_t i=0;i<rs.size();){
   const Key key=rs[i].obs.decision_key;
   const Key minute=key-day*1440;
   std::set<std::string> symbols;
   std::size_t selected=0,j=i;
   while(j<rs.size()&&rs[j].obs.decision_key==key){
    if(minute>=555&&minute<=915){
     if(!symbols.insert(rs[j].obs.symbol).second)duplicate_stock=true;
     selected+=rs[j].selected?1U:0U;
     trading.push_back(rs[j]);
    }else if(minute>915){++d.ignored_after_cutoff_rows;}
    ++j;
   }
   if(minute>=555&&minute<=915){
    if(symbols.size()!=10)missing_stock=true;
    if(selected!=5)invalid_top5=true;
    if(previous!=-1&&key!=previous+1)missing_minute=true;
    previous=key;
    if(minute==915)has_1515=true;
   }
   i=j;
  }
  if(!has_1515)++d.missing_1515;
  if(missing_stock)++d.missing_stock;
  if(duplicate_stock)++d.duplicate_stock;
  if(missing_minute)++d.missing_minute;
  if(invalid_top5)++d.invalid_top5;
  if(trading.empty()||!has_1515||missing_stock||duplicate_stock||missing_minute||invalid_top5){++d.rejected;continue;}
  accepted.emplace(day,std::move(trading));
  ++d.accepted;
  if(day_key>=train&&day_key<val)++d.accepted_train;
  else if(day_key>=val&&day_key<oos)++d.accepted_val;
  else if(day_key>=oos)++d.accepted_oos;
 }
 return accepted;
}
void printDiagnostics(const Diagnostics&d){
 std::cout<<"D9C_SESSION_DIAGNOSTICS source_days="<<d.source_days<<" accepted="<<d.accepted<<" rejected="<<d.rejected<<" ignored_after_15_15_rows="<<d.ignored_after_cutoff_rows<<'\n';
 std::cout<<"D9C_PARTITIONS train_source="<<d.source_train<<" train_accepted="<<d.accepted_train<<" validation_source="<<d.source_val<<" validation_accepted="<<d.accepted_val<<" oos_source="<<d.source_oos<<" oos_accepted="<<d.accepted_oos<<'\n';
 std::cout<<"D9C_REJECTION_REASONS missing_15_15="<<d.missing_1515<<" missing_stock="<<d.missing_stock<<" duplicate_stock="<<d.duplicate_stock<<" missing_minute="<<d.missing_minute<<" invalid_top5="<<d.invalid_top5<<'\n';
}
// Diagnostic keys are obtained from the model itself, never reconstructed here.
using QValues=std::array<double,8>;
using QTable=std::unordered_map<std::string,QValues>;
QTable readQSnapshot(const std::string&path){
 std::ifstream in(path);std::string magic;std::size_t n=0;
 if(!(in>>magic)||magic!="CETE_D9D_Q_V2")throw std::runtime_error("Q snapshot version mismatch");
 int mode=0;double inner=0,outer=0;
 if(!(in>>mode>>inner>>outer>>n)||n>10000000)throw std::runtime_error("Q diagnostic snapshot header invalid");
 QTable table;table.reserve(n);
 for(std::size_t i=0;i<n;++i){std::string key;QValues vals{};
  if(!(in>>key))throw std::runtime_error("Q diagnostic snapshot truncated");
  for(auto&v:vals)if(!(in>>v)||!std::isfinite(v))throw std::runtime_error("Q diagnostic snapshot contains invalid value");
  if(!table.emplace(key,vals).second)throw std::runtime_error("Q diagnostic snapshot duplicate key");
 }
 return table;
}
struct QDiag {std::size_t eligible=0,unseen=0,ties=0,positive=0,negative=0;
 double sum_advantage=0,min_advantage=std::numeric_limits<double>::infinity(),max_advantage=-std::numeric_limits<double>::infinity();};
QDiag qDiagnostics(const Sessions&days,Key begin,Key end,const QTable&q,const DevD9BSharedQModel&model,bool forecast_enabled){
 QDiag d;
 // These are hypothetical FLAT-state opportunities, not policy-dependent positions.
 // Top-five and forecast readiness gates match legalActions() for a flat position.
 DevD9BConfig cfg;cfg.permissions.enable_long=true;cfg.permissions.enable_short=false;
 for(const auto&[day,rows]:days){if(day*1440<begin||day*1440>=end)continue;
  for(const auto&r:rows){const auto&o=r.obs;
   if(!r.selected||(forecast_enabled&&!o.forecast_ready)||((o.decision_key%1440+1440)%1440)>=cfg.permissions.new_entry_cutoff_minutes)continue;
   ++d.eligible;auto it=q.find(model.stateKey(o,DevD9APosition::FLAT));
   if(it==q.end()){++d.unseen;++d.ties;continue;}
   const double hold=it->second.at(static_cast<std::size_t>(DevD9AAction::HOLD_FLAT));
   const double open=it->second.at(static_cast<std::size_t>(DevD9AAction::OPEN_LONG));
   const double adv=open-hold;d.sum_advantage+=adv;
   d.min_advantage=std::min(d.min_advantage,adv);d.max_advantage=std::max(d.max_advantage,adv);
   if(adv>1e-12)++d.positive;else if(adv< -1e-12)++d.negative;else ++d.ties;
  }
 }
 return d;
}
void printQDiag(const char*phase,std::size_t trial,const QDiag&d){
 std::cout<<"D9C_Q_VALUE_DIAGNOSTICS phase="<<phase<<" trial="<<trial
 <<" hypothetical_flat_eligible="<<d.eligible<<" unseen="<<d.unseen
 <<" positive_open_advantage="<<d.positive<<" negative_open_advantage="<<d.negative
 <<" equal_advantage="<<d.ties<<" mean_open_advantage="<<(d.eligible?d.sum_advantage/d.eligible:0)
 <<" min_open_advantage="<<(std::isfinite(d.min_advantage)?d.min_advantage:0)
 <<" max_open_advantage="<<(std::isfinite(d.max_advantage)?d.max_advantage:0)<<'\n';
}
// D9C Step 4: offline-only forecast/horizon diagnostics. Never used as policy inputs.
struct Corr {
 std::size_t n=0,positive_pred=0,positive_realized=0,correct_sign=0;
 double sx=0,sy=0,sxx=0,syy=0,sxy=0;
 void add(double x,double y){
  ++n;sx+=x;sy+=y;sxx+=x*x;syy+=y*y;sxy+=x*y;
  if(x>0)++positive_pred;
  if(y>0)++positive_realized;
  if((x>0&&y>0)||(x<0&&y<0))++correct_sign;
 }
 double pearson()const{
  if(n<2)return std::numeric_limits<double>::quiet_NaN();
  const double vx=n*sxx-sx*sx,vy=n*syy-sy*sy;
  return vx>0&&vy>0?(n*sxy-sx*sy)/std::sqrt(vx*vy):std::numeric_limits<double>::quiet_NaN();
 }
 double meanPred()const{return n?sx/static_cast<double>(n):0;}
 double meanReal()const{return n?sy/static_cast<double>(n):0;}
};
struct ForecastPair {double predicted,realized;};
struct ForecastSample {
 std::vector<ForecastPair> pairs;
 std::size_t insufficient=0,not_ready=0,not_selected=0;
};
ForecastSample collectForecast(const Sessions&days,Key begin,Key end,int horizon){
 ForecastSample d;
 for(const auto&[day,rows]:days){
  if(day*1440<begin||day*1440>=end)continue;
  std::map<std::string,std::vector<const Row*>> by_symbol;
  for(const auto&r:rows)by_symbol[r.obs.symbol].push_back(&r);
  for(const auto&[symbol,series]:by_symbol){
   for(std::size_t i=0;i<series.size();++i){
    const Row&r=*series[i];
    if(!r.selected){++d.not_selected;continue;}
    if(!r.obs.forecast_ready){++d.not_ready;continue;}
    const std::size_t future=i+static_cast<std::size_t>(horizon);
    if(future>=series.size()||series[future]->obs.decision_key!=r.obs.decision_key+horizon){
     ++d.insufficient;continue;
    }
    d.pairs.push_back({r.obs.predicted_forward_return,
                       series[future]->obs.close/r.obs.close-1.0});
   }
  }
 }
 return d;
}
std::array<double,4> trainingQuantiles(const ForecastSample&train){
 if(train.pairs.size()<5)throw std::runtime_error("D9C insufficient training forecast samples for quantiles");
 std::vector<double> sorted;sorted.reserve(train.pairs.size());
 for(const auto&p:train.pairs)sorted.push_back(p.predicted);
 std::sort(sorted.begin(),sorted.end());
 std::array<double,4> edges{};
 for(std::size_t i=0;i<4;++i){
  const std::size_t ix=((i+1)*sorted.size())/5;
  edges[i]=sorted[std::min(ix,sorted.size()-1)];
 }
 return edges;
}
std::size_t forecastQuintile(double x,const std::array<double,4>&edges){
 // Strict > keeps ties in the same bucket; empty quantiles are reported honestly.
 std::size_t b=0;while(b<edges.size()&&x>edges[b])++b;return b;
}
void printForecastAlignment(const char*phase,int horizon,const ForecastSample&sample,
                            const std::array<double,4>&train_edges,double round_trip_cost_bps){
 Corr all;std::array<Corr,5> q{};
 for(const auto&p:sample.pairs){all.add(p.predicted,p.realized);q[forecastQuintile(p.predicted,train_edges)].add(p.predicted,p.realized);}
 const double cost=round_trip_cost_bps/10000.0;
 std::cout<<"D9C_FORECAST_ALIGNMENT phase="<<phase<<" horizon_minutes="<<horizon
  <<" observations="<<all.n<<" insufficient_horizon="<<sample.insufficient
  <<" not_selected="<<sample.not_selected<<" forecast_not_ready="<<sample.not_ready
  <<" pearson="<<all.pearson()<<" mean_predicted="<<all.meanPred()
  <<" mean_realized="<<all.meanReal()
  <<" mean_realized_minus_round_trip_cost="<<(all.meanReal()-cost)
  <<" positive_pred_fraction="<<(all.n?static_cast<double>(all.positive_pred)/all.n:0)
  <<" correct_sign_fraction="<<(all.n?static_cast<double>(all.correct_sign)/all.n:0)<<'\n';
 std::cout<<"D9C_FORECAST_TRAIN_QUANTILE_EDGES horizon_minutes="<<horizon;
 for(std::size_t i=0;i<4;++i)std::cout<<" q"<<(i+1)<<"="<<train_edges[i];
 std::cout<<'\n';
 for(std::size_t i=0;i<q.size();++i){
  const Corr&b=q[i];
  std::cout<<"D9C_FORECAST_QUINTILE phase="<<phase<<" horizon_minutes="<<horizon
   <<" quintile="<<(i+1)<<" count="<<b.n<<" mean_predicted="<<b.meanPred()
   <<" mean_realized="<<b.meanReal()
   <<" mean_realized_minus_round_trip_cost="<<(b.meanReal()-cost)
   <<" positive_realized_fraction="<<(b.n?static_cast<double>(b.positive_realized)/b.n:0)
   <<'\n';
 }
}
void forecastAlignmentSuite(const Sessions&days,Key train,Key val,Key oos){
 // IMPORTANT: the D8 forecast's true target horizon is not established by the
 // D9C CSV or by the D8 manager assignment. These are comparisons, not proof
 // that any of these horizons is the correct training label.
 constexpr double cost_bps=20.0;
 std::cout<<"D9C_ALIGNMENT_NOTE forecast_target_horizon=UNVERIFIED "
          <<"diagnostic_horizons=1,5,15 train_quantiles_only=yes "
          <<"round_trip_cost_bps="<<cost_bps<<" oos_evaluated=no\n";
 for(int horizon:{1,5,15}){
  auto training=collectForecast(days,train,val,horizon);
  auto validation=collectForecast(days,val,oos,horizon);
  auto edges=trainingQuantiles(training);
  printForecastAlignment("TRAIN",horizon,training,edges,cost_bps);
  printForecastAlignment("VALIDATION",horizon,validation,edges,cost_bps);
 }
}
struct ForecastThresholds {double inner=0,outer=0;std::size_t n=0;};
ForecastThresholds calibrateForecast(const Sessions&days,Key train,Key val){
 std::vector<double> abs_values;
 for(const auto&[day,rows]:days){
  if(day*1440<train||day*1440>=val)continue;
  for(const auto&r:rows){
   if(!r.selected||!r.obs.forecast_ready)continue;
   const double v=std::abs(r.obs.predicted_forward_return);
   if(std::isfinite(v)&&v>0)abs_values.push_back(v);
  }
 }
 if(abs_values.size()<100)throw std::runtime_error("D9D insufficient training forecasts");
 std::sort(abs_values.begin(),abs_values.end());
 const auto at=[&](double p){return abs_values[static_cast<std::size_t>(p*(abs_values.size()-1))];};
 ForecastThresholds t{at(0.40),at(0.80),abs_values.size()};
 if(!(t.inner>0&&t.outer>t.inner))throw std::runtime_error("D9D degenerate training forecast thresholds");
 return t;
}
struct Score {
 std::size_t sessions=0,decisions=0,opens=0,forced=0;
 std::size_t eligible_long=0,eligible_short=0,flat_choices=0,long_choices=0,short_choices=0;
 std::size_t hold_long=0,close_long=0,hold_short=0,close_short=0;
 std::size_t flat_with_long_available=0,flat_with_long_chosen=0;
 std::size_t learning_updates=0,terminal_updates=0;
 double reward=0,gross_return=0,transaction_cost=0;
};
struct Pending {
 bool valid=false;
 DevD9BObservation obs;
 DevD9APosition pos{DevD9APosition::FLAT};
 DevD9AAction action{DevD9AAction::HOLD_FLAT};
 double entry_action_cost=0;
};
bool hasAction(const std::vector<DevD9AAction>&v,DevD9AAction a){
 return std::find(v.begin(),v.end(),a)!=v.end();
}
void printPolicy(const char*phase,std::size_t trial,const Score&s,std::size_t states){
 std::cout<<"D9C_POLICY_DIAGNOSTICS phase="<<phase<<" trial="<<trial
          <<" sessions="<<s.sessions<<" decisions="<<s.decisions
          <<" q_states="<<states
          <<" eligible_long="<<s.eligible_long<<" eligible_short="<<s.eligible_short
          <<" flat_with_long_available="<<s.flat_with_long_available
          <<" flat_with_long_chosen="<<s.flat_with_long_chosen
          <<" hold_flat="<<s.flat_choices<<" open_long="<<s.long_choices
          <<" open_short="<<s.short_choices<<" hold_long="<<s.hold_long
          <<" close_long="<<s.close_long<<" hold_short="<<s.hold_short
          <<" close_short="<<s.close_short<<" force_exit="<<s.forced
          <<" learning_updates="<<s.learning_updates
          <<" terminal_updates="<<s.terminal_updates<<'\n';
 std::cout<<"D9C_REWARD_DIAGNOSTICS phase="<<phase<<" trial="<<trial
          <<" gross_return_units="<<s.gross_return
          <<" transaction_cost_units="<<s.transaction_cost
          <<" net_reward_units="<<s.reward<<'\n';
}
Score replay(const Sessions&sessions,Key begin,Key end,DevD9BSharedQModel&model,DevD9BConfig cfg,bool learn){
 Score result;
 for(const auto&[day,rows]:sessions){
  if(day*1440<begin||day*1440>=end)continue;
  DevD9BEnvironment env(cfg);
  std::map<std::string,Pending> pending;
  for(const auto&r:rows){
   const auto&obs=r.obs;
   const auto pos=env.position(obs.symbol).side;
   auto legal=env.legalActions(obs);
   if(pos==DevD9APosition::FLAT&&!r.selected){
    legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::OPEN_LONG),legal.end());
    legal.erase(std::remove(legal.begin(),legal.end(),DevD9AAction::OPEN_SHORT),legal.end());
   }
   if(legal.empty())throw std::runtime_error("D9C empty legal actions");
   const bool can_long=hasAction(legal,DevD9AAction::OPEN_LONG);
   const bool can_short=hasAction(legal,DevD9AAction::OPEN_SHORT);
   if(can_long)++result.eligible_long;
   if(can_short)++result.eligible_short;
   if(pos==DevD9APosition::FLAT&&can_long)++result.flat_with_long_available;

   const auto action=model.choose(obs,pos,legal,learn);
   const auto step=env.step(obs,action);
   auto& p=pending[obs.symbol];

   // The previous action earns the price change from its observation to this one.
   // The current action's entry/ordinary-exit cost belongs to the NEXT transition.
   // At mandatory 15:15 termination, however, FORCE_EXIT cost is charged to the
   // previous holding action, since there is no next market observation.
   if(p.valid&&learn){
    const double prior_reward=p.entry_action_cost
       + step.reward + (step.terminal ? 0.0 : step.total_cost);
    model.learn(p.obs,p.pos,p.action,prior_reward,
                obs,pos,legal,step.terminal);
    ++result.learning_updates;
    if(step.terminal)++result.terminal_updates;
   }

   if(action==DevD9AAction::HOLD_FLAT)++result.flat_choices;
   else if(action==DevD9AAction::OPEN_LONG){++result.opens;++result.long_choices;++result.flat_with_long_chosen;}
   else if(action==DevD9AAction::OPEN_SHORT){++result.opens;++result.short_choices;}
   else if(action==DevD9AAction::HOLD_LONG)++result.hold_long;
   else if(action==DevD9AAction::CLOSE_LONG)++result.close_long;
   else if(action==DevD9AAction::HOLD_SHORT)++result.hold_short;
   else if(action==DevD9AAction::CLOSE_SHORT)++result.close_short;
   else if(action==DevD9AAction::FORCE_EXIT)++result.forced;

   result.reward+=step.reward;
   result.transaction_cost+=step.total_cost;
   result.gross_return+=step.reward+step.total_cost;
   ++result.decisions;

   if(step.terminal){
    if(step.position_after!=DevD9APosition::FLAT)
     throw std::runtime_error("D9C 15:15 did not flatten position");
    p.valid=false;
   }else{
    p={true,obs,pos,action,-step.total_cost};
   }
  }
  for(const auto&[symbol,p]:pending){
   if(p.valid||env.position(symbol).side!=DevD9APosition::FLAT)
    throw std::runtime_error("D9C session failed to terminate flat");
  }
  ++result.sessions;
 }
 return result;
}
}
int main(int argc,char**argv){
 try{
  if(argc!=7)throw std::invalid_argument("Usage: run_dev_d9c_shared_rl_training causal.csv TRAIN_START VALIDATION_START OOS_START output_model.txt forecast_on|forecast_off");
  const std::string mode=argv[6];
  if(mode!="forecast_on"&&mode!="forecast_off")throw std::invalid_argument("Mode must be forecast_on or forecast_off");
  const bool forecast_enabled=mode=="forecast_on";
  const Key train=dateKey(argv[2]),val=dateKey(argv[3]),oos=dateKey(argv[4]);if(!(train<val&&val<oos))throw std::invalid_argument("train < validation < OOS required");
  auto rows=load(argv[1]);Diagnostics diag;auto days=sessions(rows,diag,train,val,oos);printDiagnostics(diag);rows.clear();rows.shrink_to_fit();
  if(diag.accepted_train==0||diag.accepted_val==0)throw std::runtime_error("No accepted training or validation sessions; inspect D9C diagnostics before RL optimization");
  // Step 5D never reads OOS forecasts for calibration or training.
  const auto thresholds=forecast_enabled ? calibrateForecast(days,train,val) : ForecastThresholds{};
  std::cout<<std::setprecision(17)<<"D9D_TRAIN_ONLY_THRESHOLDS count="<<thresholds.n
           <<" inner="<<thresholds.inner<<" outer="<<thresholds.outer<<'\n';
  std::cout<<"D9D_MODE "<<mode<<" forecast_feature="<<(forecast_enabled?"enabled":"disabled")<<'\n';
  if(forecast_enabled)forecastAlignmentSuite(days,train,val,oos);
  struct Trial{double alpha,gamma,epsilon;};
  const std::vector<Trial> grid{{0.02,0.90,0.05},{0.02,0.95,0.10},{0.05,0.90,0.05},{0.05,0.95,0.10},{0.10,0.90,0.05},{0.10,0.95,0.10}};
  bool found=false;double best=-std::numeric_limits<double>::infinity();Trial winner{};std::size_t id=0;
  std::cout<<std::fixed<<std::setprecision(8)<<"mode=LONG_ONLY short_enabled=no force_exit=15:15 shared_model=yes\n";
  for(const auto& trial:grid){
   DevD9BConfig cfg;cfg.permissions.enable_long=true;cfg.permissions.enable_short=false;cfg.q_alpha=trial.alpha;cfg.q_gamma=trial.gamma;cfg.epsilon=trial.epsilon;cfg.seed=17;
   cfg.enable_predicted_forward_return=forecast_enabled;
   cfg.forecast_inner_abs=forecast_enabled?thresholds.inner:0;
   cfg.forecast_outer_abs=forecast_enabled?thresholds.outer:0;
   DevD9BSharedQModel model(cfg);auto tr=replay(days,train,val,model,cfg,true);auto v=replay(days,val,oos,model,cfg,false);
   ++id;printPolicy("TRAIN",id,tr,model.stateCount());printPolicy("VALIDATION",id,v,model.stateCount());
   // D9B has no public Q-value accessor. Snapshot through its existing, versioned
   // save interface, inspect without changing the policy, and delete the snapshot.
   const std::string snapshot=std::string(argv[5])+".qdiag.tmp";
   model.save(snapshot);
   QTable q=readQSnapshot(snapshot);
   if(std::remove(snapshot.c_str())!=0)throw std::runtime_error("Cannot remove temporary Q snapshot");
   printQDiag("TRAIN",id,qDiagnostics(days,train,val,q,model,forecast_enabled));
   printQDiag("VALIDATION",id,qDiagnostics(days,val,oos,q,model,forecast_enabled));
   std::cout<<"trial="<<id<<" alpha="<<trial.alpha<<" gamma="<<trial.gamma<<" epsilon="<<trial.epsilon<<" train_sessions="<<tr.sessions<<" train_opens="<<tr.opens<<" val_sessions="<<v.sessions<<" val_opens="<<v.opens<<" val_reward_units="<<v.reward<<'\n';
   if(tr.sessions==0||v.sessions==0||tr.opens<20||v.opens<10)continue;
   const double score=v.reward/static_cast<double>(v.sessions);
   if(!found||score>best){found=true;best=score;winner=trial;}
  }
  if(!found)throw std::runtime_error("No candidate met validation activity requirements; do not select a model");
  DevD9BConfig cfg;cfg.permissions.enable_long=true;cfg.permissions.enable_short=false;cfg.q_alpha=winner.alpha;cfg.q_gamma=winner.gamma;cfg.epsilon=winner.epsilon;cfg.seed=17;
   cfg.enable_predicted_forward_return=forecast_enabled;
   cfg.forecast_inner_abs=forecast_enabled?thresholds.inner:0;
   cfg.forecast_outer_abs=forecast_enabled?thresholds.outer:0;
  DevD9BSharedQModel final_model(cfg);auto fit=replay(days,train,val,final_model,cfg,true);
  printPolicy("TRAIN_FINAL",0,fit,final_model.stateCount());
  final_model.save(argv[5]);
  std::cout<<"selected_alpha="<<winner.alpha<<" selected_gamma="<<winner.gamma<<" selected_epsilon="<<winner.epsilon<<" validation_score_reward_units_per_session="<<best<<'\n';
  std::cout<<"final_training_sessions="<<fit.sessions<<" refit_opens="<<fit.opens<<" shared_states="<<final_model.stateCount()<<" skipped_incomplete_sessions="<<diag.rejected<<" model="<<argv[5]<<'\n';
  std::cout<<"PASS 5C-3I-D9C train/validation grid search; OOS UNTOUCHED (RESEARCH ONLY)\n";
 }catch(const std::exception&e){std::cerr<<"FAIL 5C-3I-D9C: "<<e.what()<<'\n';return 1;}
}
