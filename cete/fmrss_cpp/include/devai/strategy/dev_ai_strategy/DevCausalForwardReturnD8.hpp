#pragma once
#include "devai/strategy/dev_ai_strategy/DevManager001StatisticalStrategyD7.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace devai::strategy {
// Feature vector is known at decision_key. Index 0 is intercept.
using DevD8Features = std::array<double,4>;
struct DevD8Observation {
    std::string symbol;
    std::int64_t decision_key{0};
    double close{0};
    DevD8Features x{};
};
struct DevD8TrainingConfig {
    std::int64_t oos_start_key{0}; // first minute of holdout day
    std::int64_t horizon_minutes{5};
    double ridge_lambda{1.0};
    std::size_t minimum_training_labels{100};
};
struct DevD8FitReport {
    std::size_t training_labels{0};
    std::size_t excluded_missing_future{0};
    std::size_t excluded_crossing_split{0};
};
class DevCausalForwardReturnD8 final {
public:
    explicit DevCausalForwardReturnD8(DevD8TrainingConfig config);
    // OFFLINE ONLY: uses future closes to construct training labels, never inference features.
    DevD8FitReport fitOffline(const std::vector<DevD8Observation>& observations);
    [[nodiscard]] DevD7Forecast predict(const DevHistoricalStockFrame& stock,
                                         const DevHistoricalManagerFrame& frame) const;
    [[nodiscard]] bool fitted() const noexcept { return fitted_; }
    [[nodiscard]] const std::array<double,4>& coefficients() const noexcept {return beta_;}
private:
    DevD8TrainingConfig config_;
    std::array<double,4> beta_{};
    bool fitted_{false};
};
[[nodiscard]] DevD8Observation devD8MakeObservation(const DevHistoricalStockFrame& stock,
                                                    const DevHistoricalManagerFrame& frame);
[[nodiscard]] bool devD8SameSession(std::int64_t a,std::int64_t b);
} // namespace devai::strategy
