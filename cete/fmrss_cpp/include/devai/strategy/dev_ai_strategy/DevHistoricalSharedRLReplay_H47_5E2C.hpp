#pragma once
#include "devai/strategy/dev_ai_strategy/DevRankingRefreshController_H47_5E1.hpp"
#include "devai/strategy/dev_ai_strategy/DevSelectedRLInference_H47_5E2B.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace devai::strategy {

// Historical inference only: no brokerage, no fills, no trading authority.
// Input callbacks MUST use completed, causal 1m/5m/15m features.
enum class DevHistoricalCycleStatus_H47_5E2C {
    NoFreshRanking, InferenceComplete, MandatoryFlatBoundary
};
struct DevHistoricalInferenceRow_H47_5E2C {
    std::int64_t decision_time{};
    std::int64_t ranking_time{};
    DevHistoricalCycleStatus_H47_5E2C status{};
    bool has_inference{false};
    DevSelectedRLCycle_H47_5E2B inference{};
};
using DevHistoricalRankingProvider_H47_5E2C =
    std::function<DevTopCandidateSelection(std::int64_t)>;

class DevHistoricalSharedRLReplay_H47_5E2C final {
public:
    DevHistoricalSharedRLReplay_H47_5E2C(
        const DevSelectedRLInference_H47_5E2B& inference,
        DevHistoricalRankingProvider_H47_5E2C ranking_provider)
        : inference_(inference), ranking_provider_(std::move(ranking_provider)) {
        if (!ranking_provider_) throw std::invalid_argument("5E2C: ranking provider required");
    }

    // Time arguments are UNIX epochs, UTC-aligned, with exchange-session
    // boundaries explicitly supplied by the caller (no implicit timezone).
    // end is exclusive. flat_boundary is the 15:15 IST epoch for this session.
    [[nodiscard]] std::vector<DevHistoricalInferenceRow_H47_5E2C> run(
        std::int64_t begin, std::int64_t end, std::int64_t flat_boundary) const {
        if (begin <= 0 || begin % 60 || end <= begin || end % 60 ||
            flat_boundary <= begin || flat_boundary > end ||
            flat_boundary % 60 || (end - begin) > 24*60*60)
            throw std::invalid_argument("5E2C: invalid historical session window");

        DevRankingRefreshController_H47_5E1 ranking_controller;
        std::vector<DevHistoricalInferenceRow_H47_5E2C> rows;
        rows.reserve(static_cast<std::size_t>((end-begin)/60));
        for (auto t=begin; t<end; t+=60) {
            DevHistoricalInferenceRow_H47_5E2C row{};
            row.decision_time=t;
            // Never evaluate or refresh a model at/after mandatory flat.
            if (t>=flat_boundary) {
                row.status=DevHistoricalCycleStatus_H47_5E2C::MandatoryFlatBoundary;
                rows.push_back(std::move(row));
                continue;
            }
            if (ranking_controller.needsRefresh(t)) {
                const auto snapshot=ranking_provider_(t);
                if (snapshot.decision_time!=t)
                    throw std::invalid_argument("5E2C: ranking time mismatch");
                ranking_controller.publish(snapshot);
            }
            const auto* active=ranking_controller.activeAt(t);
            if (!active) {
                row.status=DevHistoricalCycleStatus_H47_5E2C::NoFreshRanking;
            } else {
                row.ranking_time=active->decision_time;
                row.inference=inference_.evaluate(*active,t);
                row.has_inference=true;
                row.status=DevHistoricalCycleStatus_H47_5E2C::InferenceComplete;
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }
private:
    const DevSelectedRLInference_H47_5E2B& inference_;
    DevHistoricalRankingProvider_H47_5E2C ranking_provider_;
};

} // namespace devai::strategy
