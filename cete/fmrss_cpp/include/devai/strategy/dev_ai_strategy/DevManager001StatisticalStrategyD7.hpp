#pragma once
#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"
#include <functional>
#include <string>

namespace devai::strategy {
// A causal forecast must be supplied by a trained/frozen forecasting component.
// This D7 adapter never calculates realized future returns or assumes a forecast formula.
struct DevD7Forecast {
    double expected_forward_return{0.0}; // fractional return, e.g. 0.001 = 0.1%
    std::int64_t as_of_key{0};           // last information available to estimator
    bool ready{false};
};
using DevD7ForecastProvider = std::function<DevD7Forecast(
    const DevHistoricalStockFrame&, const DevHistoricalManagerFrame&)>;

// Strategy rules are EXPLICIT, not inferred from a temporary demo ranking.
// Caller selects thresholds and ranking transform after validation.
struct DevD7StrategyConfig {
    std::string manager_id{"DevManager_001"};
    double min_expected_return{0.0};
    double min_abs_z1{0.0}, min_abs_z5{0.0}, min_abs_z15{0.0};
    bool require_same_z_direction{false};
    // true: higher expected forward return ranks first; false: larger absolute forecast ranks first
    bool rank_by_signed_forecast{true};
};
struct DevD7StrategyAssessment {
    DevManager001Assessment ranking;
    DevD7Forecast forecast;
};
class DevManager001StatisticalStrategyD7 final {
public:
    DevManager001StatisticalStrategyD7(DevD7StrategyConfig config,
                                      DevD7ForecastProvider forecast);
    DevD7StrategyAssessment assess(const DevHistoricalStockFrame& stock,
                                    const DevHistoricalManagerFrame& frame) const;
    DevManager001StatisticalPolicy rankingPolicy() const;
private:
    DevD7StrategyConfig config_;
    DevD7ForecastProvider forecast_;
};
} // namespace devai::strategy
