#pragma once

#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/RewardEngine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy
{

// ============================================================
// EXECUTION RECORD
// ============================================================

struct ExecutionRecord
{
    brokerage::ExecutionSide side{
        brokerage::ExecutionSide::BUY};

    double position_before{0.0};
    double position_after{0.0};

    // Raw observable market price at decision time.
    double market_price{0.0};

    // Actual simulated execution price after slippage.
    double execution_price{0.0};

    double quantity{0.0};
    double turnover{0.0};

    brokerage::TradingCharges charges;

    // Explicit difference between execution at market price
    // and simulated execution price, expressed monetarily.
    double slippage_cost{0.0};

    double total_execution_cost{0.0};
};


// ============================================================
// CONFIGURATION
// ============================================================

struct TradingEnvironmentConfig
{
    double initial_equity{100000.0};

    // 1.0 = full-equity notional.
    double exposure_fraction{1.0};

    // Decimal slippage per execution.
    //
    // 0.0002 = 0.02% = 2 basis points.
    double slippage_rate{0.0002};

    brokerage::BrokerageChargesConfig
        brokerage_config{};

    RewardConfig reward_config{};
};


// ============================================================
// ENVIRONMENT STATE
// ============================================================

struct TradingEnvironmentState
{
    std::string symbol;

    std::int64_t decision_time{0};

    double initial_equity{0.0};
    double equity{0.0};
    double peak_equity{0.0};

    // {-1, 0, +1}
    double position{0.0};

    // Quantity belonging to CURRENT open position.
    double quantity{0.0};

    // Slippage-adjusted execution price of CURRENT position.
    double entry_price{0.0};

    // Last observable market price.
    double previous_market_price{0.0};

    // --------------------------------------------------------
    // Gross accounting
    // --------------------------------------------------------

    double step_gross_pnl{0.0};

    double realized_gross_pnl{0.0};

    double unrealized_gross_pnl{0.0};

    // --------------------------------------------------------
    // Costs
    // --------------------------------------------------------

    double cumulative_brokerage_charges{0.0};

    double cumulative_slippage_cost{0.0};

    double cumulative_execution_cost{0.0};

    // --------------------------------------------------------
    // Net accounting
    // --------------------------------------------------------

    double net_pnl{0.0};

    double drawdown{0.0};

    std::size_t holding_bars{0};

    bool initialized{false};
};


// ============================================================
// STEP RESULT
// ============================================================

struct TradingStepResult
{
    std::int64_t decision_time{0};

    DevAction action{DevAction::HOLD};

    double previous_position{0.0};
    double target_position{0.0};

    double market_price{0.0};

    double gross_pnl{0.0};

    double brokerage_charges{0.0};
    double slippage_cost{0.0};
    double execution_cost{0.0};

    double net_pnl{0.0};

    double net_equity_return{0.0};

    // Exposure-state turnover:
    //
    // 0 = no position change
    // 1 = entry or exit
    // 2 = direct reversal
    double turnover{0.0};

    double equity_before{0.0};
    double equity_after{0.0};

    double previous_drawdown{0.0};
    double current_drawdown{0.0};

    RewardResult reward;

    std::vector<ExecutionRecord>
        executions;

    DevPortfolioState portfolio;
};


// ============================================================
// TRADING ENVIRONMENT
// ============================================================

class TradingEnvironment
{
public:

    explicit TradingEnvironment(
        TradingEnvironmentConfig config = {});


    void reset(
        const std::string& symbol,
        std::int64_t decision_time,
        double initial_market_price);


    // --------------------------------------------------------
    // STEP TIMING
    // --------------------------------------------------------
    //
    // At decision t:
    //
    //   1. Existing position earns movement from previous
    //      market price to current market price.
    //
    //   2. DevAction(t) is then applied at current price.
    //
    //   3. Execution slippage and charges are applied.
    //
    // Therefore Action(t) never earns the already-observed
    // previous -> current movement.
    // --------------------------------------------------------

    [[nodiscard]]
    TradingStepResult step(
        DevAction action,
        std::int64_t decision_time,
        double current_market_price);


    [[nodiscard]]
    const TradingEnvironmentState&
    state() const noexcept;


    [[nodiscard]]
    const TradingEnvironmentConfig&
    config() const noexcept;


    [[nodiscard]]
    DevPortfolioState
    portfolioState() const;


private:

    [[nodiscard]]
    ExecutionRecord executeOpen(
        double new_position,
        double market_price);


    [[nodiscard]]
    ExecutionRecord executeClose(
        double market_price);


    [[nodiscard]]
    brokerage::ExecutionSide
    openingSide(
        double position) const;


    [[nodiscard]]
    brokerage::ExecutionSide
    closingSide(
        double position) const;


    [[nodiscard]]
    double slippageAdjustedPrice(
        brokerage::ExecutionSide side,
        double market_price) const;


    void applyCharges(
        const ExecutionRecord& execution);


    void updateUnrealizedPnl(
        double current_market_price);


    void updateDrawdown();


private:

    TradingEnvironmentConfig config_;

    brokerage::BrokerageChargesEngine
        brokerage_engine_;

    RewardEngine reward_engine_;

    TradingEnvironmentState state_;
};

} // namespace devai::strategy