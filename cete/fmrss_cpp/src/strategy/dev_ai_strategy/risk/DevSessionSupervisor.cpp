#include "devai/strategy/dev_ai_strategy/risk/DevSessionSupervisor.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy::session {
int minuteIST(std::int64_t t) noexcept {
    const auto local = ((t + 19800) % 86400 + 86400) % 86400;
    return static_cast<int>(local / 60);
}
bool entryAllowed(std::int64_t t, const Config& cfg) {
    return minuteIST(t) < cfg.entry_cutoff_minute_ist;
}
bool isNewPositionOrSwitch(const std::string& held, const std::string& desired) {
    return !desired.empty() && desired != held;
}
bool targetAllowed(const std::string& held, const std::string& desired,
                   std::int64_t t, const Config& cfg) {
    if (minuteIST(t) >= cfg.liquidation_deadline_minute_ist)
        return desired.empty();
    return !isNewPositionOrSwitch(held, desired) || entryAllowed(t, cfg);
}
const char* toString(Priority p) noexcept {
    switch (p) {
        case Priority::NORMAL: return "NORMAL";
        case Priority::REVIEW: return "REVIEW";
        case Priority::ELEVATED: return "ELEVATED";
        case Priority::URGENT: return "URGENT";
        case Priority::MANDATORY: return "MANDATORY";
    }
    return "UNKNOWN";
}
const char* toString(Recommendation r) noexcept {
    switch (r) {
        case Recommendation::HOLD_REVIEW: return "HOLD_REVIEW";
        case Recommendation::EXIT_RECOMMENDED: return "EXIT_RECOMMENDED";
        case Recommendation::EXIT_REQUIRED: return "EXIT_REQUIRED";
    }
    return "UNKNOWN";
}
Assessment assess(const PositionSnapshot& p, const Config& cfg) {
    Assessment a;
    const int now = minuteIST(p.decision_time_utc);
    a.minutes_to_deadline = std::max(0, cfg.liquidation_deadline_minute_ist - now);
    if (!p.invested) return a;
    if (p.entry_time_utc > p.decision_time_utc)
        throw std::invalid_argument("entry timestamp later than decision timestamp");
    a.age_minutes = static_cast<int>((p.decision_time_utc - p.entry_time_utc) / 60);
    if (!(std::isfinite(p.last_price) && p.last_price > 0 &&
          std::isfinite(p.entry_price) && p.entry_price > 0))
        throw std::invalid_argument("invalid position prices");
    a.stop_triggered = p.stop_loss_price > 0 && p.last_price <= p.stop_loss_price;
    const double high = p.highest_price_since_entry;
    const double low = p.lowest_price_since_entry;
    if (high > 0 && low > 0 && high >= low) {
        const double range = (high - low) / p.entry_price;
        a.stagnant = range <= cfg.sideways_range_fraction;
        const double net_change = (p.last_price - p.entry_price) / p.entry_price;
        if (a.stagnant) a.direction = Direction::SIDEWAYS;
        else if (net_change > cfg.sideways_range_fraction) a.direction = Direction::UP;
        else if (net_change < -cfg.sideways_range_fraction) a.direction = Direction::DOWN;
    }
    if (a.stop_triggered || now >= cfg.liquidation_deadline_minute_ist) {
        a.priority = Priority::MANDATORY;
        a.recommendation = Recommendation::EXIT_REQUIRED;
        a.message = a.stop_triggered ? "STOP_LOSS_TRIGGERED" : "LIQUIDATION_DEADLINE";
        return a;
    }
    if (a.minutes_to_deadline <= cfg.liquidation_buffer_minutes) {
        a.priority = Priority::URGENT;
        a.recommendation = Recommendation::EXIT_RECOMMENDED;
        a.message = "LIQUIDATION_BUFFER: start prompt exit handling; verify actual fills";
        return a;
    }
    if (a.age_minutes >= cfg.stagnation_review_minutes && a.stagnant) {
        a.priority = Priority::REVIEW;
        a.message = "STAGNANT_POSITION: compare causal HOLD and EXIT net outcomes";
        if (p.forecast_available &&
            p.expected_hold_net_advantage_fraction - p.estimated_downside_risk_fraction <=
                cfg.minimum_expected_net_advantage_fraction) {
            a.priority = Priority::ELEVATED;
            a.recommendation = Recommendation::EXIT_RECOMMENDED;
            a.message = "STAGNANT_WEAK_OUTLOOK: exit recommended, not automatically forced";
        }
    } else {
        a.message = "ROUTINE_POSITION_MONITORING";
    }
    return a;
}
} // namespace devai::strategy::session
