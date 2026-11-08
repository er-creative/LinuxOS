#pragma once
#include "devai/benchmark/DevHistoricalFeedRankingIntegration_H47.hpp"
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devai::benchmark::h47 {

// CETE uses Howard Hinnant's civil-day ordinal (1970-01-01 == 719468).
// Historical keys are exchange-local day ordinal * 1440 + minute of day.
inline std::int64_t exchangeKeyToUtcH47(std::int64_t key) {
    if (key <= 0) throw std::invalid_argument("H4.7 Stage3: invalid exchange key");
    constexpr std::int64_t epoch_ordinal = 719468;
    const auto day = key / 1440;
    const auto minute = key % 1440;
    if (day < epoch_ordinal || minute < 0 || minute >= 1440)
        throw std::invalid_argument("H4.7 Stage3: invalid exchange date");
    return (day - epoch_ordinal) * 86400 + minute * 60 - 19800;
}

struct HistoricalDayH47 {
    std::int64_t exchange_day{};
    HistoricalFeedRankingBatch batch;
};
struct RealHistoricalExecutionResultH47 {
    devai::strategy::DevHistoricalFeedReport feed_report;
    std::vector<HistoricalDayH47> days;
    std::size_t incomplete_days{};
    std::size_t total_observations{};
};

// Runs actual CETE historical feed + manager ranking. No RL or D9F4C is invoked.
// Caller supplies its established statistical policy inside 'manager' and its
// causal H2 feature encoder. No placeholder model/forecast is manufactured here.
class DevRealHistoricalExecutionH47 final {
public:
    static RealHistoricalExecutionResultH47 run(
        const devai::strategy::DevRealHistoricalMultiStockFeed& feed,
        const std::vector<devai::strategy::DevHistoricalMinuteInput>& inputs,
        const devai::strategy::DevManager001HistoricalRanking& manager,
        const FeatureEncoder& encoder) {
        if (inputs.empty() || !encoder)
            throw std::invalid_argument("H4.7 Stage3: inputs and encoder required");
        RealHistoricalExecutionResultH47 result;
        const TimeConverter convert = exchangeKeyToUtcH47;
        // The feed owns validation, completed candles, and warm-up handling.
        result.feed_report = feed.replay(inputs, [&](const ManagerFrame& frame) {
            const auto minute = frame.decision_key % 1440;
            if (minute > 915) return; // post-square-off candles are not decisions
            const auto day = (frame.decision_key - 1) / 1440;
            if (result.days.empty() || result.days.back().exchange_day != day) {
                if (!result.days.empty() &&
                    (result.days.back().batch.observations.empty() ||
                     !result.days.back().batch.observations.back().terminal))
                    ++result.incomplete_days;
                result.days.push_back(HistoricalDayH47{day, {}});
            }
            auto ranked = manager.evaluate(frame);
            auto& batch = result.days.back().batch;
            DevHistoricalFeedRankingIntegrationH47::append(
                batch, frame, ranked, convert, encoder);
            // A complete session ends at 15:15. Frames after cutoff are ignored;
            // the historical feed may continue emitting until 15:30.
            // NOTE: do not append those frames to a terminal batch.
        });
        if (!result.days.empty() &&
            (result.days.back().batch.observations.empty() ||
             !result.days.back().batch.observations.back().terminal))
            ++result.incomplete_days;
        for (const auto& day : result.days)
            result.total_observations += day.batch.observations.size();
        if (!result.feed_report.validation.passed)
            throw std::runtime_error("H4.7 Stage3: historical feed validation failed");
        if (result.total_observations == 0)
            throw std::runtime_error("H4.7 Stage3: no usable historical observations");
        return result;
    }
};
} // namespace devai::benchmark::h47
