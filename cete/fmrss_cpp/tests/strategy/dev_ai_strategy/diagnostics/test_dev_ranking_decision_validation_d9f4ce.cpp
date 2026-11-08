#include "devai/strategy/dev_ai_strategy/diagnostics/DevRankingDecisionValidator_D9F4CE.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy::d9f4ce;
int main() {
    Row r{};
    r.time=1790049600; r.raw=4;r.final=4;r.legal={true,false,true,true,true,true,true};
    r.q={0.1,0.0,0.2,0.3,0.9,0.1,0.1};
    r.symbols={"A","B","C","D","E"};r.scores={39,39,39,39,39};
    r.desired="C";r.held_after="C";
    Row second=r;second.time+=60;second.held_before="C";second.age=1;
    Row end=second;end.time=1790070300;end.age=345;end.raw=4;end.final=0;
    end.legal={true,true,false,false,true,false,false};end.desired="";end.held_after="";
    end.recommendation="EXIT_REQUIRED";
    auto s=validate({r,second,end});
    assert(s.rows==3 && s.raw_action4==3 && s.forced_exits==1);
    assert(s.equal_score_rows==3 && s.all_scores_constant && s.candidate_slot_changes==0);
    assert(s.illegal_final==0 && s.raw_not_greedy==0 && s.deadline_violations==0);
    Row bad=second;bad.time=1790070000;bad.desired="D";
    auto v=validate({bad});assert(v.entry_cutoff_violations==1);
    std::cout<<"D9F4CE_RANKING_DECISION_VALIDATION_PASS actions=1 ranking=1 cutoff=1 forced_exit=1\n";
}
