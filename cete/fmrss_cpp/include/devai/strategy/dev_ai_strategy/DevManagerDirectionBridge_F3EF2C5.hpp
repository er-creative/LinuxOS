#pragma once
#include "devai/strategy/dev_ai_strategy/DevIntradayDirectionPolicy_F3EF2C4B.hpp"
#include "devai/strategy/dev_ai_strategy/DevExecutionGatewayD9C.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerDirectionContract_H46.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy::f3ef2c5 {
// A conservative boundary between the existing Top-5/DevAgent shared-model
// pipeline and the existing gateway. No new manager or RL model is created.
// Q-values must be supplied by the SAME trained shared RL model; do not derive
// them from a LONG-only checkpoint or statistical ranking score.
struct AgentAssessment {
    std::string symbol;
    std::size_t rank = 0; // 1..5, from existing causal statistical ranking
    std::int64_t decision_key = 0;
    bool assigned = false;
    bool numerical_integrity = false;
    bool shortable = false;
    bool shared_model_supports_short = false;
    double q_open_long = 0;
    double q_open_short = 0;
    double q_wait = 0;
    double fill_price = 0; // causal TESTING price; not a predicted future fill
    std::uint64_t quantity = 0;
};
struct Context {
    std::string manager_id = "DevManager_001"; // explicit manager identity; unknown IDs rejected
    f3ef2c4b::Config policy{};
    f3ef2c4b::Position position{};
    std::string held_symbol;
    int minute_of_day_ist = 0;
    std::int64_t decision_key = 0;
    bool risk_approved = false;
    bool model_approved = false;
    bool trained_shared_model_supports_short = false;
    bool gateway_short_enabled = false;
    double min_q_advantage = 0.0;
};
enum class PlanKind { WAIT, OPEN_LONG, OPEN_SHORT, EXIT_FIRST };
struct Plan {
    PlanKind kind = PlanKind::WAIT;
    std::string symbol;
    std::uint64_t quantity = 0;
    double price = 0;
    std::string reason;
};

// For existing positions, never submit an OPEN on top of a position.
// EXIT_FIRST means: manager must close and confirm reconciliation, then
// re-evaluate a fresh candidate on a new invocation. Not an atomic switch.
Plan decide(const Context& ctx, const std::vector<AgentAssessment>& top5);

// Convert an approved plan into the existing gateway suggestion. The caller
// supplies a globally unique decision_id; the gateway still applies all guards.
DevTradeSuggestion toGatewaySuggestion(const Plan& plan,
                                      const Context& ctx,
                                      const std::string& decision_id);
}
