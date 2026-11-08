#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace devai::strategy {
struct DevHistoricalRiskLimits_H47_6C1G2B1 {
    // Zero disables the corresponding loss limit. Values are fractions, e.g. 0.02 = 2%.
    double max_daily_loss_fraction{0.0};
    double max_weekly_loss_fraction{0.0};
    double max_drawdown_fraction{0.0};
};

class DevHistoricalExecutionRiskGate_H47_6C1G2B1 final {
public:
    explicit DevHistoricalExecutionRiskGate_H47_6C1G2B1(
        DevHistoricalRiskLimits_H47_6C1G2B1 limits = {}) : limits_(limits) {
        validate(limits_.max_daily_loss_fraction);
        validate(limits_.max_weekly_loss_fraction);
        validate(limits_.max_drawdown_fraction);
    }
    // Observe PRE-DECISION marked equity; when a limit trips, liquidate and
    // block new entries until the next session (drawdown is persistent).
    bool observe(std::int64_t t, double equity) {
        if (t <= 0 || !std::isfinite(equity) || equity <= 0)
            throw std::invalid_argument("6C1G2B1: invalid timestamp/equity");
        if (last_time_ && t <= last_time_)
            throw std::invalid_argument("6C1G2B1: nonchronological risk observation");
        const auto local_day = floorDiv(t + 19800, 86400);
        const auto local_week = floorDiv(local_day + 3, 7); // Monday-start ISO-like weeks
        if (!initialized_ || local_day != day_) {
            day_ = local_day;
            day_start_ = equity;
            day_blocked_ = false;
        }
        if (!initialized_ || local_week != week_) {
            week_ = local_week;
            week_start_ = equity;
            week_blocked_ = false;
        }
        peak_ = initialized_ ? (equity > peak_ ? equity : peak_) : equity;
        initialized_ = true;
        last_time_ = t;
        if (limits_.max_daily_loss_fraction > 0 && equity <= day_start_ * (1.0 - limits_.max_daily_loss_fraction)) day_blocked_ = true;
        if (limits_.max_weekly_loss_fraction > 0 && equity <= week_start_ * (1.0 - limits_.max_weekly_loss_fraction)) week_blocked_ = true;
        if (limits_.max_drawdown_fraction > 0 && equity <= peak_ * (1.0 - limits_.max_drawdown_fraction)) drawdown_blocked_ = true;
        return !(day_blocked_ || week_blocked_ || drawdown_blocked_);
    }
    bool blocked() const noexcept { return day_blocked_ || week_blocked_ || drawdown_blocked_; }
private:
    static void validate(double x) {
        if (!std::isfinite(x) || x < 0 || x >= 1)
            throw std::invalid_argument("6C1G2B1: risk limit must be in [0,1)");
    }
    static std::int64_t floorDiv(std::int64_t n, std::int64_t d) noexcept {
        const auto q = n / d, r = n % d;
        return q - (r < 0 ? 1 : 0);
    }
    DevHistoricalRiskLimits_H47_6C1G2B1 limits_{};
    std::int64_t last_time_{}, day_{}, week_{};
    double day_start_{}, week_start_{}, peak_{};
    bool initialized_{false}, day_blocked_{false}, week_blocked_{false}, drawdown_blocked_{false};
};
} // namespace devai::strategy
