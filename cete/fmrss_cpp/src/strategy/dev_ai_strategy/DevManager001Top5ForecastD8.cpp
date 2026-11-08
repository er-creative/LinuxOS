#include "devai/strategy/dev_ai_strategy/DevManager001Top5ForecastD8.hpp"
#include <cmath>
#include <set>
#include <stdexcept>

namespace devai::strategy {
DevManager001Assessment devD8StatisticalRankingPolicy(
    const DevHistoricalStockFrame& s, const DevHistoricalManagerFrame& f) {
    // D6 validates causality, freshness and readiness before invoking this policy.
    (void)f;
    if (!std::isfinite(s.close_1m.zscore) || !std::isfinite(s.close_5m.zscore) ||
        !std::isfinite(s.close_15m.zscore)) return {false, 0.0, "NONFINITE_Z"};
    const double score=0.5*std::abs(s.close_1m.zscore)
                      +0.3*std::abs(s.close_5m.zscore)
                      +0.2*std::abs(s.close_15m.zscore);
    return {true, score, "STATISTICAL_BASELINE"};
}
DevManager001Top5ForecastD8::DevManager001Top5ForecastD8(
    const DevManager001HistoricalRanking& r, const DevCausalForwardReturnD8& m,
    std::int64_t horizon):ranker_(r),model_(m),horizon_(horizon) {
    if(horizon<=0 || horizon>=1440) throw std::invalid_argument("D8 invalid horizon");
}
DevD8Top5ForecastFrame DevManager001Top5ForecastD8::evaluate(
    const DevHistoricalManagerFrame& f) const {
    const auto ranked=ranker_.evaluate(f); // ranks all 10 BEFORE any forecast call
    if(ranked.decision_key!=f.decision_key || ranked.manager_id!=f.manager_id ||
       ranked.selected.size()>5) throw std::runtime_error("D8 invalid D6 ranking frame");
    DevD8Top5ForecastFrame out;
    out.manager_id=ranked.manager_id;
    out.decision_key=ranked.decision_key;
    out.evaluated=ranked.evaluated;
    out.rejected=ranked.rejected;
    std::set<std::string> seen;
    for(const auto& c:ranked.selected) {
        if(!seen.insert(c.symbol).second || c.symbol.empty())
            throw std::runtime_error("D8 duplicate selected symbol");
        DevD8SelectedForecast p;
        p.symbol=c.symbol;
        p.ranking_score=c.ranking_score;
        p.as_of_key=f.decision_key;
        p.target_key=f.decision_key+horizon_;
        p.current_price=c.observations.one_minute.close;
        if(!std::isfinite(p.current_price) || p.current_price<=0) {
            p.reason="INVALID_CURRENT_PRICE";
        } else if(!devD8SameSession(f.decision_key,p.target_key)) {
            p.reason="TARGET_OUTSIDE_SESSION_DAY";
        } else if(p.target_key%1440 >
                  (((f.decision_key-1)/1440 == 739850) ? 14*60+45 : 15*60+30)) {
            // Standard NSE close 15:30; special 2025-10-21 session closes 14:45.
            p.reason="TARGET_AFTER_SESSION_CLOSE";
        } else if(!model_.fitted()) {
            p.reason="MODEL_NOT_FITTED";
        } else {
            try {
                const auto predicted=model_.predict(c.observations,f);
                if(!predicted.ready) {
                    p.reason="FORECAST_UNAVAILABLE";
                } else if(predicted.as_of_key!=f.decision_key) {
                    p.reason="FORECAST_NOT_CURRENT";
                } else {
                    p.predicted_forward_return=predicted.expected_forward_return;
                    p.predicted_future_price=p.current_price*(1.0+p.predicted_forward_return);
                    if(!std::isfinite(p.predicted_forward_return) ||
                       !std::isfinite(p.predicted_future_price) ||
                       p.predicted_future_price<=0) {
                        p.predicted_forward_return=0;
                        p.predicted_future_price=0;
                        p.reason="INVALID_FORECAST_VALUE";
                    } else {
                        p.ready=true;
                        p.reason="READY";
                    }
                }
            } catch(const std::exception&) {
                p.reason="INVALID_CAUSAL_FEATURES";
            }
        }
        out.selected.push_back(std::move(p));
    }
    return out;
}
} // namespace devai::strategy
