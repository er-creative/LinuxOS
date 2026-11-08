#include "devai/strategy/dev_ai_strategy/DevMultiCandidateState_D9F4A.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace devai::strategy::d9f4a {
namespace {
void require(bool ok, const char* msg) { if (!ok) throw std::invalid_argument(msg); }
bool finite(double v) { return std::isfinite(v); }
// Fixed bins are versioned; never silently change them after model training.
int scoreBin(double x) { return x < -2 ? -3 : x < -.7 ? -2 : x < 0 ? -1 : x == 0 ? 0 : x < .7 ? 1 : x < 2 ? 2 : 3; }
int forecastBin(double r) { // decimal returns; 5 bps steps, clipped to [-20,20]
    return std::clamp(static_cast<int>(std::floor(r / 0.0005)), -20, 20);
}
int ageBin(int x) { return x < 5 ? 0 : x < 10 ? 1 : x < 20 ? 2 : x < 40 ? 3 : 4; }
int timeBin(int x) { return x <= 10 ? 0 : x <= 30 ? 1 : x <= 90 ? 2 : 3; }
} // namespace
std::string DevMultiCandidateState_D9F4A::key(const CandidateState& s) {
    const auto& p=s.legacy_state;
    require(s.rank>=1 && s.rank<=5, "D9F4A invalid rank");
    require(finite(s.statistical_score) && finite(s.forecast_component) &&
            finite(s.combined_score) && finite(s.predicted_forward_return), "D9F4A nonfinite features");
    std::ostringstream o;
    o << "D9F4A:v1:" << (p.forecast_mode?1:0) << ':' << (p.invested?1:0)
      << ':' << s.rank << ':' << scoreBin(s.statistical_score)
      << ':' << (s.forecast_ready?forecastBin(s.predicted_forward_return):99)
      << ':' << scoreBin(s.combined_score) << ':' << scoreBin(p.held_score)
      << ':' << scoreBin(p.advantage) << ':' << ageBin(p.position_age)
      << ':' << timeBin(p.minutes_to_close)
      << ':' << std::clamp(p.entries_remaining,0,3)
      << ':' << (s.eligible?1:0);
    return o.str();
}
std::vector<CandidateState> DevMultiCandidateState_D9F4A::build(
    const PortfolioInput& p, const std::vector<CandidateInput>& candidates) {
    require(p.decision_key>0, "D9F4A invalid decision key");
    require(candidates.size()>=1 && candidates.size()<=5, "D9F4A expected 1..5 ranked candidates");
    require(p.position_age>=0 && p.minutes_to_close>=0 && p.entries_remaining>=0,
            "D9F4A invalid portfolio counters");
    require(finite(p.held_score), "D9F4A invalid held score");
    require(!p.invested || !p.held_symbol.empty(), "D9F4A invested without held symbol");
    std::set<std::string> symbols;
    std::set<int> ranks;
    std::vector<CandidateState> result;
    result.reserve(candidates.size());
    for (const auto& c : candidates) {
        require(!c.symbol.empty() && c.symbol!=p.held_symbol, "D9F4A invalid/held candidate");
        require(c.decision_key==p.decision_key, "D9F4A stale/future candidate");
        require(c.rank>=1 && c.rank<=5 && symbols.insert(c.symbol).second && ranks.insert(c.rank).second,
                "D9F4A duplicate or invalid ranking");
        require(finite(c.zscore_1m) && finite(c.zscore_5m) && finite(c.zscore_15m) &&
                finite(c.predicted_forward_return), "D9F4A nonfinite candidate");
        // Unready forecasts must not silently contribute a prediction.
        require(c.forecast_ready || c.predicted_forward_return==0.0, "D9F4A unready forecast has value");
        const double stat=.5*c.zscore_1m+.3*c.zscore_5m+.2*c.zscore_15m;
        const double bonus=p.forecast_mode && c.forecast_ready ? 100.0*c.predicted_forward_return : 0.0;
        const double combined=stat+bonus;
        require(finite(stat) && finite(bonus) && finite(combined), "D9F4A score overflow");
        CandidateState s;
        s.symbol=c.symbol;s.decision_key=c.decision_key;s.rank=c.rank;
        s.statistical_score=stat;s.forecast_component=bonus;s.combined_score=combined;
        s.predicted_forward_return=c.predicted_forward_return;
        s.forecast_ready=c.forecast_ready;s.eligible=c.eligible;
        s.legacy_state={};
        s.legacy_state.invested=p.invested;
        s.legacy_state.candidate_score=combined;
        s.legacy_state.held_score=p.invested?p.held_score:0.0;
        s.legacy_state.advantage=p.invested?combined-p.held_score:combined;
        s.legacy_state.position_age=p.position_age;
        s.legacy_state.minutes_to_close=p.minutes_to_close;
        s.legacy_state.entries_remaining=p.entries_remaining;
        s.legacy_state.candidate_available=c.eligible;
        s.legacy_state.forecast_mode=p.forecast_mode;
        s.state_key=key(s);
        result.push_back(std::move(s));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.rank<b.rank;});
    return result;
}
} // namespace devai::strategy::d9f4a
