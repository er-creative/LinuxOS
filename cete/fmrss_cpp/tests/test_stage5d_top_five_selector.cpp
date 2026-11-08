#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main() {
    std::vector<DevCandidateScore> candidates;
    for (int i=0;i<10;++i) {
        DevCandidateScore c;
        c.symbol="STOCK"+std::to_string(i+1);
        c.decision_time=1792123200;
        c.score=100.0-i;
        c.eligible=true;
        c.numerical_integrity=true;
        candidates.push_back(c);
    }
    for (std::size_t count : {std::size_t{10},std::size_t{5},std::size_t{4},std::size_t{1}}) {
        auto sample=candidates;
        sample.resize(count);
        auto result=DevTopCandidateSelector{}.select(sample,"NIFTY%2050");
        assert(result.selected_candidate_count==std::min(count,std::size_t{5}));
        for(std::size_t i=0;i<5;++i) {
            const auto& a=result.assignments[i];
            assert(a.slot_index==i);
            assert(a.agent_name=="DevAgent_00"+std::to_string(i+1));
            if(i<count) {
                assert(a.assigned);
                assert(a.candidate_rank==i+1);
                assert(a.symbol=="STOCK"+std::to_string(i+1));
            } else assert(!a.assigned);
        }
    }
    // Verify deterministic alphabetical tie-break for equal scores.
    auto tied=candidates;
    tied[0].score=tied[1].score;
    auto result=DevTopCandidateSelector{}.select(tied,"NIFTY%2050");
    assert(result.assignments[0].symbol=="STOCK1");
    assert(result.assignments[4].candidate_rank==5);
    std::cout << "Stage 5D selector regression PASS\n";
}
