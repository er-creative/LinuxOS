#pragma once
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace devai::strategy::f3ef2c5a {
// Training-time legal agent actions. This is a reusable policy, NOT an
// automatic modification of DevSequentialRLTrainer's existing training loop.
struct TrainingPolicy {
    bool allow_long = true;
    bool allow_short = false; // opt-in only after directional training/reward integration
    bool shortable = false;
    bool risk_approved = false;
    bool entries_allowed = false;
    bool force_flat = false;
};
inline std::array<bool, DEV_ACTION_COUNT> agentMask(const TrainingPolicy& p, double position) {
    if (!isValidDevPosition(position)) throw std::invalid_argument("invalid position");
    std::array<bool, DEV_ACTION_COUNT> mask{};
    mask[static_cast<std::size_t>(DevAction::HOLD)] = !p.force_flat || position == DEV_POSITION_FLAT;
    mask[static_cast<std::size_t>(DevAction::EXIT)] = position != DEV_POSITION_FLAT;
    const bool entry = p.risk_approved && p.entries_allowed && !p.force_flat;
    // Do not allow implicit same-step reversals; EXIT must be reconciled first.
    mask[static_cast<std::size_t>(DevAction::LONG)] = entry && p.allow_long && position == DEV_POSITION_FLAT;
    mask[static_cast<std::size_t>(DevAction::SHORT)] = entry && p.allow_short && p.shortable && position == DEV_POSITION_FLAT;
    return mask;
}
}
