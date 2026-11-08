#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace devai::strategy::f3ef2c5d {
// A pure, auditable boundary between manager slot selection, agent direction,
// and the historical ledger's target-symbol protocol.
struct Input {
    std::size_t manager_action = 0; // 0 WAIT, 1 KEEP, 2..6 agent slot
    std::size_t agent_action = 0;   // 0 HOLD, 1 LONG, 2 SHORT, 3 EXIT
    std::string candidate;
    std::string held;
    std::int64_t signed_quantity = 0;
    bool short_enabled = false;
    bool candidate_shortable = false;
    bool model_short_verified = false;
    int minute_ist = 0;
};
inline std::string preserve(const Input& x) {
    if (x.signed_quantity == 0) return {};
    if (x.held.empty()) throw std::invalid_argument("held quantity missing symbol");
    return x.signed_quantity < 0 ? "SHORT:" + x.held : x.held;
}
inline std::string resolve(const Input& x) {
    if (x.manager_action > 6 || x.agent_action > 3 || x.minute_ist < 0 || x.minute_ist >= 1440)
        throw std::invalid_argument("invalid frozen action or time");
    if ((x.signed_quantity == 0) != x.held.empty())
        throw std::invalid_argument("inconsistent portfolio");
    // Cash-market MIS square-off: force both LONG and SHORT flat by 15:15 IST.
    if (x.minute_ist >= 15 * 60 + 15) return {};
    const auto current = preserve(x);
    if (x.manager_action == 0 || x.manager_action == 1) return current;
    if (x.candidate.empty()) throw std::invalid_argument("selected agent has no candidate");
    if (x.signed_quantity != 0) {
        // No atomic switch/reversal. Only the held symbol's EXIT can close.
        if (x.candidate == x.held && x.agent_action == 3) return {};
        return current;
    }
    if (x.minute_ist < 9 * 60 + 15 || x.minute_ist >= 14 * 60 + 45) return {};
    if (x.agent_action == 1) return x.candidate;
    if (x.agent_action == 2 && x.short_enabled && x.candidate_shortable && x.model_short_verified)
        return "SHORT:" + x.candidate;
    return {};
}
} // namespace devai::strategy::f3ef2c5d
