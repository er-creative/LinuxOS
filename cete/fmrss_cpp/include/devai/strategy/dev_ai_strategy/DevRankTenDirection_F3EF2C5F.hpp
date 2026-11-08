#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace devai::strategy::f3ef2c5f {
enum class RankTenDirection { DOWNWARD, NEUTRAL, UPWARD };
// Agent Q-values are learned action values, NOT calibrated probabilities.
// This diagnostic compares relative action values; a negative expected return
// must be validated with causal statistical features before live deployment.
inline RankTenDirection classify(const std::array<double, 4>& q, double margin = 0.0) {
    for (double x : q) if (!std::isfinite(x)) throw std::invalid_argument("nonfinite direction Q");
    if (!std::isfinite(margin) || margin < 0) throw std::invalid_argument("invalid direction margin");
    if (q[2] > q[1] + margin && q[2] > q[0] + margin) return RankTenDirection::DOWNWARD;
    if (q[1] > q[2] + margin && q[1] > q[0] + margin) return RankTenDirection::UPWARD;
    return RankTenDirection::NEUTRAL;
}
inline bool shortPermitted(std::size_t slot, RankTenDirection direction,
                           bool short_enabled, bool symbol_shortable, bool may_enter) noexcept {
    return slot == 4 && direction == RankTenDirection::DOWNWARD &&
           short_enabled && symbol_shortable && may_enter;
}
} // namespace devai::strategy::f3ef2c5f
