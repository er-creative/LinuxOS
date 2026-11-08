#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevManager001HistoricalStrategyD7B::DevManager001HistoricalStrategyD7B(
    DevD7BConfig config, const DevManager001StatisticalStrategyD7& strategy)
    : config_(std::move(config)),
      manager_({config_.manager_id, config_.top_k}, strategy.rankingPolicy()) {
    if(config_.manager_id.empty() || config_.top_k == 0 ||
       config_.decision_interval_minutes <= 0 ||
       1440 % config_.decision_interval_minutes != 0)
        throw std::invalid_argument("D7B invalid manager/Top-K/decision cadence");
}
DevD7BReport DevManager001HistoricalStrategyD7B::replay(
    const DevRealHistoricalMultiStockFeed& feed,
    const std::vector<DevHistoricalMinuteInput>& inputs,
    const DevD7BConsumer& consumer) const {
    if(!consumer) throw std::invalid_argument("D7B consumer required");
    DevD7BReport out;
    out.feed=feed.replay(inputs,[&](const DevHistoricalManagerFrame& frame) {
        ++out.observed_frames;
        // D5 keys are exchange-local day-ordinal*1440+minute.
        if(frame.decision_key % config_.decision_interval_minutes != 0) return;
        const auto ranked=manager_.evaluate(frame);
        ++out.decision_frames;
        out.evaluated += ranked.evaluated;
        out.rejected += ranked.rejected;
        out.selected += ranked.selected.size();
        consumer(ranked);
    });
    return out;
}
DevD7Forecast diagnosticPastOnlyForecast(
    const DevHistoricalStockFrame& s, const DevHistoricalManagerFrame& f) {
    if(s.five_minute.completed_key > f.decision_key ||
       s.five_minute.completed_key <= 0 ||
       f.decision_key-s.five_minute.completed_key >= 5 ||
       !(s.five_minute.open > 0.0) ||
       !std::isfinite(s.five_minute.open) ||
       !std::isfinite(s.five_minute.close))
        return {};
    return {(s.five_minute.close / s.five_minute.open) - 1.0,
            s.five_minute.completed_key, true};
}
} // namespace devai::strategy
