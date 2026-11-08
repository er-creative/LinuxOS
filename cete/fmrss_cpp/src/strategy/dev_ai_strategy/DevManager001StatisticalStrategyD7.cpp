#include "devai/strategy/dev_ai_strategy/DevManager001StatisticalStrategyD7.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>
namespace devai::strategy {
namespace {
bool valid(double x) { return std::isfinite(x); }
}
DevManager001StatisticalStrategyD7::DevManager001StatisticalStrategyD7(
    DevD7StrategyConfig config, DevD7ForecastProvider forecast)
    : config_(std::move(config)), forecast_(std::move(forecast)) {
    if(config_.manager_id.empty() || !forecast_ ||
       !valid(config_.min_expected_return) ||
       !valid(config_.min_abs_z1) || !valid(config_.min_abs_z5) || !valid(config_.min_abs_z15) ||
       config_.min_abs_z1 < 0 || config_.min_abs_z5 < 0 || config_.min_abs_z15 < 0)
        throw std::invalid_argument("D7 requires an explicit causal forecast provider and valid thresholds");
}
DevD7StrategyAssessment DevManager001StatisticalStrategyD7::assess(
    const DevHistoricalStockFrame& s, const DevHistoricalManagerFrame& f) const {
    if(f.manager_id != config_.manager_id || f.decision_key <= 0 || s.symbol.empty() ||
       s.one_minute.completed_key != f.decision_key ||
       s.five_minute.completed_key > f.decision_key ||
       s.fifteen_minute.completed_key > f.decision_key ||
       s.five_minute.completed_key <= 0 ||
       s.fifteen_minute.completed_key <= 0 ||
       f.decision_key-s.five_minute.completed_key >= 5 ||
       f.decision_key-s.fifteen_minute.completed_key >= 15 ||
       (s.five_minute.completed_key-1)/1440 != (f.decision_key-1)/1440 ||
       (s.fifteen_minute.completed_key-1)/1440 != (f.decision_key-1)/1440 ||
       !s.close_1m.ready || !s.close_5m.ready || !s.close_15m.ready)
        throw std::invalid_argument("D7 stale/unready observation or manager mismatch");
    const double z1=s.close_1m.zscore,z5=s.close_5m.zscore,z15=s.close_15m.zscore;
    if(!valid(z1)||!valid(z5)||!valid(z15)||
       !valid(s.close_1m.mean)||!valid(s.close_5m.mean)||!valid(s.close_15m.mean)||
       !valid(s.close_1m.standard_deviation)||!valid(s.close_5m.standard_deviation)||
       !valid(s.close_15m.standard_deviation))
        throw std::invalid_argument("D7 nonfinite statistical features");
    DevD7StrategyAssessment out;
    if(std::abs(z1)<config_.min_abs_z1 || std::abs(z5)<config_.min_abs_z5 ||
       std::abs(z15)<config_.min_abs_z15) {
        out.ranking.reason="Z_THRESHOLD"; return out;
    }
    if(config_.require_same_z_direction && !((z1>0&&z5>0&&z15>0)||(z1<0&&z5<0&&z15<0))) {
        out.ranking.reason="Z_DIRECTION"; return out;
    }
    out.forecast=forecast_(s,f);
    if(!out.forecast.ready) {out.ranking.reason="FORECAST_UNAVAILABLE";return out;}
    if(out.forecast.as_of_key <= 0 || out.forecast.as_of_key > f.decision_key)
        throw std::invalid_argument("D7 forecast uses future/unverifiable timestamp");
    if(!valid(out.forecast.expected_forward_return))
        throw std::invalid_argument("D7 nonfinite forecast");
    if(out.forecast.expected_forward_return < config_.min_expected_return) {
        out.ranking.reason="FORECAST_THRESHOLD"; return out;
    }
    out.ranking.eligible=true;
    out.ranking.ranking_score=config_.rank_by_signed_forecast ?
        out.forecast.expected_forward_return : std::abs(out.forecast.expected_forward_return);
    out.ranking.reason="ELIGIBLE";
    return out;
}
DevManager001StatisticalPolicy DevManager001StatisticalStrategyD7::rankingPolicy() const {
    return [self=*this](const DevHistoricalStockFrame& s,const DevHistoricalManagerFrame& f) {
        return self.assess(s,f).ranking;
    };
}
} // namespace devai::strategy
