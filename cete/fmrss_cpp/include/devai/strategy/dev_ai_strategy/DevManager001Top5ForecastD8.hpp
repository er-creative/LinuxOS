#pragma once
#include "devai/strategy/dev_ai_strategy/DevCausalForwardReturnD8.hpp"
#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy {
struct DevD8SelectedForecast {
    std::string symbol;
    std::int64_t as_of_key{0};
    std::int64_t target_key{0};
    double ranking_score{0.0};
    double current_price{0.0};
    double predicted_forward_return{0.0}; // fraction
    double predicted_future_price{0.0};
    bool ready{false};
    std::string reason;
};
struct DevD8Top5ForecastFrame {
    std::string manager_id;
    std::int64_t decision_key{0};
    std::size_t evaluated{0};
    std::size_t rejected{0};
    std::vector<DevD8SelectedForecast> selected;
};
// D6 ranking is independent of D8 inference: only Top-5 selected symbols are forecast.
// Invalid individual forecasts are returned as explicit unavailable records.
class DevManager001Top5ForecastD8 final {
public:
    DevManager001Top5ForecastD8(const DevManager001HistoricalRanking& ranker,
                                const DevCausalForwardReturnD8& model,
                                std::int64_t horizon_minutes);
    [[nodiscard]] DevD8Top5ForecastFrame evaluate(const DevHistoricalManagerFrame& frame) const;
private:
    const DevManager001HistoricalRanking& ranker_;
    const DevCausalForwardReturnD8& model_;
    std::int64_t horizon_;
};
// Explicit non-forecast mathematical ranking baseline; replace with frozen production
// ranking policy when ready. It uses current/past z-scores only.
[[nodiscard]] DevManager001Assessment devD8StatisticalRankingPolicy(
    const DevHistoricalStockFrame& stock, const DevHistoricalManagerFrame& frame);
} // namespace devai::strategy
