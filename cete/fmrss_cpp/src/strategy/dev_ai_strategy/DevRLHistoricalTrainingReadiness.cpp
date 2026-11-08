#include "devai/strategy/dev_ai_strategy/DevRLHistoricalTrainingReadiness.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>

namespace devai::strategy {
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::invalid_argument(message);
}
bool near(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
           std::abs(a-b) <= 1e-10 * (1.0 + std::max(std::abs(a),std::abs(b)));
}
void checkCandidates(const std::vector<DevSharedCandidate>& actual,
                     const std::vector<DevUniverseRankedShare>& all,
                     std::int64_t time) {
    auto ranked=all;
    std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){
        if(a.score!=b.score) return a.score>b.score;
        return a.symbol<b.symbol;
    });
    const std::array<std::string,5> ids={"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"};
    check(actual.size()==ids.size(),"expected five candidates");
    for(std::size_t j=0;j<ids.size();++j) {
        const auto& a=actual[j];
        check(a.slot==j && a.candidate_rank==j+1 && a.symbol==ranked[j].symbol,
              "Phase-4 top-five ranking/slot mismatch");
        check(near(a.candidate_score,ranked[j].score),"candidate score mismatch");
        check(a.observation.ready && a.observation.numerical_integrity &&
              a.observation.decision_time==time && a.observation.symbol==a.symbol,
              "invalid candidate observation timestamp/identity");
    }
}
void checkUniverse(const DevUniverseRankingTick& tick,
                   const std::set<std::string>& expected) {
    check(tick.universe.size()==expected.size(),"incomplete Phase-4 universe");
    std::set<std::string> seen;
    for(const auto& s:tick.universe) {
        check(expected.count(s.symbol) && seen.insert(s.symbol).second,
              "unknown or duplicate Phase-4 symbol");
        check(std::isfinite(s.score) && s.observation.ready &&
              s.observation.numerical_integrity &&
              s.observation.symbol==s.symbol &&
              s.observation.decision_time==tick.decision_time,
              "invalid Phase-4 score or observation");
    }
}
} // namespace
DevRLHistoricalTrainingReadinessReport DevRLHistoricalTrainingReadiness::validate(
    const std::vector<DevUniverseRankingTick>& ranking,
    const std::vector<std::string>& universe,
    const std::vector<DevHistoricalExperience>& history,
    std::int64_t cutoff,
    const std::vector<std::string>& held_symbols_before) {
    DevRLHistoricalTrainingReadinessReport out{};
    try {
        check(cutoff>0,"invalid training cutoff");
        check(!ranking.empty() && ranking.size()==history.size(),
              "ranking/history count mismatch");
        check(universe.size()==10,"current top-five adapter expects ten shares");
        const std::set<std::string> expected(universe.begin(),universe.end());
        check(expected.size()==universe.size() && !expected.count(""),
              "invalid universe symbols");
        check(held_symbols_before.empty() || held_symbols_before.size()==history.size(),
              "held-position timeline length mismatch");
        std::int64_t last=0;
        double initial=0;
        for(std::size_t i=0;i<history.size();++i) {
            const auto& r=ranking[i];const auto& h=history[i];
            check(r.decision_time>last && h.decision_time==r.decision_time,
                  "nonchronological or mismatched decision timestamp");
            check(h.decision_time<cutoff,"decision outside training interval");
            checkUniverse(r,expected);
            checkCandidates(h.candidates,r.universe,r.decision_time);
            check(h.terminal==(i+1==history.size()),"terminal flag mismatch");
            if(i+1<history.size()) {
                check(h.next_decision_time==ranking[i+1].decision_time &&
                      h.next_decision_time>h.decision_time &&
                      h.next_decision_time<cutoff,
                      "invalid next decision or training-boundary leakage");
                checkCandidates(h.next_candidates,ranking[i+1].universe,
                                ranking[i+1].decision_time);
                check(near(h.equity_after,history[i+1].equity_before),
                      "equity timeline discontinuity");
            } else {
                check(h.next_candidates.empty(),"terminal has next candidates");
            }
            check(std::isfinite(h.equity_before) && h.equity_before>0 &&
                  std::isfinite(h.equity_after) && h.equity_after>0,
                  "invalid equity");
            if(i==0) initial=h.equity_before;
            check(near(h.portfolio_reward,(h.equity_after-h.equity_before)/initial),
                  "supervisor reward does not match equity delta");
            if(h.intent.action==DevRotationAction::BUY ||
               h.intent.action==DevRotationAction::ROTATE) {
                bool found=false;
                const std::array<std::string,5> ids={"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"};
                for(const auto& c:h.candidates)
                    if(c.symbol==h.intent.symbol && c.slot<ids.size() &&
                       ids[c.slot]==h.intent.agent_id) found=true;
                check(found,"BUY/ROTATE not assigned to current top-five agent");
            }
            if(!held_symbols_before.empty() && !held_symbols_before[i].empty()) {
                check(expected.count(held_symbols_before[i])!=0,
                      "held symbol outside manager universe");
                // A held stock need not remain top-five. The caller must
                // separately prove manager state retains the position.
            }
            last=r.decision_time;
            ++out.decisions;
            out.candidate_assignments+=h.candidates.size();
        }
        out.ready=true;
    } catch(const std::exception& e) {out.errors.emplace_back(e.what());}
    return out;
}
} // namespace devai::strategy
