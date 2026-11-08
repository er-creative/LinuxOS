#include "devai/strategy/dev_ai_strategy/DevIntradayPolicyD9A.hpp"
#include <cmath>
#include <stdexcept>

namespace devai::strategy {
namespace {
int minuteOfDay(std::int64_t key) {
    if (key <= 0) throw std::invalid_argument("D9A invalid decision key");
    return static_cast<int>(((key % 1440) + 1440) % 1440);
}
bool validProbability(double x) { return std::isfinite(x) && x >= 0.0 && x <= 1.0; }
}
DevIntradayPolicyD9A::DevIntradayPolicyD9A(DevD9ATradingConfig config) : config_(config) {
    if (!config.intraday_only || config.allow_overnight ||
        config.force_exit_minutes < 0 || config.force_exit_minutes >= 1440 ||
        config.new_entry_cutoff_minutes < 0 ||
        config.new_entry_cutoff_minutes > config.force_exit_minutes ||
        !validProbability(config.minimum_long_profit_probability) ||
        !validProbability(config.minimum_short_profit_probability))
        throw std::invalid_argument("D9A invalid intraday configuration");
}
DevD9AResult DevIntradayPolicyD9A::evaluate(const DevD9AInput& x) const {
    const int minute = minuteOfDay(x.decision_key);
    // Forced liquidation has precedence over probabilities, permissions, and risk.
    if (minute >= config_.force_exit_minutes) {
        return x.position == DevD9APosition::FLAT
            ? DevD9AResult{DevD9AAction::HOLD_FLAT, "past mandatory exit; entries forbidden"}
            : DevD9AResult{DevD9AAction::FORCE_EXIT, "mandatory 15:15 intraday liquidation"};
    }
    if (!validProbability(x.long_profit_probability) || !validProbability(x.short_profit_probability) ||
        !std::isfinite(x.expected_long_net_return) || !std::isfinite(x.expected_short_net_return))
        throw std::invalid_argument("D9A invalid model estimates");
    const bool long_ok = config_.enable_long && x.risk_allows_new_entry &&
        minute < config_.new_entry_cutoff_minutes &&
        x.long_profit_probability >= config_.minimum_long_profit_probability &&
        x.expected_long_net_return > 0.0;
    const bool short_ok = config_.enable_short && x.stock_shortable && x.risk_allows_new_entry &&
        minute < config_.new_entry_cutoff_minutes &&
        x.short_profit_probability >= config_.minimum_short_profit_probability &&
        x.expected_short_net_return > 0.0;
    if (x.position == DevD9APosition::LONG) {
        if (!config_.enable_long) return {DevD9AAction::CLOSE_LONG, "long direction disabled"};
        if (short_ok && x.expected_short_net_return > x.expected_long_net_return)
            return {DevD9AAction::CLOSE_LONG, "short preferred; close before separate short entry"};
        return {DevD9AAction::HOLD_LONG, "existing long; position management deferred to D9B"};
    }
    if (x.position == DevD9APosition::SHORT) {
        if (!config_.enable_short || !x.stock_shortable)
            return {DevD9AAction::CLOSE_SHORT, "short direction disabled or unavailable"};
        if (long_ok && x.expected_long_net_return > x.expected_short_net_return)
            return {DevD9AAction::CLOSE_SHORT, "long preferred; close before separate long entry"};
        return {DevD9AAction::HOLD_SHORT, "existing short; position management deferred to D9B"};
    }
    if (long_ok && (!short_ok || x.expected_long_net_return >= x.expected_short_net_return))
        return {DevD9AAction::OPEN_LONG, "permitted long with positive expected net return"};
    if (short_ok)
        return {DevD9AAction::OPEN_SHORT, "permitted short with positive expected net return"};
    return {DevD9AAction::HOLD_FLAT, "no permitted positive-expectancy entry"};
}
} // namespace devai::strategy
