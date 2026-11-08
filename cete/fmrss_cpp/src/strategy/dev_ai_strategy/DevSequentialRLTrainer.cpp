#include "devai/strategy/dev_ai_strategy/DevSequentialRLTrainer.hpp"
#include "devai/strategy/dev_ai_strategy/DevRankTenDirection_F3EF2C5F.hpp"
#include "devai/strategy/dev_ai_strategy/DevDirectionalTraining_F3EF2C5B.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <deque>
#include <fstream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace devai::strategy {
namespace {
namespace dir=f3ef2c5b;
enum class Side { LONG, SHORT };
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Transition {
 DevSharedInput state{},next{};
 std::size_t action{};
 double reward{};
 std::array<bool,DEV_SHARED_OUTPUT> next_mask{};
 bool terminal{},agent{};
};
std::array<bool,DEV_SHARED_OUTPUT> padded(const std::array<bool,4>& small) {
 std::array<bool,DEV_SHARED_OUTPUT> m{};
 for(std::size_t j=0;j<small.size();++j)m[j]=small[j];
 return m;
}
bool shortable(const DevSequentialRLConfig& c,const std::string& symbol) {
 return c.allow_intraday_short && c.shortable_symbols.contains(symbol);
}
DevManagerEncodedState stateFor(const DevHistoricalDecisionRecord& r,const DevOOSLedgerState& p,
                                const DevSharedNeuralNetwork& net,double initial,
                                const DevSequentialRLConfig& c,Side side) {
 DevManagerEncodedState s{};s.decision_time=r.decision_time;s.numerical_integrity=true;
 s.valid_actions[dir::WAIT]=true;
 s.valid_actions[dir::KEEP]=p.quantity!=0 && !dir::mustFlatten(r.decision_time);
 for(const auto& candidate:r.candidates) {
   require(candidate.slot_index<DEV_AGENT_SLOT_COUNT,"invalid agent slot");
   const auto q=net.predictAgent(candidate.observation);
   const std::size_t base=candidate.slot_index*8;
   for(std::size_t j=0;j<4;++j)s.values[base+j]=std::clamp(q.values[j],-10.,10.);
   s.values[base+4]=std::clamp(candidate.candidate_score/100.,-1.,1.);
   s.values[base+5]=1.;s.values[base+6]=double(candidate.slot_index)/4.;s.values[base+7]=1.;
   const bool eligible=side==Side::LONG ||
       (c.allow_intraday_short && c.shortable_symbols.contains(candidate.symbol));
   if((eligible && dir::mayEnter(r.decision_time)) ||
      (p.quantity!=0 && candidate.symbol==p.held_symbol))
     s.valid_actions[dir::FIRST_AGENT+candidate.slot_index]=true;
 }
 s.values[40]=p.quantity>0?1.:p.quantity<0?-1.:0.;
 s.values[41]=(p.equity-initial)/initial;
 s.values[42]=p.peak_equity>0?(p.peak_equity-p.equity)/p.peak_equity:0.;
 // Encode manager direction so the shared policy distinguishes two flat portfolios.
 s.values[43]=(side==Side::LONG?1.:-1.)*double(r.candidates.size())/5.;
 return s;
}
std::array<bool,DEV_SHARED_OUTPUT> directionMask(bool invested,Side side,
    bool eligible,bool entries,bool replacement=false) {
 auto m=padded(dir::agentMask(invested,eligible,side==Side::SHORT,entries));
 if(replacement && entries && eligible) { m[1]=side==Side::LONG; m[2]=side==Side::SHORT; }
 if(replacement) m[3]=false; // EXIT belongs to held symbol, not replacement candidate
 if(side==Side::LONG) m[2]=false;
 else m[1]=false;
 return m;
}
std::size_t choose(const std::array<double,DEV_SHARED_OUTPUT>& q,
                   const std::array<bool,DEV_SHARED_OUTPUT>& mask,double epsilon,std::mt19937& rng) {
 std::vector<std::size_t> valid;
 for(std::size_t a=0;a<mask.size();++a)if(mask[a])valid.push_back(a);
 require(!valid.empty(),"no legal actions");
 if(std::uniform_real_distribution<double>(0.,1.)(rng)<epsilon)
   return valid[std::uniform_int_distribution<std::size_t>(0,valid.size()-1)(rng)];
 return *std::max_element(valid.begin(),valid.end(),[&](auto a,auto b){return q[a]<q[b];});
}
double best(const std::array<double,DEV_SHARED_OUTPUT>& q,const std::array<bool,DEV_SHARED_OUTPUT>& mask) {
 double value=-1e100;
 for(std::size_t a=0;a<mask.size();++a)if(mask[a])value=std::max(value,q[a]);
 require(value>-1e99,"empty next-state action mask");return value;
}
std::string heldTarget(const DevOOSLedgerState& s) {return dir::preserveTarget(s.held_symbol,s.quantity);}
double equityAt(const DevOOSLedgerState& s,std::int64_t time,const DevOOSPriceProvider& provider) {
 return dir::markedEquity(s.cash,s.quantity,s.quantity?provider(s.held_symbol,time):0.);
}
} // namespace

DevSequentialRLTrainer::DevSequentialRLTrainer(DevSequentialRLConfig c):cfg_(std::move(c)){}
DevSequentialRLResult DevSequentialRLTrainer::run() const {
 const auto& c=cfg_;
 require(c.epochs>0 && c.epochs<=100 && c.batch_size>0 && c.replay_capacity>=c.batch_size &&
         c.warmup>=c.batch_size && c.warmup<=c.replay_capacity && c.target_sync>0,"invalid training sizes");
 require(std::isfinite(c.gamma)&&c.gamma>=0&&c.gamma<1&&
         std::isfinite(c.learning_rate)&&c.learning_rate>0&&c.learning_rate<=.01&&
         
         std::isfinite(c.initial_capital)&&c.initial_capital>0&&
         std::isfinite(c.epsilon_start)&&std::isfinite(c.epsilon_end)&&
         c.epsilon_start>=c.epsilon_end&&c.epsilon_end>=0&&c.epsilon_start<=1&&
         std::isfinite(c.brokerage_rate)&&c.brokerage_rate>=0&&
         std::isfinite(c.slippage_rate)&&c.slippage_rate>=0&&
         std::isfinite(c.other_cost_rate)&&c.other_cost_rate>=0,
         "invalid training hyperparameters");
 require(c.brokerage_rate==0.0 && c.slippage_rate==0.0 && c.other_cost_rate==0.0,
         "5E2K4: RL training must be cost-free; use a separate realistic-cost evaluation");
 if(c.allow_intraday_short)require(!c.shortable_symbols.empty(),"SHORT requires explicitly shortable symbols");
 require(!c.validation_cache.empty(),"explicit validation cache required");
 require(c.cache!=c.validation_cache,"train and validation paths must differ");
 require(c.checkpoint!=c.cache && c.checkpoint!=c.validation_cache,"checkpoint path collides with cache");
 const auto data=DevHistoricalTrainingCache::load(c.cache);
 const auto validation=DevHistoricalTrainingCache::load(c.validation_cache);
 DevHistoricalTrainingDatasetValidator::validate(data);
 require(data.records.size()>=20,"need >=20 historical records");
 const std::size_t boundary=data.records.size();
 const auto training_started=std::chrono::steady_clock::now();
 std::cout<<"[CETE][RL][START] epochs="<<c.epochs
          <<" train_records="<<boundary
          <<" validation_records="<<validation.records.size()
          <<" learning_rate="<<c.learning_rate<<std::endl;
 std::cout<<"[CETE][RL][CONFIG] manager=DevManager_001 agents=5"
          <<" shared_model=DevSharedNeuralNetwork"
          <<" allow_short="<<c.allow_intraday_short
          <<" initial_capital="<<c.initial_capital
          <<" brokerage="<<c.brokerage_rate
          <<" slippage="<<c.slippage_rate
          <<" other_cost="<<c.other_cost_rate
          <<" epsilon_start="<<c.epsilon_start
          <<" epsilon_end="<<c.epsilon_end<<std::endl;
 std::cout<<"[CETE][RL][COST_MODE] ZERO_COST (training only)"<<std::endl;
 require(validation.records.size()>=3,"validation cache too small");
 require(data.records.back().decision_time<validation.records.front().decision_time,
         "train and validation chronology overlaps");
 require(data.records.front().benchmark_symbol==validation.records.front().benchmark_symbol,
         "train and validation benchmarks differ");
 std::set<std::string> symbols;
 for(const auto& r:data.records)for(const auto& x:r.candidates)symbols.insert(x.symbol);
 for(const auto& r:validation.records)for(const auto& x:r.candidates)symbols.insert(x.symbol);
 for(const auto& sym:c.shortable_symbols)require(symbols.contains(sym),"shortable symbol not present in dataset");
 DevHistoricalCloseStore prices;
 for(const auto& sym:symbols)prices.loadMinuteFile(c.minute_folder/(sym+"_1min.txt"),sym);
 DevSharedNeuralNetwork online(c.seed),target(c.seed);
 DevOOSLedgerConfig lc{};lc.initial_capital=c.initial_capital;lc.max_allocation_fraction=.75;
 lc.cost_mode=DevOOSCostMode::ZERO_COST;
 lc.brokerage_rate=0.;lc.slippage_rate=0.;lc.other_cost_rate=0.;
 lc.allow_intraday_short=c.allow_intraday_short;
 std::mt19937 rng(c.seed);std::deque<Transition> buffer;
 DevSequentialRLResult result{};double sum_loss=0;std::size_t losses=0,updates=0;
 const DevOOSPriceProvider provider=[&](const std::string& s,std::int64_t t){return prices.completedClose(s,t);};
 const DevOOSPriceProvider execution=[&](const std::string& s,std::int64_t t){return prices.openingPrice(s,t);};
 for(std::size_t epoch=0;epoch<c.epochs;++epoch) {
   const auto epoch_started=std::chrono::steady_clock::now();
   std::cout<<"[CETE][RL][EPOCH] "<<(epoch+1)<<"/"<<c.epochs<<" START"<<std::endl;
   // Both portfolios start independently at 7000, but update ONE shared model.
   DevRealHistoricalMultiStockOOS long_ledger(lc);
   DevRealHistoricalMultiStockOOS short_ledger(lc);
   std::size_t session_number=0, session_entries=0, session_forced=0;
   double session_start_equity=long_ledger.state().equity;
   double session_brokerage=0., session_slippage=0., session_other=0.;
   const auto emit_session=[&](std::int64_t timestamp) {
     const auto& state=long_ledger.state();
     require(state.quantity==0,"session ended with open LONG position");
     require(std::isfinite(state.equity) && std::isfinite(state.cash),
             "session ended with non-finite portfolio state");
     const double costs=session_brokerage+session_slippage+session_other;
     std::cout<<"[CETE][RL][SESSION] epoch="<<(epoch+1)
              <<" index="<<(++session_number)
              <<" last_decision_epoch="<<timestamp
              <<" start_equity="<<std::setprecision(12)<<session_start_equity
              <<" end_equity="<<state.equity
              <<" net_change="<<(state.equity-session_start_equity)
              <<" entries="<<session_entries
              <<" forced_exits="<<session_forced
              <<" brokerage="<<session_brokerage
              <<" slippage="<<session_slippage
              <<" other_costs="<<session_other
              <<" costs="<<costs
              <<" overnight_quantity="<<state.quantity<<std::endl;
     session_start_equity=state.equity;
     session_entries=0;session_forced=0;
     session_brokerage=0.;session_slippage=0.;session_other=0.;
   };
   for(std::size_t i=0;i+1<boundary;++i) {
     const auto& r=data.records[i];const auto& n=data.records[i+1];
     if(i>0 && i%5000==0) {
       const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now()-epoch_started).count();
       const double percent=100.0*static_cast<double>(i)/static_cast<double>(boundary-1);
       std::cout<<"[CETE][RL][PROGRESS] epoch="<<(epoch+1)<<"/"<<c.epochs
                <<" records="<<i<<"/"<<(boundary-1)
                <<" percent="<<std::fixed<<std::setprecision(1)<<percent<<"%"
                <<" elapsed="<<elapsed<<"s"
                <<" replay="<<buffer.size()
                <<" manager_updates="<<result.manager_updates
                <<" agent_updates="<<result.agent_updates<<std::endl;
     }
     if(n.decision_time!=r.decision_time+60) {
      // The preceding contiguous transition already liquidated at its final
      // available timestamp and included execution costs in its reward.
      // Never silently liquidate outside a recorded reward transition.
      require(long_ledger.state().quantity==0 && short_ledger.state().quantity==0,
              "position crossed missing-minute/session boundary");
      emit_session(r.decision_time);
      continue;
    }
    for(const Side side : {Side::LONG,Side::SHORT}) {
       if(side==Side::SHORT && !c.allow_intraday_short) continue;
       auto& ledger=side==Side::LONG?long_ledger:short_ledger;
       const auto prev=ledger.state();
       require(side==Side::LONG?prev.quantity>=0:prev.quantity<=0,
               "portfolio direction mismatch");
       const double before=equityAt(prev,r.decision_time,provider);
       const auto ms=stateFor(r,prev,online,c.initial_capital,c,side);
       const auto encoded=online.encodeManager(ms);
       const double fraction=double(epoch*boundary+i)/double(c.epochs*boundary);
       const double epsilon=c.epsilon_start+(c.epsilon_end-c.epsilon_start)*fraction;
       const std::size_t action=choose(online.predict(encoded),ms.valid_actions,epsilon,rng);
       // WAIT is the explicit FLAT target. KEEP preserves a held position.
       // This also permits EXIT if the held symbol drops out of the top five.
       std::string desired=(action==dir::WAIT)?std::string{}:heldTarget(prev);
       std::size_t selected_direction=0;
       using Candidate=typename std::decay_t<decltype(r.candidates)>::value_type;
       const Candidate* selected=nullptr;
       if(action>=dir::FIRST_AGENT) {
         auto it=std::find_if(r.candidates.begin(),r.candidates.end(),
            [&](const auto& x){return x.slot_index==action-dir::FIRST_AGENT;});
         require(it!=r.candidates.end(),"missing selected agent slot");
         const auto mask=directionMask(prev.quantity!=0,side,
            (side==Side::LONG || shortable(c,it->symbol)),dir::mayEnter(r.decision_time),
            prev.quantity!=0 && it->symbol!=prev.held_symbol);
         selected_direction=choose(online.predict(online.encodeAgent(it->observation)),mask,epsilon,rng);
         if(selected_direction==1 || selected_direction==2) {
           desired=dir::targetForEntry(it->symbol,selected_direction);
           selected=&*it;
         } else if(selected_direction==3 && prev.quantity!=0) {
           desired.clear();selected=&*it;
         }
       }
       const bool next_gap=(i+2<boundary &&
          data.records[i+2].decision_time!=n.decision_time+60);
      const bool terminal=(i+2>=boundary || next_gap);
      // At a decision already beyond cutoff, no agent choice is credited.
      const bool forced_now=dir::mustFlatten(r.decision_time);
      if(forced_now) { desired.clear();selected=nullptr;selected_direction=0; }
      require(side==Side::LONG?desired.rfind("SHORT:",0)!=0:
           (desired.empty() || desired.rfind("SHORT:",0)==0),
           "cross-direction order blocked");
       const auto fill=ledger.execute(r.decision_time,desired,execution);
       require(fill.numerical_integrity,"ledger execution integrity failure");
       if(side==Side::LONG) {
         session_brokerage+=fill.brokerage;
         session_slippage+=fill.slippage;
         session_other+=fill.other_costs;
       }
       const auto after_state=ledger.state();
       require(side==Side::LONG?after_state.quantity>=0:after_state.quantity<=0,
               "cross-direction position blocked");
       if(prev.quantity==0&&after_state.quantity>0) {
         ++result.long_entries;
         ++session_entries;
       }
       if(prev.quantity==0&&after_state.quantity<0)++result.short_entries;
       // The last contiguous decision is a terminal boundary. Liquidate at
      // its NEXT known timestamp, not earlier, and include both exit costs
      // and the holding-period P&L in the transition reward.
      if(terminal || dir::mustFlatten(n.decision_time)) {
        if(ledger.state().quantity!=0) {
          const auto close=ledger.execute(n.decision_time,{},execution);
          require(close.numerical_integrity && ledger.state().quantity==0,
                  "terminal liquidation failed");
           ++result.forced_exits;
           if(side==Side::LONG) {
             ++session_forced;
             session_brokerage+=close.brokerage;
             session_slippage+=close.slippage;
             session_other+=close.other_costs;
           }
        }
      }
      const auto final_state=ledger.state();
      const double after=equityAt(final_state,n.decision_time,provider);
      const double reward=std::clamp((after-before)/c.initial_capital*100.,-10.,10.);
      require(std::isfinite(reward),"nonfinite reward");
      // A mandatory liquidation at the CURRENT timestamp is not an RL
      // action. Its costs were captured by the preceding transition.
      if(!forced_now) {
        const auto nextstate=stateFor(n,final_state,online,c.initial_capital,c,side);
        Transition t{};t.state=encoded;t.next=online.encodeManager(nextstate);
        t.action=action;t.reward=reward;t.next_mask=nextstate.valid_actions;
        t.terminal=terminal || dir::mustFlatten(n.decision_time);
        buffer.push_back(t);if(buffer.size()>c.replay_capacity)buffer.pop_front();
        ++result.train_decisions;
        if(selected) {
          auto next_it=std::find_if(n.candidates.begin(),n.candidates.end(),
             [&](const auto& x){return x.symbol==selected->symbol;});
          Transition a{};a.agent=true;a.state=online.encodeAgent(selected->observation);
          a.next=(next_it==n.candidates.end())?DevSharedInput{}:
              online.encodeAgent(next_it->observation);
          a.action=selected_direction;
          a.reward=reward;
          a.next_mask=directionMask(final_state.quantity!=0,side,
              (side==Side::LONG || shortable(c,selected->symbol)),dir::mayEnter(n.decision_time));
          a.terminal=t.terminal || next_it==n.candidates.end();
          buffer.push_back(a);if(buffer.size()>c.replay_capacity)buffer.pop_front();
        }
      }
      if(buffer.size()<c.warmup)continue;
       for(std::size_t k=0;k<c.batch_size;++k) {
         const auto& sample=buffer[std::uniform_int_distribution<std::size_t>(0,buffer.size()-1)(rng)];
         const auto future=sample.terminal?0.:best(target.predict(sample.next),sample.next_mask);
         const auto td=std::clamp(sample.reward+c.gamma*future,-10.,10.);
         sum_loss+=online.train(sample.state,sample.action,td,c.learning_rate);
         ++losses;if(sample.agent)++result.agent_updates;else ++result.manager_updates;
         if(++updates%c.target_sync==0)target.setParameters(online.parameters());
       }
     }
   }
   // The final decision is not processed by the i+1 transition loop.
   // Training portfolio must be flat at the end of every epoch.
   require(long_ledger.state().quantity==0 && short_ledger.state().quantity==0,
           "training epoch ended with open position");
   emit_session(data.records.back().decision_time);
   result.last_training_equity=long_ledger.state().equity;
   result.last_short_training_equity=short_ledger.state().equity;
   const auto epoch_seconds=std::chrono::duration_cast<std::chrono::seconds>(
       std::chrono::steady_clock::now()-epoch_started).count();
   std::cout<<"[CETE][RL][EPOCH] "<<(epoch+1)<<"/"<<c.epochs
            <<" COMPLETE elapsed="<<epoch_seconds<<"s"
            <<" equity="<<result.last_training_equity
            <<" replay="<<buffer.size()<<std::endl;
 }
 result.replay_transitions=buffer.size();result.mean_td_loss=losses?sum_loss/losses:0.;
 require(std::isfinite(result.mean_td_loss),"non-finite mean training TD loss");
 std::cout<<"[CETE][RL][TRAINING_LOSS] samples="<<losses
          <<" mean_td_loss="<<std::scientific<<std::setprecision(10)
          <<result.mean_td_loss<<std::defaultfloat<<std::endl;
 require(result.manager_updates>0&&result.agent_updates>0,"both roles must receive updates");
 // Held-out diagnostic: frozen weights, no optimizer updates. This is NOT portfolio OOS P&L.
 std::cout<<"[CETE][RL][VALIDATION] START records="<<validation.records.size()<<std::endl;
 double vl=0;std::size_t vn=0;
 for(std::size_t i=0;i+1<validation.records.size();++i) {
   const auto& r=validation.records[i],&n=validation.records[i+1];
   if(n.decision_time!=r.decision_time+60)continue;
   for(const auto& x:r.candidates) {
     auto it=std::find_if(n.candidates.begin(),n.candidates.end(),
        [&](const auto& y){return y.symbol==x.symbol;});
     if(it==n.candidates.end())continue;
     const double p0=provider(x.symbol,r.decision_time),p1=provider(x.symbol,n.decision_time);
     const double raw=(p1/p0-1.)*100.;
     const auto s=online.encodeAgent(x.observation),ns=online.encodeAgent(it->observation);
     // Evaluate each direction under its own legal action mask.
     for(const Side side : {Side::LONG,Side::SHORT}) {
       if(side==Side::SHORT && !c.allow_intraday_short) continue;
       if(side==Side::SHORT && !shortable(c,x.symbol)) continue;
       const auto mask=directionMask(false,side,(side==Side::LONG || shortable(c,x.symbol)),
                                    dir::mayEnter(n.decision_time));
     const double future=c.gamma*best(online.predict(ns),mask);
     const std::size_t action=side==Side::LONG?1:2;
     const double reward=std::clamp(action==1?raw:-raw,-10.,10.);
     const double diff=online.predict(s)[action]-(reward+future);
     vl+=.5*diff*diff;++vn;
     }
   }
 }
 result.validation_td_loss=vn?vl/vn:0.;
 require(vn>0,"validation has no eligible TD comparisons");
 require(std::isfinite(result.validation_td_loss),"non-finite validation TD loss");
 std::cout<<"[CETE][RL][VALIDATION] COMPLETE comparisons="<<vn
          <<" td_loss="<<std::scientific<<std::setprecision(10)
          <<result.validation_td_loss<<std::defaultfloat<<std::endl;
 if(!c.checkpoint.parent_path().empty())std::filesystem::create_directories(c.checkpoint.parent_path());
 online.save(c.checkpoint);
 DevSharedNeuralNetwork reload(c.seed+1);reload.load(c.checkpoint);
 require(reload.parameters().w1==online.parameters().w1 &&
         reload.parameters().w2==online.parameters().w2 &&
         reload.parameters().b1==online.parameters().b1 &&
         reload.parameters().b2==online.parameters().b2,
         "checkpoint reload mismatch");
 // Sidecar documents training configuration; old V1 checkpoints have no SHORT certification.
 // Consumers MUST check it before setting trained_shared_model_supports_short=true.
 std::ofstream meta(c.checkpoint.string()+".direction.txt",std::ios::trunc);
 require(bool(meta),"cannot write directional checkpoint metadata");
 meta<<"CETE_F3E_F2C5F_DIRECTION_V2\n";
 meta<<"allow_short="<<(c.allow_intraday_short?1:0)<<"\n";
 meta<<"training_cache="<<c.cache.string()<<"\n";
 meta<<"validation_cache="<<c.validation_cache.string()<<"\n";
 meta<<"training_records="<<data.records.size()<<"\n";
 meta<<"validation_records="<<validation.records.size()<<"\n";
 meta<<std::setprecision(17);
 meta<<"mean_td_loss="<<result.mean_td_loss<<"\n";
 meta<<"validation_td_loss="<<result.validation_td_loss<<"\n";
 meta<<"cost_mode=ZERO_COST\n";
 meta<<"brokerage_rate="<<c.brokerage_rate<<"\n";
 meta<<"slippage_rate="<<c.slippage_rate<<"\n";
 meta<<"other_cost_rate="<<c.other_cost_rate<<"\n";
 meta<<"short_entries="<<result.short_entries<<"\n";
 // Retain the V2 directional header for compatibility with existing consumers.
 // This training configuration uses one manager and five agents, not two managers.
 meta<<"manager=DevManager_001\n";
 meta<<"agent_count=5\n";
 meta<<"shared_model=DevSharedNeuralNetwork\n";
 meta<<"baseline_algorithm=DQN_style\n";
 meta<<"dual_manager_shared_parameters=0\n";
 meta<<"long_manager=DevManager_001\n";
 meta<<"shortable_symbols=";
 for(const auto& sym:c.shortable_symbols)meta<<sym<<',';
 meta<<"\n";
 require(bool(meta),"failed to write directional checkpoint metadata");
 meta.flush();
 require(bool(meta),"checkpoint metadata flush failed");
 const auto total_seconds=std::chrono::duration_cast<std::chrono::seconds>(
     std::chrono::steady_clock::now()-training_started).count();
 std::cout<<"[CETE][RL][CHECKPOINT] saved_and_verified="<<c.checkpoint.string()<<std::endl;
 std::cout<<"[CETE][RL][COMPLETE] elapsed="<<total_seconds<<"s"
          <<" manager_updates="<<result.manager_updates
          <<" agent_updates="<<result.agent_updates
          <<" validation_td_loss="<<std::scientific<<std::setprecision(10)
          <<result.validation_td_loss<<std::defaultfloat<<std::endl;
 return result;
}
}
