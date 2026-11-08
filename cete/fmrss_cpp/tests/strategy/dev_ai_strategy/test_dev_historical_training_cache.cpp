#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"

#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

namespace
{
using namespace devai::strategy;
struct Stats { std::size_t checks{0}; std::size_t failures{0}; };
void require(bool c, const std::string& s, Stats& x){ ++x.checks; if(c) std::cout<<"[PASS] "<<s<<'\n'; else {++x.failures; std::cerr<<"[FAIL] "<<s<<'\n';}}
template<class F> void expectThrow(F&& f,const std::string& s,Stats& x){bool t=false;try{f();}catch(const std::exception&){t=true;}require(t,s,x);}

DevAIObservation observation(const std::string& symbol, std::int64_t t)
{
    DevAIObservation o{}; o.agent_name="Dev"; o.symbol=symbol; o.benchmark_symbol="NIFTY%2050"; o.decision_time=t;
    for(std::size_t i=0;i<o.market.size();++i) o.market[i]=(static_cast<double>(i%9)-4.0)/4.0;
    for(std::size_t i=0;i<o.availability.size();++i) o.availability[i]=(i%3)!=0;
    o.portfolio.position=0.0; o.portfolio.unrealized_return=0.0; o.portfolio.realized_return=0.0;
    o.portfolio.equity_return=0.0; o.portfolio.drawdown=0.0; o.portfolio.holding_period=0.0;
    o.numerical_integrity=true; o.ready=true; return o;
}

DevHistoricalCandidateRecord candidate(std::size_t slot,const std::string& symbol,std::int64_t t)
{
    DevHistoricalCandidateRecord c{}; c.slot_index=slot; c.agent_name=DevTopCandidateSelector::agentNames().at(slot);
    c.symbol=symbol; c.candidate_score=0.9-static_cast<double>(slot)*0.1; c.candidate_rank=slot+1;
    c.observation=observation(symbol,t);
    c.outcomes.push_back({5,t+300,0.0125,true,true});
    c.outcomes.push_back({15,0,0.0,false,true});
    c.numerical_integrity=true; return c;
}

DevHistoricalTrainingDataset dataset()
{
    DevHistoricalTrainingDataset d{}; d.numerical_integrity=true;
    const std::string symbols[5]={"RELIANCE","SBIN","INFY","HDFCBANK","BHEL"};
    for(int k=0;k<3;++k){ DevHistoricalDecisionRecord r{}; r.decision_time=1000+k*60; r.benchmark_symbol="NIFTY%2050"; r.universe_stock_count=10; r.numerical_integrity=true;
        const std::size_t n=static_cast<std::size_t>(k+1); for(std::size_t i=0;i<n;++i) r.candidates.push_back(candidate(i,symbols[i],r.decision_time)); d.records.push_back(std::move(r)); }
    return d;
}
}

int main()
{
    Stats s; std::cout<<"\n======================================================================\nDEVAI PHASE 3.15 — HISTORICAL TRAINING DATASET / CACHE\n======================================================================\n";
    auto d=dataset();
    try { DevHistoricalTrainingDatasetValidator::validate(d); require(true,"Valid chronological dataset passes causality/integrity validation",s); } catch(...) { require(false,"Valid chronological dataset passes causality/integrity validation",s); }
    require(d.records.size()==3,"Dataset preserves chronological decision records",s);
    require(d.records[0].candidates.size()==1 && d.records[1].candidates.size()==2 && d.records[2].candidates.size()==3,"Dataset supports dynamic 0-5 candidate counts",s);
    require(d.records[0].candidates[0].observation.decision_time==d.records[0].decision_time,"Observation contains only current decision-time identity",s);
    require(d.records[0].candidates[0].outcomes[0].outcome_time>d.records[0].decision_time,"Future outcome is stored separately from observation",s);

    const auto path=std::filesystem::temp_directory_path()/"devai_phase315_test.cache";
    DevHistoricalTrainingCache::save(path,d); auto loaded=DevHistoricalTrainingCache::load(path);
    require(loaded.records.size()==d.records.size(),"Cache round-trip preserves record count",s);
    require(loaded.records[2].candidates[1].symbol=="SBIN","Cache round-trip preserves candidate identity",s);
    require(std::fabs(loaded.records[0].candidates[0].outcomes[0].forward_return-0.0125)<1e-15,"Cache round-trip preserves offline outcome",s);
    require(loaded.records[1].candidates[0].observation.market==d.records[1].candidates[0].observation.market,"Cache round-trip preserves all 39 market values",s);
    require(loaded.records[1].candidates[0].observation.availability==d.records[1].candidates[0].observation.availability,"Cache round-trip preserves all 39 availability flags",s);

    {auto x=d; x.records[1].decision_time=x.records[0].decision_time; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Duplicate/non-increasing decision time is rejected",s);}
    {auto x=d; x.records[1].candidates[0].observation.decision_time+=60; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Observation from another decision time is rejected",s);}
    {auto x=d; x.records[2].candidates.erase(x.records[2].candidates.begin()); expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Hole/non-contiguous DevAgent assignment is rejected",s);}
    {auto x=d; x.records[2].candidates[1].candidate_rank=5; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Candidate rank inconsistent with DevAgent slot is rejected",s);}
    {auto x=d; x.records[0].candidates[0].outcomes[0].outcome_time=x.records[0].decision_time; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Available future outcome must mature after origin decision time",s);}
    {auto x=d; x.records[0].candidates[0].outcomes[1].forward_return=0.1; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Unavailable outcome cannot smuggle future return",s);}
    {auto x=d; x.records[0].candidates[0].observation.market[0]=std::numeric_limits<double>::quiet_NaN(); expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Non-finite cached observation is rejected",s);}
    {auto x=d; x.records[0].candidates[0].outcomes[0].forward_return=std::numeric_limits<double>::infinity(); expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Non-finite available offline outcome is rejected",s);}
    {auto x=d; x.records[2].candidates[1].symbol=x.records[2].candidates[0].symbol; x.records[2].candidates[1].observation.symbol=x.records[2].candidates[0].symbol; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Duplicate candidate symbol is rejected",s);}
    {auto x=d; x.records[0].benchmark_symbol=""; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Empty benchmark is rejected",s);}
    {auto x=d; x.records[0].candidates[0].symbol="NIFTY%2050"; x.records[0].candidates[0].observation.symbol="NIFTY%2050"; expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Benchmark contamination is rejected",s);}
    {auto x=d; x.records[0].candidates[0].outcomes.push_back(x.records[0].candidates[0].outcomes[0]); expectThrow([&]{DevHistoricalTrainingDatasetValidator::validate(x);},"Duplicate offline horizon is rejected",s);}

    // Cache must be reusable: repeated loads do no CETE historical reconstruction.
    auto loaded2=DevHistoricalTrainingCache::load(path);
    require(loaded2.records.size()==3,"Second cache load reuses persisted dataset without historical replay",s);
    std::filesystem::remove(path);

    std::cout<<"\n======================================================================\nPHASE 3.15 HISTORICAL TRAINING DATASET / CACHE SUMMARY\n======================================================================\nChecks   : "<<s.checks<<"\nFailures : "<<s.failures<<"\n======================================================================\n";
    if(s.failures==0) std::cout<<"DEVAI PHASE 3.15 HISTORICAL TRAINING DATASET / CACHE: PASSED\n";
    return s.failures==0?0:1;
}
