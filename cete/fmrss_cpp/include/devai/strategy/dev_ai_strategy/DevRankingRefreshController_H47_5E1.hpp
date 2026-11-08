#pragma once
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace devai::strategy {
// Session-anchored ranking publication only; no RL or trade authority.
class DevRankingRefreshController_H47_5E1 final {
public:
    static constexpr std::int64_t refresh_seconds = 1800;
    static constexpr std::int64_t decision_seconds = 60;
    static constexpr std::int64_t ist_offset_seconds = 19800;
    static constexpr std::int64_t day_seconds = 86400;
    static constexpr std::int64_t session_open_seconds = 9*3600 + 15*60;
    static constexpr std::int64_t session_close_seconds = 15*3600 + 15*60;

    static bool isDecisionTime(std::int64_t t) noexcept {
        return t > 0 && t % decision_seconds == 0;
    }
    static std::int64_t localDay(std::int64_t t) noexcept {
        return (t + ist_offset_seconds) / day_seconds;
    }
    static std::int64_t localSecondOfDay(std::int64_t t) noexcept {
        return (t + ist_offset_seconds) % day_seconds;
    }
    static bool isSessionTime(std::int64_t t) noexcept {
        if (!isDecisionTime(t)) return false;
        const auto s = localSecondOfDay(t);
        return s >= session_open_seconds && s <= session_close_seconds;
    }
    static bool isRefreshTime(std::int64_t t) noexcept {
        if (!isSessionTime(t)) return false;
        const auto elapsed = localSecondOfDay(t) - session_open_seconds;
        return elapsed < session_close_seconds - session_open_seconds &&
               elapsed % refresh_seconds == 0;
    }
    static std::int64_t rankingEpoch(std::int64_t t) noexcept {
        if (!isSessionTime(t)) return 0;
        const auto elapsed = localSecondOfDay(t) - session_open_seconds;
        const auto slot = elapsed == session_close_seconds - session_open_seconds
            ? elapsed - 1 : elapsed;
        return t - (elapsed - (slot / refresh_seconds) * refresh_seconds);
    }
    bool needsRefresh(std::int64_t t) const noexcept {
        return isRefreshTime(t) && (!active_ || t > active_->decision_time);
    }
    const DevTopCandidateSelection* activeAt(std::int64_t t) const noexcept {
        if (!active_ || !isSessionTime(t) ||
            localDay(t) != localDay(active_->decision_time) ||
            rankingEpoch(t) != active_->decision_time) return nullptr;
        return active_.get();
    }
    void publish(const DevTopCandidateSelection& next) {
        validate(next);
        if (!isRefreshTime(next.decision_time))
            throw std::invalid_argument("5E1: publication outside 30-minute session boundary");
        if (active_ && next.decision_time <= active_->decision_time)
            throw std::invalid_argument("5E1: duplicate or retrograde publication");
        auto replacement = std::make_shared<const DevTopCandidateSelection>(next);
        active_ = std::move(replacement);
    }
private:
    static void validate(const DevTopCandidateSelection& s) {
        if (!s.numerical_integrity || s.decision_time <= 0 ||
            s.benchmark_symbol.empty() ||
            s.input_candidate_count < DEV_AGENT_SLOT_COUNT ||
            s.eligible_candidate_count < DEV_AGENT_SLOT_COUNT ||
            s.selected_candidate_count != DEV_AGENT_SLOT_COUNT)
            throw std::invalid_argument("5E1: incomplete ranking");
        std::unordered_set<std::string> seen;
        for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i) {
            const auto& a = s.assignments[i];
            if (!a.assigned || a.slot_index != i ||
                a.agent_name != DevTopCandidateSelector::agentNames()[i] ||
                a.candidate_rank != i + 1 || a.symbol.empty() ||
                a.symbol == s.benchmark_symbol ||
                a.decision_time != s.decision_time ||
                !std::isfinite(a.candidate_score) || !seen.insert(a.symbol).second)
                throw std::invalid_argument("5E1: invalid assignment");
        }
    }
    std::shared_ptr<const DevTopCandidateSelection> active_;
};
} // namespace devai::strategy
