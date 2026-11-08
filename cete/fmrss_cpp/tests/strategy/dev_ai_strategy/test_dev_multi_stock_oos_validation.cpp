#include "devai/strategy/dev_ai_strategy/DevMultiStockOOSValidator.hpp"
#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

using namespace devai::strategy;
namespace {
struct Stats { std::size_t checks{0}, failures{0}; };
void req(bool c,const std::string& m,Stats& s){++s.checks;if(c)std::cout<<"[PASS] "<<m<<'\n';else{++s.failures;std::cerr<<"[FAIL] "<<m<<'\n';}}
template<class F> void throws(F f,const std::string&m,Stats&s){bool x=false;try{f();}catch(const std::exception&){x=true;}req(x,m,s);}

DevAIObservation obs(const std::string& sym,const std::string& agent,std::int64_t t){
    DevAIObservation o{};
    o.agent_name=agent;
    o.symbol=sym;
    o.benchmark_symbol="NIFTY%2050";
    o.decision_time=t;
    o.market.fill(0.0);
    o.availability.fill(1.0);
    o.ready=true;
    o.numerical_integrity=true;
    return o;
}
DevOOSDecisionPoint point(std::int64_t t,std::size_t n){DevOOSDecisionPoint p{};p.decision_time=t;p.benchmark_symbol="NIFTY%2050";p.portfolio.portfolio.position=DEV_POSITION_FLAT;p.portfolio.numerical_integrity=true;p.numerical_integrity=true;static const char* syms[]={"RELIANCE","SBIN","INFY","BHEL","HDFCBANK"};for(std::size_t i=0;i<n;++i){DevOOSCandidate c{};c.assignment.slot_index=i;c.assignment.agent_name=DevTopCandidateSelector::agentNames()[i];c.assignment.symbol=syms[i];c.assignment.decision_time=t;c.assignment.candidate_score=1.0-0.1*i;c.assignment.candidate_rank=(i+1);c.assignment.assigned=true;c.observation=obs(syms[i],c.assignment.agent_name,t);p.candidates.push_back(c);}return p;}
}
int main(){Stats s;std::cout<<"\n======================================================================\nDEVAI PHASE 3.19 — MULTI-STOCK OOS VALIDATION\n======================================================================\n";
try{
 DevQNetwork agent({73117U,true}); DevManagerQNetwork manager({73117U,true}); DevMultiStockOOSValidator v(agent,manager);
 std::vector<DevOOSDecisionPoint> ps{point(1000,5),point(1060,3),point(1120,0)};
 double equity=100000.0; std::size_t calls=0;
 auto exec=[&](const DevOOSDecisionPoint&p,const DevManagerState&,const DevManagerDecision&,const DevPortfolioRotationPlan& plan){DevOOSExecutionResult r{};r.decision_time=p.decision_time;r.equity_before=equity;r.gross_pnl=0.0;r.net_pnl=0.0;r.equity_after=equity;r.drawdown=0.0;r.executed_legs=plan.legs.size();r.numerical_integrity=true;++calls;return r;};
 auto out=v.run(ps,exec);
 req(out.decision_count==3,"OOS validator traverses complete chronological sequence",s);
 req(calls==3,"CETE execution seam is invoked once per decision",s);
 req(out.no_trade_count+out.keep_current_count+out.select_agent_count==3,"Every OOS point produces exactly one Manager decision",s);
 req(out.initial_equity==100000.0 && out.final_equity==100000.0,"CETE equity path reconciles",s);
 req(out.net_pnl==0.0 && out.net_return==0.0,"OOS summary uses supplied CETE economics only",s);
 req(out.numerical_integrity,"OOS summary has numerical integrity",s);
 req(ps[0].candidates.size()==5 && ps[1].candidates.size()==3 && ps[2].candidates.empty(),"OOS contract accepts five contiguous LONG ranks 1..5",s);
 auto bad=ps;bad[0].candidates[4].assignment.candidate_rank=10;throws([&]{v.run(bad,exec);},"OOS validator rejects legacy rank-10 fifth candidate",s);
 bad=ps;bad[1].decision_time=900;throws([&]{v.run(bad,exec);},"OOS validator rejects non-chronological decisions",s);
 bad=ps;bad[0].candidates[0].assignment.symbol="NIFTY%2050";bad[0].candidates[0].observation.symbol="NIFTY%2050";throws([&]{v.run(bad,exec);},"OOS validator rejects benchmark contamination",s);
 auto badexec=[&](const DevOOSDecisionPoint&p,const DevManagerState&,const DevManagerDecision&,const DevPortfolioRotationPlan&){DevOOSExecutionResult r{};r.decision_time=p.decision_time;r.equity_before=100000;r.equity_after=100010;r.gross_pnl=0;r.net_pnl=0;r.numerical_integrity=true;return r;};
 throws([&]{v.run({ps[0]},badexec);},"OOS validator rejects unreconciled CETE economics",s);
 }catch(const std::exception&e){++s.failures;std::cerr<<"[UNEXPECTED] "<<e.what()<<'\n';}
 std::cout<<"\n======================================================================\nPHASE 3.19 MULTI-STOCK OOS VALIDATION SUMMARY\n======================================================================\nChecks   : "<<s.checks<<"\nFailures : "<<s.failures<<"\n======================================================================\nDEVAI PHASE 3.19 MULTI-STOCK OOS VALIDATION: "<<(s.failures?"FAILED":"PASSED")<<"\n";return s.failures?1:0;}
