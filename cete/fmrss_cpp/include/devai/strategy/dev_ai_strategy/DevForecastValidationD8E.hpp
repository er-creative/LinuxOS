#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace devai::strategy {
struct DevD8EEvaluationRow {
    std::string symbol;
    std::int64_t decision_key{0}, target_key{0};
    double current_price{0}, predicted_return{0}, predicted_price{0}, actual_price{0};
};
struct DevD8EMetrics {
    std::size_t count{0}, nonzero_predictions{0}, direction_correct{0};
    double return_mae{0}, return_rmse{0}, baseline_rmse{0}, price_rmse{0};
    double correlation{0}, directional_accuracy{0};
    double gross_mean_return{0}, net_mean_return{0}, signal_fraction{0};
};
struct DevD8EReport {
    DevD8EMetrics aggregate;
    std::map<std::string,DevD8EMetrics> per_stock;
};
// cost_bps is the TOTAL hypothetical round-trip cost, not per-side.
// threshold_bps is the minimum predicted return required to enter a long.
// This is a single-horizon independent signal diagnostic, NOT a portfolio backtest.
DevD8EReport devD8EEvaluate(const std::vector<DevD8EEvaluationRow>& rows,
                             double cost_bps, double threshold_bps);
} // namespace devai::strategy
