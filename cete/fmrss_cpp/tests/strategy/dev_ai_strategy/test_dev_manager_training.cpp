#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;
struct Stats{std::size_t checks{0},failures{0};};
void require(bool c,const std::string& s,Stats& x){++x.checks;if(c)std::cout<<"[PASS] "<<s<<'\n';else{++x.failures;std::cerr<<"[FAIL] "<<s<<'\n';}}
template<class F>void expectThrow(F&&f,const std::string&s,Stats&x){bool t=false;try{f();}catch(const std::exception&){t=true;}require(t,s,x);}

DevManagerPortfolioState flatPortfolio(){DevManagerPortfolioState p{};p.portfolio.position=DEV_POSITION_FLAT;p.portfolio.drawdown=.01;p.numerical_integrity=true;return p;}
DevManagerPortfolioState longPortfolio(const std::string&s){auto p=flatPortfolio();p.current_symbol=s;p.portfolio.position=DEV_POSITION_LONG;p.portfolio.unrealized_return=.01;p.portfolio.holding_period=4;p.numerical_integrity=true;return p;}
DevAgentReport report(std::size_t slot,const std::string&sym,std::int64_t t,double score){DevAgentReport r{};r.slot_index=slot;r.agent_name=DevTopCandidateSelector::agentNames()[slot];r.assigned=true;r.symbol=sym;r.decision_time=t;r.candidate_score=score;r.candidate_rank=slot+1;r.q_values.values={.1,.4,.2,-.1};r.q_values.numerical_integrity=true;r.preferred_action=DevAction::LONG;r.preferred_q_value=.4;r.numerical_integrity=true;return r;}
std::vector<DevAgentReport> reports(std::size_t n,std::int64_t t){const std::array<std::string,5>s{"RELIANCE","SBIN","BHEL","INFY","HDFCBANK"};std::vector<DevAgentReport>v;for(std::size_t i=0;i<n;++i)v.push_back(report(i,s[i],t,.95-.05*i));return v;}
DevManagerState state(DevManager&m,std::size_t n,std::int64_t t,bool holding=false){return m.buildState(t,reports(n,t),holding?longPortfolio("RELIANCE"):flatPortfolio());}
bool same(const DevManagerNetworkParameters&a,const DevManagerNetworkParameters&b){return a.weights1==b.weights1&&a.biases1==b.biases1&&a.weights2==b.weights2&&a.biases2==b.biases2&&a.weights3==b.weights3&&a.biases3==b.biases3;}

DevManagerTransition transition(DevManager&m,std::int64_t t,double reward){DevManagerTransition x{};x.state=state(m,5,t,false);x.action=DevManagerAction::NO_TRADE;x.reward=reward;x.next_state=state(m,5,t+60,false);x.terminal=false;x.decision_time=t;x.next_decision_time=t+60;x.numerical_integrity=true;return x;}
}

int main(){
    using namespace devai::strategy; Stats s{}; DevManager manager; DevManagerStateEncoder encoder;
    std::cout<<"\n======================================================================\nDEVAI PHASE 3.17 — DEVMANAGER TRAINING\n======================================================================\n";

    for(std::size_t n=0;n<=5;++n){auto st=state(manager,n,1000+static_cast<std::int64_t>(n)*60,false);auto e=encoder.encode(st);require(e.numerical_integrity,"Manager encoder accepts "+std::to_string(n)+" assigned DevAgents",s);require(e.valid_actions[0],"NO_TRADE valid with "+std::to_string(n)+" assigned DevAgents",s);require(!e.valid_actions[1],"KEEP_CURRENT masked while flat for "+std::to_string(n)+" assigned DevAgents",s);bool slots=true;for(std::size_t i=0;i<5;++i)slots=slots&&(e.valid_actions[2+i]==(i<n));require(slots,"SELECT_AGENT mask exactly follows assigned/EMPTY slots for "+std::to_string(n)+" reports",s);}

    auto hold=encoder.encode(state(manager,3,2000,true));require(hold.valid_actions[1],"KEEP_CURRENT becomes valid only with objective CETE holding",s);
    require(DEV_MANAGER_INPUT_SIZE==62,"Manager state has frozen 62-value input",s);
    DevManagerQNetwork net;require(net.parameterCount()==2663,"Manager Q-network owns expected 2,663 parameters",s);
    auto q=net.predict(encoder.encode(state(manager,5,2060,false)));require(q.numerical_integrity,"Manager Q-network inference has numerical integrity",s);

    DevManagerTrainingConfig cfg{};cfg.replay_capacity=16;cfg.minimum_replay_size=8;cfg.batch_size=8;cfg.target_sync_interval=2;cfg.epsilon_start=0.0;cfg.epsilon_end=0.0;cfg.epsilon_decay_selections=10;
    DevManagerTrainer trainer(cfg);
    require(same(trainer.onlineNetwork().parameters(),trainer.targetNetwork().parameters()),"Training initialization hard-synchronizes Manager target network",s);
    require(!trainer.canTrain(),"Manager trainer waits for replay warm-up",s);
    for(int i=0;i<8;++i)trainer.addExperience(transition(manager,3000+i*120,.001*(i+1)));
    require(trainer.replaySize()==8&&trainer.canTrain(),"Causal Manager transitions enter replay and satisfy warm-up",s);
    const auto before=trainer.onlineNetwork().parameters();const auto u1=trainer.trainOneUpdate();
    require(u1.numerical_integrity,"First Manager mini-batch update has numerical integrity",s);require(u1.batch_size==8,"First Manager update uses configured batch size",s);require(u1.completed_training_updates==1,"First Manager update increments global training counter",s);require(!u1.target_synchronized,"Manager target is not synchronized before configured interval",s);require(!same(before,trainer.onlineNetwork().parameters()),"Manager online Q-network changes after training",s);
    const auto u2=trainer.trainOneUpdate();require(u2.completed_training_updates==2&&u2.target_synchronized,"Second Manager update reaches target synchronization boundary",s);require(same(trainer.onlineNetwork().parameters(),trainer.targetNetwork().parameters()),"Manager target equals online network after synchronization",s);

    auto zero=state(manager,0,5000,false);auto d0=trainer.selectDecision(zero,manager);require(d0.manager_decision.type==DevManagerDecisionType::NO_TRADE,"With zero candidates and flat portfolio Manager can only choose NO_TRADE",s);
    auto five=state(manager,5,5060,false);auto d5=trainer.selectDecision(five,manager);require(d5.numerical_integrity&&d5.manager_decision.numerical_integrity,"Manager policy materializes a validated architecture decision",s);
    const auto selections=trainer.completedActionSelections();trainer.resetEpisode();require(trainer.completedActionSelections()==selections,"Manager episode reset preserves global epsilon schedule",s);

    auto bad=transition(manager,6000,0.0);bad.reward=std::numeric_limits<double>::quiet_NaN();expectThrow([&]{trainer.addExperience(bad);},"Manager trainer rejects non-finite economic reward",s);
    bad=transition(manager,6120,0.0);bad.action=DevManagerAction::KEEP_CURRENT;expectThrow([&]{trainer.addExperience(bad);},"Manager trainer rejects action masked by causal state",s);
    bad=transition(manager,6240,0.0);bad.next_decision_time=bad.decision_time;expectThrow([&]{trainer.addExperience(bad);},"Manager trainer rejects non-forward transition chronology",s);

    const auto learned=trainer.onlineNetwork().parameters();trainer.clearExperienceAndResetTraining();require(trainer.replaySize()==0,"Explicit Manager training reset clears replay",s);require(trainer.completedTrainingUpdates()==0&&trainer.completedActionSelections()==0,"Explicit Manager training reset clears global counters",s);require(same(learned,trainer.onlineNetwork().parameters()),"Explicit Manager training reset preserves learned online parameters",s);require(same(trainer.onlineNetwork().parameters(),trainer.targetNetwork().parameters()),"Explicit Manager training reset re-synchronizes target",s);

    DevManagerTrainingConfig badcfg=cfg;badcfg.batch_size=9;badcfg.minimum_replay_size=8;expectThrow([&]{DevManagerTrainer x(badcfg);},"Manager trainer rejects batch larger than replay warm-up threshold",s);
    DevManagerTrainer fresh(cfg);expectThrow([&]{(void)fresh.trainOneUpdate();},"Manager trainer rejects update before replay warm-up",s);

    std::cout<<"\n======================================================================\nPHASE 3.17 DEVMANAGER TRAINING SUMMARY\n======================================================================\nChecks   : "<<s.checks<<"\nFailures : "<<s.failures<<"\n======================================================================\n";
    if(s.failures==0){std::cout<<"DEVAI PHASE 3.17 DEVMANAGER TRAINING: PASSED\n";return 0;}std::cout<<"DEVAI PHASE 3.17 DEVMANAGER TRAINING: FAILED\n";return 1;
}
