#pragma once
#include "devai/statistics/MathematicalStateFeatures.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace devai::strategy {
// Stage 6A: causal, deterministic, LONG-only mathematical *candidate* signal.
// No future candles or realized forward returns are accepted by this interface.
// This is a research baseline, not a validated profitable strategy.
struct DevHybridSignalConfig {
    double entry_z_threshold{-1.5};
    double exit_z_threshold{-0.2};
    double max_absolute_z{6.0};
};
struct DevHybridMathematicalSignal {
    std::string symbol;
    std::int64_t decision_time{0};
    bool ready{false};
    double mean_return_1m{0.0};
    double standard_deviation_1m{0.0};
    double z_score_1m{0.0};
    double z_score_5m{0.0};
    double z_score_15m{0.0};
    double opportunity_score{0.0}; // bounded [0,1], NOT a probability
    bool buy_candidate{false};
    bool exit_candidate{false};
};
inline DevHybridMathematicalSignal makeHybridMathematicalSignal(
    const devai::statistics::MathematicalStateFeatures& state,
    const DevHybridSignalConfig& cfg = {}) {
    if (!(std::isfinite(cfg.entry_z_threshold) && std::isfinite(cfg.exit_z_threshold) &&
          std::isfinite(cfg.max_absolute_z) && cfg.entry_z_threshold < cfg.exit_z_threshold &&
          cfg.max_absolute_z > 0.0))
        throw std::invalid_argument("invalid hybrid signal thresholds");
    DevHybridMathematicalSignal result{};
    result.symbol = state.stock_symbol;
    result.decision_time = state.decision_time;
    const auto& a = state.one_minute.return_statistics;
    const auto& b = state.five_minute.return_statistics;
    const auto& c = state.fifteen_minute.return_statistics;
    result.ready = state.decision_time > 0 && !state.stock_symbol.empty() &&
        state.numerical_integrity && state.fully_ready &&
        state.one_minute_ready && state.five_minute_ready && state.fifteen_minute_ready &&
        a.ready && b.ready && c.ready &&
        std::isfinite(a.rolling_mean) && std::isfinite(a.rolling_standard_deviation) &&
        a.rolling_standard_deviation > 0.0 && std::isfinite(a.z_score) &&
        std::isfinite(b.z_score) && std::isfinite(c.z_score) &&
        std::abs(a.z_score) <= cfg.max_absolute_z &&
        std::abs(b.z_score) <= cfg.max_absolute_z &&
        std::abs(c.z_score) <= cfg.max_absolute_z;
    if (!result.ready) return result;
    result.mean_return_1m = a.rolling_mean;
    result.standard_deviation_1m = a.rolling_standard_deviation;
    result.z_score_1m = a.z_score;
    result.z_score_5m = b.z_score;
    result.z_score_15m = c.z_score;
    // Research hypothesis: 1m mean reversion, provided the 5m and 15m
    // returns are not simultaneously deeply negative. No claim of profitability.
    result.buy_candidate = a.z_score <= cfg.entry_z_threshold &&
        b.z_score > cfg.entry_z_threshold && c.z_score > cfg.entry_z_threshold;
    result.exit_candidate = a.z_score >= cfg.exit_z_threshold;
    result.opportunity_score = result.buy_candidate ?
        std::clamp((-a.z_score + b.z_score * 0.1 + c.z_score * 0.1) /
                   cfg.max_absolute_z, 0.0, 1.0) : 0.0;
    return result;
}
} // namespace devai::strategy
