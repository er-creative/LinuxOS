#include "devai/strategy/dev_ai_strategy/DevHistoricalInferenceAudit_H47_5E2E.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
template<class F> bool fails(F&& f) {
    try{f();}catch(const std::exception&){return true;}return false;
}
int main() {
    constexpr std::int64_t t=1792123200;
    DevHistoricalInferenceProvenance_H47_5E2E p{
        "test-checkpoint","test-encoder","test-cache","test-ranking",
        true,true,true,true
    }; // TEST ASSERTIONS ONLY; not evidence of a validated real checkpoint.
    DevHistoricalInferenceRow_H47_5E2C row{};
    row.decision_time=t;row.ranking_time=t;
    row.status=DevHistoricalCycleStatus_H47_5E2C::InferenceComplete;
    row.has_inference=true;
    row.inference.complete=true;
    row.inference.decision_time=t;row.inference.ranking_time=t;
    for(std::size_t i=0;i<DEV_AGENT_SLOT_COUNT;++i){
        auto& r=row.inference.reports[i];
        r.agent_name=DevTopCandidateSelector::agentNames()[i];
        r.symbol="STOCK"+std::to_string(i);
        r.decision_time=t;r.candidate_rank=i+1;
        r.decision.valid=true;r.decision.selected_action=0;
    }
    auto flat=row;flat.decision_time=t+60;
    flat.status=DevHistoricalCycleStatus_H47_5E2C::MandatoryFlatBoundary;
    flat.has_inference=false;
    std::vector<DevHistoricalInferenceRow_H47_5E2C> rows{row,flat};
    const auto summary=DevHistoricalInferenceAudit_H47_5E2E::validate(rows,t+60,p);
    assert(summary.minutes==2 && summary.inferred_minutes==1);
    assert(summary.agent_reports==5 && summary.flat_minutes==1);
    auto bad=p;bad.checkpoint_validated=false;
    assert(fails([&]{(void)DevHistoricalInferenceAudit_H47_5E2E::validate(rows,t+60,bad);}));
    rows[1].has_inference=true;
    assert(fails([&]{(void)DevHistoricalInferenceAudit_H47_5E2E::validate(rows,t+60,p);}));
    rows[1].has_inference=false;
    rows[0].inference.reports[1].symbol=rows[0].inference.reports[0].symbol;
    assert(fails([&]{(void)DevHistoricalInferenceAudit_H47_5E2E::validate(rows,t+60,p);}));
    std::cout<<"Stage 5E.2E inference audit contract PASS (synthetic fixture)\n";
}
