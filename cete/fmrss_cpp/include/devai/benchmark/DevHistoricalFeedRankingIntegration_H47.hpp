#pragma once

#include "devai/benchmark/DevHistoricalObservationAdapter_H47.hpp"
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockFeed.hpp"
#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace devai::benchmark::h47 {

// Stage 2 collects causal observations and timestamped, completed 1m close prices.
// It deliberately does not execute a broker/ledger or manufacture a ranking policy.
struct HistoricalFeedRankingBatch {
    std::vector<Observation> observations;
    // UTC decision timestamp -> symbol -> last completed 1m close at that decision.
    std::map<std::int64_t, std::map<std::string, double>> completed_close_prices;
    devai::strategy::DevHistoricalFeedReport feed_report{};
    std::size_t skipped_without_five{0};
};

class DevHistoricalFeedRankingIntegrationH47 final {
public:
    // Can be tested with actual manager/ranking frames independently of file ingestion.
    // Ranking must originate from DevManager001HistoricalRanking::evaluate(frame).
    static void append(HistoricalFeedRankingBatch& out,
                       const ManagerFrame& frame,
                       const RankingFrame& ranking,
                       const TimeConverter& clock,
                       const FeatureEncoder& encoder) {
        if (ranking.selected.size() != 5) {
            // Do not silently invent candidate slots, or skip mandatory square-off.
            const auto minute = frame.decision_key % 1440;
            if (minute >= 915)
                throw std::invalid_argument("H4.7 Stage2: no five candidates at mandatory square-off");
            ++out.skipped_without_five;
            return;
        }
        auto observation = DevHistoricalObservationAdapterH47::convert(
            frame, ranking, clock, encoder);
        if (!out.observations.empty() &&
            observation.decision_time_utc <= out.observations.back().decision_time_utc)
            throw std::invalid_argument("H4.7 Stage2: nonchronological historical frames");
        if (!out.observations.empty() && out.observations.back().terminal)
            throw std::invalid_argument("H4.7 Stage2: frame after terminal");

        std::map<std::string, double> prices;
        for (const auto& stock : frame.stocks) {
            const auto& candle = stock.one_minute;
            if (stock.symbol.empty() || candle.completed_key <= 0 ||
                candle.completed_key > frame.decision_key ||
                !std::isfinite(candle.close) || candle.close <= 0.0)
                throw std::invalid_argument("H4.7 Stage2: invalid completed price");
            if (!prices.emplace(stock.symbol, candle.close).second)
                throw std::invalid_argument("H4.7 Stage2: duplicate price symbol");
        }
        for (const auto& symbol : observation.ranked_top_five)
            if (!prices.contains(symbol))
                throw std::invalid_argument("H4.7 Stage2: ranked candidate lacks price");
        // A held symbol can fall out of the top five. Preserve prices for all stocks.
        if (!out.completed_close_prices.emplace(observation.decision_time_utc,
                                                 std::move(prices)).second)
            throw std::invalid_argument("H4.7 Stage2: duplicate decision timestamp");
        out.observations.push_back(std::move(observation));
    }

    // Real CETE integration. Supply the established statistical ranking policy
    // through the manager object, plus the production UTC converter and H2 encoder.
    static HistoricalFeedRankingBatch run(
        const devai::strategy::DevRealHistoricalMultiStockFeed& feed,
        const std::vector<devai::strategy::DevHistoricalMinuteInput>& inputs,
        const devai::strategy::DevManager001HistoricalRanking& manager,
        const TimeConverter& clock,
        const FeatureEncoder& encoder) {
        if (inputs.empty())
            throw std::invalid_argument("H4.7 Stage2: no historical minute inputs");
        HistoricalFeedRankingBatch out;
        out.feed_report = feed.replay(inputs, [&](const ManagerFrame& frame) {
            const auto ranked = manager.evaluate(frame);
            append(out, frame, ranked, clock, encoder);
        });
        if (out.observations.empty())
            throw std::invalid_argument("H4.7 Stage2: no complete five-stock observations");
        if (!out.observations.back().terminal)
            throw std::invalid_argument("H4.7 Stage2: missing 15:15 terminal observation");
        return out;
    }
};

} // namespace devai::benchmark::h47
