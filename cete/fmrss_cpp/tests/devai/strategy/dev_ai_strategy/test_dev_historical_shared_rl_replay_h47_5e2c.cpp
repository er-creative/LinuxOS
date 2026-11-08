#include "devai/strategy/dev_ai_strategy/DevHistoricalSharedRLReplay_H47_5E2C.hpp"
#include <cassert>
#include <iostream>
#include <memory>
using namespace devai;
struct TestModel5E2C final : benchmark::IDevRLModel {
    benchmark::DevRLCandidate candidate() const noexcept override {return benchmark::DevRLCandidate::MaskablePPO;}
    std::string_view modelName() const noexcept override {return "deterministic-test-only";}
    benchmark::DevRLDecision evaluate(const benchmark::DevRLRequest& r) const override {
        std::array<double,benchmark::kOutputCount> v{};v[1]=1.0;
        return benchmark::selectLegalAction(r,v);
    }
};
int main() {
    constexpr std::int64_t refresh=1792123200; // aligned to 15 minutes
    int rankings=0, requests=0;
    strategy::DevSelectedRLInference_H47_5E2B inference(
        std::make_shared<TestModel5E2C>(),
        [&](const std::string&,std::int64_t,std::size_t) {
            ++requests;
            benchmark::DevRLRequest r{};
            r.role=benchmark::DevRLRole::Agent;
            r.legal_actions[0]=true;r.legal_actions[1]=true;
            return r;
        });
    strategy::DevHistoricalSharedRLReplay_H47_5E2C replay(
        inference,[&](std::int64_t t) {
            ++rankings;
            strategy::DevTopCandidateSelection s{};
            s.decision_time=t;s.benchmark_symbol="NIFTY%2050";
            s.numerical_integrity=true;s.input_candidate_count=10;
            s.eligible_candidate_count=10;s.selected_candidate_count=5;
            for(std::size_t i=0;i<5;++i) {
                auto& a=s.assignments[i];a.assigned=true;a.slot_index=i;
                a.candidate_rank=i+1;a.decision_time=t;a.candidate_score=5.0-i;
                a.agent_name=strategy::DevTopCandidateSelector::agentNames()[i];
                a.symbol="STOCK"+std::to_string(i);
            }
            return s;
        });
    // 14 minutes of active inference, then 15:15 mandatory-flat cutoff.
    auto rows=replay.run(refresh+60,refresh+16*60,refresh+15*60);
    assert(rows.size()==15);
    assert(rows[0].status==strategy::DevHistoricalCycleStatus_H47_5E2C::NoFreshRanking);
    assert(rows[13].status==strategy::DevHistoricalCycleStatus_H47_5E2C::NoFreshRanking);
    assert(rows[14].status==strategy::DevHistoricalCycleStatus_H47_5E2C::MandatoryFlatBoundary);
    assert(rankings==0 && requests==0);
    rows=replay.run(refresh,refresh+16*60,refresh+15*60);
    assert(rows.size()==16 && rankings==1 && requests==15*5);
    assert(rows[0].has_inference && rows[14].has_inference);
    assert(rows[15].status==strategy::DevHistoricalCycleStatus_H47_5E2C::MandatoryFlatBoundary);
    for(int i=0;i<15;++i) {
        assert(rows[i].ranking_time==refresh);
        assert(rows[i].inference.complete);
        assert(rows[i].inference.reports[0].decision.selected_action==1);
    }
    std::cout<<"Stage 5E.2C historical replay contract PASS (test model; no orders)\n";
}
