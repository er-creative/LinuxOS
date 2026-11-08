#pragma once
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include "devai/strategy/dev_ai_strategy/DevRankingRefreshController_H47_5E1.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace devai::strategy {
// Model-agnostic integration boundary. This is NOT an RL model.
// The caller supplies a callback backed by the EXISTING single shared model.
// Observations must be built from data completed by decision_time.
struct DevSharedRLAssessment_H47_5E2 {
    std::string agent_name;
    std::string symbol;
    std::int64_t decision_time{0};
    double action_value{0.0}; // model-defined value; not a probability
    bool valid{false};
};
struct DevSharedRLCycle_H47_5E2 {
    std::int64_t decision_time{0};
    std::int64_t ranking_time{0};
    std::array<DevSharedRLAssessment_H47_5E2, DEV_AGENT_SLOT_COUNT> reports{};
    bool complete{false};
};
using DevSharedRLInferenceFn_H47_5E2 = std::function<double(
    const std::string& symbol, std::int64_t decision_time,
    std::size_t agent_slot)>;

class DevSharedRLInferenceBridge_H47_5E2 final {
public:
    explicit DevSharedRLInferenceBridge_H47_5E2(
        DevSharedRLInferenceFn_H47_5E2 infer) : infer_(std::move(infer)) {
        if (!infer_) throw std::invalid_argument("5E2 requires real shared-model inference callback");
    }
    [[nodiscard]] DevSharedRLCycle_H47_5E2 evaluate(
        const DevTopCandidateSelection& ranking, std::int64_t decision_time) const {
        if (decision_time <= 0 || decision_time % 60 != 0 ||
            !ranking.numerical_integrity || ranking.decision_time <= 0 ||
            !DevRankingRefreshController_H47_5E1::isSessionTime(decision_time) ||
            !DevRankingRefreshController_H47_5E1::isRefreshTime(ranking.decision_time) ||
            ranking.decision_time != DevRankingRefreshController_H47_5E1::rankingEpoch(decision_time) ||
            ranking.selected_candidate_count != DEV_AGENT_SLOT_COUNT ||
            ranking.eligible_candidate_count < DEV_AGENT_SLOT_COUNT ||
            ranking.benchmark_symbol.empty())
            throw std::invalid_argument("5E2 invalid/stale ranking or decision timestamp");
        DevSharedRLCycle_H47_5E2 result;
        result.decision_time = decision_time;
        result.ranking_time = ranking.decision_time;
        std::unordered_set<std::string> symbols;
        for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i) {
            const auto& a = ranking.assignments[i];
            if (!a.assigned || a.slot_index != i || a.candidate_rank != i+1 ||
                a.agent_name != DevTopCandidateSelector::agentNames()[i] ||
                a.decision_time != ranking.decision_time || a.symbol.empty() ||
                a.symbol == ranking.benchmark_symbol || !symbols.insert(a.symbol).second)
                throw std::invalid_argument("5E2 invalid agent assignment");
        }
        // Only after full validation, call the SAME inference callback five times.
        for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i) {
            const auto& a = ranking.assignments[i];
            const double v = infer_(a.symbol, decision_time, i);
            if (!std::isfinite(v)) throw std::runtime_error("5E2 nonfinite model output");
            result.reports[i] = {a.agent_name, a.symbol, decision_time, v, true};
        }
        result.complete = true;
        return result;
    }
private:
    DevSharedRLInferenceFn_H47_5E2 infer_;
};
} // namespace devai::strategy
