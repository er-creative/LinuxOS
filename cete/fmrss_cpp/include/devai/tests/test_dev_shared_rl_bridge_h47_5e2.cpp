#include "devai/strategy/dev_ai_strategy/DevSharedRLInferenceBridge_H47_5E2.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    DevTopCandidateSelection s;
    s.decision_time = 1792123200; s.benchmark_symbol = "NIFTY%2050";
    s.numerical_integrity = true; s.input_candidate_count=10;
    s.eligible_candidate_count=10; s.selected_candidate_count=5;
    for (std::size_t i=0;i<5;++i) {
        auto& a=s.assignments[i];
        a.slot_index=i; a.candidate_rank=i+1;
        a.agent_name=DevTopCandidateSelector::agentNames()[i];
        a.symbol="STOCK"+std::to_string(i); a.decision_time=s.decision_time;
        a.assigned=true;
    }
    int calls=0;
    // Deterministic TEST DOUBLE; does not represent actual trained-model inference.
    DevSharedRLInferenceBridge_H47_5E2 bridge([&](const std::string&, std::int64_t, std::size_t i){
        ++calls; return static_cast<double>(i); });
    auto r=bridge.evaluate(s,s.decision_time+60);
    assert(r.complete && calls==5 && r.reports[4].action_value==4.0);
    assert(r.reports[0].symbol=="STOCK0");
    bool rejected=false;
    try { (void)bridge.evaluate(s,s.decision_time+900); }
    catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected && calls==5);
    rejected=false; s.assignments[4].symbol=s.assignments[0].symbol;
    try { (void)bridge.evaluate(s,s.decision_time+60); }
    catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected && calls==5);
    std::cout << "Stage 5E.2 bridge contract tests PASS (test double only)\n";
}
