#pragma once
#include "devai/strategy/dev_ai_strategy/DevManager.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerDirectionBridge_F3EF2C5.hpp"
#include <array>
#include <cstddef>

namespace devai::strategy::f3ef2c5a {
// Pure decision adapter. No order submission, model training, or model ownership.
// A SELECT_AGENT manager action chooses a slot; that agent's Q-vector chooses
// LONG/SHORT only when flat. Existing manager retains HOLD/EXIT/SWITCH authority.
struct CandidateExecution {
    double completed_candle_price = 0.0;
    std::uint64_t quantity = 0;
    bool shortable = false;
};
struct Input {
    const DevManagerState* state = nullptr;
    const DevManager* manager = nullptr;
    DevManagerAction manager_action = DevManagerAction::NO_TRADE;
    std::array<CandidateExecution, DEV_AGENT_SLOT_COUNT> execution{};
    f3ef2c5::Context context{};
};
struct Result {
    DevManagerDecision manager_decision{};
    f3ef2c5::Plan plan{};
};
[[nodiscard]] Result decide(const Input& input);
}
