#include "devai/strategy/dev_ai_strategy/DevRLHistoricalTrainingReadiness.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy;
int main() {
    const std::vector<std::string> universe={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN"};
    const std::int64_t t0=1791517560;
    std::vector<DevUniverseRankingTick> ranks;
    std::vector<DevHistoricalExperience> history;
    for(int i=0;i<3;++i) {
        DevUniverseRankingTick tick{};tick.decision_time=t0+60*i;
        for(std::size_t j=0;j<universe.size();++j) {
            DevUniverseRankedShare share{};share.symbol=universe[j];share.score=static_cast<double>(j);
            share.observation.symbol=share.symbol;
            share.observation.decision_time=tick.decision_time;
            share.observation.ready=true;share.observation.numerical_integrity=true;
            tick.universe.push_back(share);
        }
        ranks.push_back(tick);
    }
    for(int i=0;i<3;++i) {
        DevHistoricalExperience e{};e.decision_time=ranks[i].decision_time;
        for(std::size_t j=0;j<5;++j) {
            const auto& share=ranks[i].universe[9-j];
            DevSharedCandidate c{};c.slot=j;c.candidate_rank=j+1;c.symbol=share.symbol;
            c.candidate_score=share.score;c.observation=share.observation;
            e.candidates.push_back(c);
        }
        e.terminal=(i==2);
        e.equity_before=100000.0+100.0*i;
        e.equity_after=100100.0+100.0*i;
        e.portfolio_reward=0.001;
        if(!e.terminal) {
            e.next_decision_time=ranks[i+1].decision_time;
            e.next_candidates=e.candidates;
            for(auto& c:e.next_candidates)c.observation.decision_time=e.next_decision_time;
        }
        history.push_back(e);
    }
    auto good=DevRLHistoricalTrainingReadiness::validate(ranks,universe,history,t0+240);
    assert(good.ready && good.decisions==3 && good.candidate_assignments==15);
    auto bad=history;bad[1].portfolio_reward=0.05;
    assert(!DevRLHistoricalTrainingReadiness::validate(ranks,universe,bad,t0+240).ready);
    bad=history;bad[0].candidates[0].symbol="ITC";
    assert(!DevRLHistoricalTrainingReadiness::validate(ranks,universe,bad,t0+240).ready);
    bad=history;bad[0].next_candidates[0].observation.decision_time=t0+300;
    assert(!DevRLHistoricalTrainingReadiness::validate(ranks,universe,bad,t0+240).ready);
    bad=history;bad[0].intent={DevRotationAction::BUY,"ITC","DevAgent_001"};
    assert(!DevRLHistoricalTrainingReadiness::validate(ranks,universe,bad,t0+240).ready);
    std::cout<<"PASS 5C-3I-B Phase-4 ranking/historical reward integration contract\n";
}
