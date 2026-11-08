#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevMultiStockOOSValidator.hpp"
#include "devai/strategy/dev_ai_strategy/DevZerodhaIntradayCosts.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
using DevOOSPriceProvider = std::function<double(const std::string&, std::int64_t)>;
enum class DevOOSCostMode { LEGACY_PERCENT, ZERO_COST, ZERODHA_EQUITY_INTRADAY };
struct DevOOSLedgerConfig {
    double initial_capital{7000.0};
    double max_allocation_fraction{0.75};
    double brokerage_rate{0.0};
    double slippage_rate{0.0};
    double other_cost_rate{0.0};
    int short_square_off_hour_ist{15};
    int short_square_off_minute_ist{15};
    bool allow_intraday_short{false};
    DevOOSCostMode cost_mode{DevOOSCostMode::LEGACY_PERCENT};
    DevExchange exchange{DevExchange::NSE};
    DevZerodhaChargeRates zerodha_rates{};
};
struct DevOOSLedgerState {
    double cash{7000.0};
    double equity{7000.0};
    double peak_equity{7000.0};
    std::string held_symbol;
    std::int64_t quantity{0};
    std::int64_t last_time{0};
    double entry_price{0.0};
    std::int64_t entry_time{0};
    double realized_net_pnl{0.0};
    double restricted_short_proceeds{0.0};
};
class DevRealHistoricalMultiStockOOS {
public:
    explicit DevRealHistoricalMultiStockOOS(DevOOSLedgerConfig config = {});
    [[nodiscard]] const DevOOSLedgerState& state() const noexcept;
    [[nodiscard]] DevOOSExecutionResult execute(std::int64_t decision_time,
        const std::string& desired_symbol, const DevOOSPriceProvider& prices);
    [[nodiscard]] static DevManagerPortfolioState portfolioAt(
        const DevOOSLedgerState& ledger, double marked_price,
        std::int64_t decision_time, double initial_capital);
    [[nodiscard]] static DevOOSDecisionPoint toDecisionPoint(
        const DevHistoricalDecisionRecord& record, const DevManagerPortfolioState& portfolio);
private:
    DevOOSLedgerConfig config_{};
    DevOOSLedgerState state_{};
};
}
