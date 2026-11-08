#include "devai/strategy/dev_ai_strategy/DevSelectedRLInference_H47_5E2B.hpp"
#include "devai/benchmark/DevMaskablePPOInferenceAdapter_H47_5E2B.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace devai;
struct FakeModel final : benchmark::IDevRLModel {
    benchmark::DevRLCandidate candidate() const noexcept override { return benchmark::DevRLCandidate::MaskablePPO; }
    std::string_view modelName() const noexcept override { return "Test model, not trained PPO"; }
    benchmark::DevRLDecision evaluate(const benchmark::DevRLRequest& r) const override {
        std::array<double,benchmark::kOutputCount> scores{};
        scores[1]=4.0;
        return benchmark::selectLegalAction(r,scores);
    }
};
int main() {
    strategy::DevTopCandidateSelection s{};
    s.decision_time=1792122300; s.benchmark_symbol="NIFTY%2050";
    s.numerical_integrity=true; s.eligible_candidate_count=10; s.selected_candidate_count=5;
    for(std::size_t i=0;i<5;++i) {
        auto& a=s.assignments[i];a.assigned=true;a.slot_index=i;a.candidate_rank=i+1;
        a.agent_name=strategy::DevTopCandidateSelector::agentNames()[i];
        a.symbol="STOCK"+std::to_string(i);a.decision_time=s.decision_time;
    }
    int calls=0;
    strategy::DevSelectedRLInference_H47_5E2B runtime(
        std::make_shared<FakeModel>(),[&](const std::string&,std::int64_t,std::size_t){
            ++calls;benchmark::DevRLRequest r{};r.role=benchmark::DevRLRole::Agent;
            r.legal_actions[0]=true;r.legal_actions[1]=true;return r;
        });
    const auto cycle=runtime.evaluate(s,s.decision_time+60);
    assert(cycle.complete && calls==5 && cycle.reports[0].decision.selected_action==1);
    assert(cycle.candidate==benchmark::DevRLCandidate::MaskablePPO);
    bool rejected=false;
    const auto last_valid=runtime.evaluate(s,s.decision_time+1740);
    assert(last_valid.complete && calls==10);
    try {(void)runtime.evaluate(s,s.decision_time+1800);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected && calls==10);
    rejected=false;
    try {(void)runtime.evaluate(s,s.decision_time+86400);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected && calls==10);
    rejected=false;s.assignments[4].symbol=s.assignments[0].symbol;
    try {(void)runtime.evaluate(s,s.decision_time+60);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected && calls==10);
    // Exercise the real C++ PPO inference adapter with an initialized, UNTRAINED
    // PPO instance. This verifies integration only, not strategy quality.
    auto ppo=std::make_shared<benchmark::DevMaskablePPO>();
    auto adapter=std::make_shared<benchmark::DevMaskablePPOInferenceAdapter_H47_5E2B>(ppo);
    benchmark::DevRLRequest req{};
    req.role=benchmark::DevRLRole::Agent;
    req.legal_actions[0]=true;
    req.legal_actions[1]=true;
    auto ppo_decision=adapter->evaluate(req);
    assert(ppo_decision.valid);
    assert(ppo_decision.selected_action<benchmark::kAgentActionCount);
    assert(req.legal_actions[ppo_decision.selected_action]);
    req.role=benchmark::DevRLRole::Manager;
    req.legal_actions[4]=true;
    auto manager_decision=adapter->evaluate(req);
    assert(manager_decision.valid && req.legal_actions[manager_decision.selected_action]);
    std::cout << "Stage 5E.2B selected-model and PPO C++ adapter PASS (untrained PPO; no orders)\\n";
}
