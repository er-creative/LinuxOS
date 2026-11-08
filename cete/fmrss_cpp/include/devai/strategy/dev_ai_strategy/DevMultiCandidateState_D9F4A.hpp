#pragma once
#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy::d9f4a {
// All inputs must be observable at decision_key. No realized forward returns.
struct CandidateInput {
    std::string symbol;
    std::int64_t decision_key{};
    int rank{}; // 1..5, assigned by DevManager's statistical ranking
    double zscore_1m{}, zscore_5m{}, zscore_15m{};
    bool forecast_ready{};
    double predicted_forward_return{}; // decimal, e.g. 0.001 = 0.1%; 5-minute D8 forecast
    bool eligible{}; // already screened for budget, cooldown, and per-symbol limits
};
struct PortfolioInput {
    std::int64_t decision_key{};
    bool invested{};
    std::string held_symbol;
    double held_score{};
    int position_age{};
    int minutes_to_close{};
    int entries_remaining{};
    bool forecast_mode{};
};
struct CandidateState {
    std::string symbol; // audit/routing identity, deliberately NOT part of shared Q state key
    std::int64_t decision_key{};
    int rank{};
    double statistical_score{};
    double forecast_component{};
    double combined_score{};
    double predicted_forward_return{};
    bool forecast_ready{};
    bool eligible{};
    d9f::State legacy_state{}; // bridge only: NOT sufficient for D9F.4 candidate comparison
    std::string state_key; // extended, shared, candidate-conditioned representation
};
class DevMultiCandidateState_D9F4A final {
public:
    // Expects 1..5 distinct, ranked, time-aligned candidates; returns rank order.
    [[nodiscard]] static std::vector<CandidateState> build(
        const PortfolioInput& portfolio, const std::vector<CandidateInput>& candidates);
    [[nodiscard]] static std::string key(const CandidateState& state);
};
} // namespace devai::strategy::d9f4a
