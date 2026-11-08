#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevMultiStockOOSValidator.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
// Market prices must be timestamp-aligned, known at the decision time and
// sourced from CETE historical candles (never from forward-return labels).
using DevOOSPriceProvider = std::function<double(const std::string&, std::int64_t)>;

struct DevOOSLedgerConfig {
    double initial_capital{7000.0};
    double max_allocation_fraction{0.75};
    double brokerage_rate{0.0};
    double slippage_rate{0.0};
    double other_cost_rate{0.0};
};

struct DevOOSLedgerState {
    double cash{7000.0};
    double equity{7000.0};
    double peak_equity{7000.0};
    std::string held_symbol;
    std::int64_t quantity{0};
    std::int64_t last_time{0};
};

// Independent, shared-cash, long-only ledger. One open stock maximum.
// The desired symbol is an instruction from DevManager, not a prediction.
class DevRealHistoricalMultiStockOOS {
public:
    explicit DevRealHistoricalMultiStockOOS(DevOOSLedgerConfig config = {});
    [[nodiscard]] const DevOOSLedgerState& state() const noexcept;
    [[nodiscard]] DevOOSExecutionResult execute(
        std::int64_t decision_time,
        const std::string& desired_symbol,
        const DevOOSPriceProvider& prices);

    // Convert 3.15B candidate records without copying offline outcomes.
    [[nodiscard]] static DevOOSDecisionPoint toDecisionPoint(
        const DevHistoricalDecisionRecord& record,
        const DevManagerPortfolioState& portfolio);
private:
    DevOOSLedgerConfig config_{};
    DevOOSLedgerState state_{};
};
}
