#include "devai/benchmark/DevActualReplayOrchestrator.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace devai::benchmark::h44 {
namespace {
void check(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
bool finite(double x) { return std::isfinite(x); }
void checkCandle(const Candle& c, std::int64_t t) {
    check(c.complete, "incomplete candle");
    check(c.close_time_utc == t, "stale/future candle");
    check(finite(c.open)&&finite(c.high)&&finite(c.low)&&finite(c.close)&&finite(c.volume),
          "nonfinite OHLCV");
    check(c.low>0 && c.high>=c.low && c.open>=c.low && c.open<=c.high &&
          c.close>=c.low && c.close<=c.high && c.volume>=0, "invalid OHLCV");
}
}
std::vector<Evaluation> DevActualReplayOrchestrator::run(const RunOptions& o) {
    check(o.universe.size()==kUniverse, "expected exactly 10 tradable symbols");
    std::set<std::string> universe(o.universe.begin(),o.universe.end());
    check(universe.size()==kUniverse && !universe.count("NIFTY%2050"),
          "invalid tradable universe");
    for(const auto& s:universe) check(!s.empty(), "empty symbol");
    check(!o.completed_candle_times.empty(), "empty replay times");
    for(std::size_t i=1;i<o.completed_candle_times.size();++i)
        check(o.completed_candle_times[i]>o.completed_candle_times[i-1],
              "nonchronological replay times");
    std::vector<Evaluation> results;
    results.push_back({Direction::Long,{}});
    if(o.enable_short) results.push_back({Direction::Short,{}});
    for(auto t:o.completed_candle_times) {
        auto candles=bridge_.completedCandles(t);
        check(candles.size()==kUniverse, "missing/extra candles");
        std::set<std::string> seen;
        for(const auto& c:candles) {
            checkCandle(c,t);
            check(universe.count(c.symbol)==1 && seen.insert(c.symbol).second,
                  "unknown/duplicate symbol");
        }
        for(auto& result:results) {
            auto ranked=bridge_.rankTopFive(result.direction,candles,t);
            check(ranked.size()==kTopK, "ranking must return top five");
            std::set<std::string> selected;
            for(const auto& c:ranked) {
                check(universe.count(c.symbol)==1 && selected.insert(c.symbol).second,
                      "invalid top-five symbol");
                check(finite(c.score), "invalid score");
                for(double x:c.observation) check(finite(x), "invalid observation");
            }
            auto decision=bridge_.modelDecision(result.direction,t,ranked);
            check(decision.close_time_utc==t && decision.direction==result.direction,
                  "model decision timestamp/direction mismatch");
            for(std::size_t i=0;i<kTopK;++i)
                check(decision.ranked_symbols[i]==ranked[i].symbol,
                      "decision ranking mismatch");
            if(decision.action==Action::Open) {
                check(selected.count(decision.symbol)==1, "OPEN outside top-five");
                check(finite(decision.price) && decision.price>0, "invalid OPEN price");
            }
            result.decisions.push_back(std::move(decision));
            result.model_predictions_used=true;
        }
    }
    // Fail-closed: do not claim replay success until the actual engine returns.
    for(auto& result:results) {
        bridge_.executeHistoricalReplay(result.direction,result.decisions);
        result.execution_replay_used=true;
    }
    return results;
}
}
