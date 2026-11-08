#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalMinuteValidator.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace devai::strategy {
// Exchange-local minute completion boundaries encoded as day-ordinal*1440+minute.
// No Unix timezone conversion, no forward-looking observations.
struct DevHistoricalCandle {
    std::int64_t start_key{0}, completed_key{0};
    double open{0}, high{0}, low{0}, close{0}, volume{0};
};
struct DevHistoricalStats {
    std::size_t count{0};
    double mean{0}, standard_deviation{0}, zscore{0};
    bool ready{false};
};
struct DevHistoricalStockFrame {
    std::string symbol;
    DevHistoricalCandle one_minute, five_minute, fifteen_minute;
    DevHistoricalStats close_1m, close_5m, close_15m;
};
struct DevHistoricalManagerFrame {
    std::string manager_id{"DevManager_001"};
    std::int64_t decision_key{0};
    std::vector<DevHistoricalStockFrame> stocks;
};
struct DevHistoricalFeedConfig {
    std::string manager_id{"DevManager_001"};
    std::string benchmark_symbol{"NIFTY%2050"};
    std::size_t rolling_window{20};
    std::size_t minimum_history{20};
    DevHistoricalMinuteValidationConfig sessions{};
};
struct DevHistoricalFeedReport {
    DevHistoricalMinuteValidationReport validation;
    std::size_t eligible_decisions{0};
    std::size_t skipped_incomplete{0};
    std::size_t skipped_warmup{0};
    std::vector<std::string> issues;
};
// Manager-owned consumer receives real, causally completed observations.
// It must supply the strategy's established ordering policy; no ranking score
// or forward-return prediction is invented by this feed.
using DevHistoricalManagerConsumer = std::function<void(const DevHistoricalManagerFrame&)>;
class DevRealHistoricalMultiStockFeed final {
public:
    explicit DevRealHistoricalMultiStockFeed(DevHistoricalFeedConfig config);
    DevHistoricalFeedReport replay(const std::vector<DevHistoricalMinuteInput>& inputs,
                                   const DevHistoricalManagerConsumer& manager_consumer) const;
private:
    DevHistoricalFeedConfig config_;
};
} // namespace devai::strategy
