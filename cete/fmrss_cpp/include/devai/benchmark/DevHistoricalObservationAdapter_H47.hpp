#pragma once

#include "devai/benchmark/DevRuntimeReplayBridge.hpp"
#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

namespace devai::benchmark::h47 {

using Observation = devai::benchmark::h45::HistoricalManagerObservation;
using ManagerFrame = devai::strategy::DevHistoricalManagerFrame;
using RankingFrame = devai::strategy::DevManager001RankingFrame;

// Must convert CETE's exchange-local day-ordinal minute key to UTC epoch seconds.
using TimeConverter = std::function<std::int64_t(std::int64_t)>;

// Must use the existing production causal H2 encoder, never realized future labels.
using FeatureEncoder = std::function<
    std::array<double, devai::benchmark::kInputCount>(
        const ManagerFrame&, const RankingFrame&)>;

class DevHistoricalObservationAdapterH47 final {
public:
    static Observation convert(const ManagerFrame& frame,
                               const RankingFrame& ranking,
                               const TimeConverter& convert_time,
                               const FeatureEncoder& encode) {
        if (!convert_time || !encode)
            throw std::invalid_argument("H4.7: missing time converter or feature encoder");
        if (frame.manager_id != "DevManager_001" ||
            ranking.manager_id != "DevManager_001")
            throw std::invalid_argument("H4.7: LONG DevManager_001 only");
        if (frame.decision_key != ranking.decision_key || frame.decision_key <= 0)
            throw std::invalid_argument("H4.7: invalid or mismatched decision keys");
        if (ranking.selected.size() != 5)
            throw std::invalid_argument("H4.7: exactly five ranked candidates required");

        Observation result{};
        result.decision_time_utc = convert_time(frame.decision_key);
        if (result.decision_time_utc <= 0 || result.decision_time_utc % 60 != 0)
            throw std::invalid_argument("H4.7: invalid UTC decision timestamp");
        auto ist_seconds = (result.decision_time_utc + 19800) % 86400;
        if (ist_seconds < 0) ist_seconds += 86400;
        const auto minute_ist = ist_seconds / 60;
        auto minute_key = frame.decision_key % 1440;
        if (minute_key < 0) minute_key += 1440;
        if (minute_ist != minute_key)
            throw std::invalid_argument("H4.7: exchange-local key / UTC mismatch");
        if (minute_ist < 555 || minute_ist > 915)
            throw std::invalid_argument("H4.7: outside 09:15-15:15 IST");

        result.encoded_manager_features = encode(frame, ranking);
        for (double x : result.encoded_manager_features)
            if (!std::isfinite(x))
                throw std::invalid_argument("H4.7: nonfinite encoded feature");

        std::set<std::string> seen;
        for (std::size_t i = 0; i < 5; ++i) {
            const auto& candidate = ranking.selected[i];
            if (candidate.symbol.empty() || !seen.insert(candidate.symbol).second ||
                !std::isfinite(candidate.ranking_score))
                throw std::invalid_argument("H4.7: invalid ranked candidate");
            bool found = false;
            for (const auto& stock : frame.stocks) {
                if (stock.symbol != candidate.symbol) continue;
                if (stock.one_minute.completed_key <= 0 ||
                    stock.one_minute.completed_key > frame.decision_key)
                    throw std::invalid_argument("H4.7: future or uncompleted 1m candle");
                if (stock.five_minute.completed_key > frame.decision_key ||
                    stock.fifteen_minute.completed_key > frame.decision_key)
                    throw std::invalid_argument("H4.7: future higher-timeframe candle");
                found = true;
                break;
            }
            if (!found)
                throw std::invalid_argument("H4.7: ranked symbol missing from frame");
            result.ranked_top_five[i] = candidate.symbol;
        }
        result.legal_actions.fill(true);
        // These flags rely on a validated CETE feed and a certified causal encoder.
        // They are not independent proofs of feature causality.
        result.completed_candles = true;
        result.causal_features = true;
        result.terminal = minute_ist == 915;
        return result;
    }
};

} // namespace devai::benchmark::h47
