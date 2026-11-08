#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>


namespace devai::strategy
{

// ============================================================================
// Constructor
// ============================================================================

TradingEnvironment::TradingEnvironment(
    TradingEnvironmentConfig config)
    :
    config_(
        std::move(config)),
    brokerage_engine_(
        config_.brokerage_config),
    reward_engine_(
        config_.reward_config)
{
    if (!std::isfinite(
            config_.initial_equity) ||
        config_.initial_equity <= 0.0)
    {
        throw std::invalid_argument(
            "Initial equity must be finite and positive.");
    }


    if (!std::isfinite(
            config_.exposure_fraction) ||
        config_.exposure_fraction <= 0.0)
    {
        throw std::invalid_argument(
            "Exposure fraction must be finite and positive.");
    }


    if (!std::isfinite(
            config_.slippage_rate) ||
        config_.slippage_rate < 0.0 ||
        config_.slippage_rate >= 1.0)
    {
        throw std::invalid_argument(
            "Slippage rate must be finite and in [0,1).");
    }
}


// ============================================================================
// Reset
// ============================================================================

void TradingEnvironment::reset(
    const std::string& symbol,
    std::int64_t decision_time,
    double initial_market_price)
{
    if (symbol.empty())
    {
        throw std::invalid_argument(
            "TradingEnvironment symbol must not be empty.");
    }


    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "TradingEnvironment decision time must be positive.");
    }


    if (!std::isfinite(
            initial_market_price) ||
        initial_market_price <= 0.0)
    {
        throw std::invalid_argument(
            "Initial market price must be finite and positive.");
    }


    state_ =
        TradingEnvironmentState{};


    state_.symbol =
        symbol;

    state_.decision_time =
        decision_time;

    state_.initial_equity =
        config_.initial_equity;

    state_.equity =
        config_.initial_equity;

    state_.peak_equity =
        config_.initial_equity;

    state_.position =
        DEV_POSITION_FLAT;

    state_.quantity =
        0.0;

    state_.entry_price =
        0.0;

    state_.previous_market_price =
        initial_market_price;

    state_.step_gross_pnl =
        0.0;

    state_.realized_gross_pnl =
        0.0;

    state_.unrealized_gross_pnl =
        0.0;

    state_.cumulative_brokerage_charges =
        0.0;

    state_.cumulative_slippage_cost =
        0.0;

    state_.cumulative_execution_cost =
        0.0;

    state_.net_pnl =
        0.0;

    state_.drawdown =
        0.0;

    state_.holding_bars =
        0;

    state_.initialized =
        true;
}


// ============================================================================
// Opening execution side
// ============================================================================

brokerage::ExecutionSide
TradingEnvironment::openingSide(
    double position) const
{
    if (position ==
        DEV_POSITION_LONG)
    {
        return
            brokerage::ExecutionSide::BUY;
    }


    if (position ==
        DEV_POSITION_SHORT)
    {
        return
            brokerage::ExecutionSide::SELL;
    }


    throw std::invalid_argument(
        "Opening position must be LONG or SHORT.");
}


// ============================================================================
// Closing execution side
// ============================================================================

brokerage::ExecutionSide
TradingEnvironment::closingSide(
    double position) const
{
    if (position ==
        DEV_POSITION_LONG)
    {
        return
            brokerage::ExecutionSide::SELL;
    }


    if (position ==
        DEV_POSITION_SHORT)
    {
        return
            brokerage::ExecutionSide::BUY;
    }


    throw std::invalid_argument(
        "Closing position must be LONG or SHORT.");
}


// ============================================================================
// Slippage-adjusted execution price
//
// BUY:
//     market * (1 + slippage)
//
// SELL:
//     market * (1 - slippage)
//
// Both are adverse execution assumptions.
// ============================================================================

double
TradingEnvironment::slippageAdjustedPrice(
    brokerage::ExecutionSide side,
    double market_price) const
{
    if (!std::isfinite(
            market_price) ||
        market_price <= 0.0)
    {
        throw std::invalid_argument(
            "Market price must be finite and positive.");
    }


    double execution_price =
        market_price;


    if (side ==
        brokerage::ExecutionSide::BUY)
    {
        execution_price =
            market_price *
            (1.0 +
             config_.slippage_rate);
    }
    else
    {
        execution_price =
            market_price *
            (1.0 -
             config_.slippage_rate);
    }


    if (!std::isfinite(
            execution_price) ||
        execution_price <= 0.0)
    {
        throw std::runtime_error(
            "Invalid slippage-adjusted execution price.");
    }


    return execution_price;
}


// ============================================================================
// Apply brokerage / statutory charges
//
// IMPORTANT:
//
// Slippage is NOT subtracted here.
//
// Slippage is economically represented through execution price.
//
// Opening slippage:
//     applied in executeOpen()
//
// Closing slippage:
//     applied in executeClose()
//
// execution.slippage_cost remains a reporting / diagnostics value.
// ============================================================================

void TradingEnvironment::applyCharges(
    const ExecutionRecord& execution)
{
    if (!std::isfinite(
            execution.charges.total) ||
        execution.charges.total < 0.0)
    {
        throw std::runtime_error(
            "Execution contains invalid brokerage charges.");
    }


    if (!std::isfinite(
            execution.slippage_cost) ||
        execution.slippage_cost < 0.0)
    {
        throw std::runtime_error(
            "Execution contains invalid slippage cost.");
    }


    if (!std::isfinite(
            execution.total_execution_cost) ||
        execution.total_execution_cost < 0.0)
    {
        throw std::runtime_error(
            "Execution contains invalid total execution cost.");
    }


    // ------------------------------------------------------------------------
    // Only explicit brokerage/statutory charges are directly deducted here.
    //
    // Slippage is already represented through execution price.
    // ------------------------------------------------------------------------

    state_.equity -=
        execution.charges.total;


    state_.cumulative_brokerage_charges +=
        execution.charges.total;


    state_.cumulative_slippage_cost +=
        execution.slippage_cost;


    state_.cumulative_execution_cost +=
        execution.total_execution_cost;


    if (!std::isfinite(
            state_.equity) ||
        state_.equity <= 0.0)
    {
        throw std::runtime_error(
            "TradingEnvironment equity invalid after charges.");
    }
}


// ============================================================================
// Open position
//
// CRITICAL ACCOUNTING RULE:
//
// The new position executes at execution_price, not market_price.
//
// Therefore:
//
// LONG:
//     execution > market
//     immediate entry slippage P&L < 0
//
// SHORT:
//     execution < market
//     immediate entry slippage P&L < 0
//
// The economic entry-slippage loss is booked into equity exactly once.
//
// We DO NOT additionally subtract execution.slippage_cost.
// ============================================================================

ExecutionRecord
TradingEnvironment::executeOpen(
    double new_position,
    double market_price)
{
    if (state_.position !=
        DEV_POSITION_FLAT)
    {
        throw std::logic_error(
            "Cannot open position while another position is active.");
    }


    if (new_position !=
            DEV_POSITION_LONG &&
        new_position !=
            DEV_POSITION_SHORT)
    {
        throw std::invalid_argument(
            "New position must be LONG or SHORT.");
    }


    if (!std::isfinite(
            market_price) ||
        market_price <= 0.0)
    {
        throw std::invalid_argument(
            "Open market price must be finite and positive.");
    }


    ExecutionRecord execution;


    execution.position_before =
        DEV_POSITION_FLAT;

    execution.position_after =
        new_position;

    execution.side =
        openingSide(
            new_position);

    execution.market_price =
        market_price;


    execution.execution_price =
        slippageAdjustedPrice(
            execution.side,
            market_price);


    // ------------------------------------------------------------------------
    // Exposure sizing
    //
    // Quantity is determined from observable market price.
    // ------------------------------------------------------------------------

    const double target_notional =
        state_.equity *
        config_.exposure_fraction;


    if (!std::isfinite(
            target_notional) ||
        target_notional <= 0.0)
    {
        throw std::runtime_error(
            "Invalid target notional.");
    }


    execution.quantity =
        target_notional /
        market_price;


    if (!std::isfinite(
            execution.quantity) ||
        execution.quantity <= 0.0)
    {
        throw std::runtime_error(
            "Invalid opening quantity.");
    }


    execution.turnover =
        execution.quantity *
        execution.execution_price;


    if (!std::isfinite(
            execution.turnover) ||
        execution.turnover <= 0.0)
    {
        throw std::runtime_error(
            "Invalid opening turnover.");
    }


    // ------------------------------------------------------------------------
    // Explicit slippage amount for reporting.
    // ------------------------------------------------------------------------

    execution.slippage_cost =
        execution.quantity *
        std::fabs(
            execution.execution_price -
            execution.market_price);


    if (!std::isfinite(
            execution.slippage_cost) ||
        execution.slippage_cost < 0.0)
    {
        throw std::runtime_error(
            "Invalid opening slippage cost.");
    }


    // ------------------------------------------------------------------------
    // Shared CETE brokerage / statutory charges.
    // ------------------------------------------------------------------------

    execution.charges =
        brokerage_engine_.calculate(
            execution.side,
            execution.turnover);


    execution.total_execution_cost =
        execution.charges.total +
        execution.slippage_cost;


    if (!std::isfinite(
            execution.total_execution_cost) ||
        execution.total_execution_cost < 0.0)
    {
        throw std::runtime_error(
            "Invalid opening execution cost.");
    }


    // ========================================================================
    // CRITICAL FIX — OPENING SLIPPAGE EQUITY ACCOUNTING
    // ========================================================================
    //
    // LONG:
    //
    //   +1 * quantity * (market - higher BUY execution)
    //   = negative
    //
    // SHORT:
    //
    //   -1 * quantity * (market - lower SELL execution)
    //   = negative
    //
    // Therefore both directions correctly incur adverse entry slippage.
    //
    // Do NOT subtract execution.slippage_cost separately.
    // ========================================================================

    const double entry_slippage_pnl =
        new_position *
        execution.quantity *
        (
            market_price -
            execution.execution_price
        );


    if (!std::isfinite(
            entry_slippage_pnl))
    {
        throw std::runtime_error(
            "Invalid opening slippage P&L.");
    }


    state_.equity +=
        entry_slippage_pnl;


    if (!std::isfinite(
            state_.equity) ||
        state_.equity <= 0.0)
    {
        throw std::runtime_error(
            "TradingEnvironment equity invalid "
            "after opening slippage.");
    }


    // ------------------------------------------------------------------------
    // Apply brokerage/statutory charges.
    //
    // Slippage is NOT deducted again here.
    // ------------------------------------------------------------------------

    applyCharges(
        execution);


    // ------------------------------------------------------------------------
    // Establish position at actual execution price.
    // ------------------------------------------------------------------------

    state_.position =
        new_position;

    state_.quantity =
        execution.quantity;

    state_.entry_price =
        execution.execution_price;


    // ------------------------------------------------------------------------
    // Mark open position to observable market price.
    //
    // At the instant of entry this equals the economic slippage loss.
    // ------------------------------------------------------------------------

    state_.unrealized_gross_pnl =
        state_.position *
        state_.quantity *
        (
            market_price -
            state_.entry_price
        );


    if (!std::isfinite(
            state_.unrealized_gross_pnl))
    {
        throw std::runtime_error(
            "Invalid unrealized P&L after opening.");
    }


    state_.holding_bars =
        0;


    return execution;
}


// ============================================================================
// Close position
//
// Before this function is called, step() has already marked the existing
// position from previous_market_price to current market_price.
//
// Therefore this function only needs to account for the difference:
//
//     current observable market price
//          -> actual closing execution price
//
// That difference is closing slippage.
// ============================================================================

ExecutionRecord
TradingEnvironment::executeClose(
    double market_price)
{
    if (state_.position ==
        DEV_POSITION_FLAT)
    {
        throw std::logic_error(
            "Cannot close a flat position.");
    }


    if (!std::isfinite(
            market_price) ||
        market_price <= 0.0)
    {
        throw std::invalid_argument(
            "Close market price must be finite and positive.");
    }


    const double old_position =
        state_.position;

    const double old_quantity =
        state_.quantity;

    const double old_entry_price =
        state_.entry_price;


    if (old_quantity <= 0.0 ||
        !std::isfinite(
            old_quantity))
    {
        throw std::runtime_error(
            "Open position contains invalid quantity.");
    }


    if (old_entry_price <= 0.0 ||
        !std::isfinite(
            old_entry_price))
    {
        throw std::runtime_error(
            "Open position contains invalid entry price.");
    }


    ExecutionRecord execution;


    execution.position_before =
        old_position;

    execution.position_after =
        DEV_POSITION_FLAT;

    execution.side =
        closingSide(
            old_position);

    execution.market_price =
        market_price;


    execution.execution_price =
        slippageAdjustedPrice(
            execution.side,
            market_price);


    execution.quantity =
        old_quantity;


    execution.turnover =
        execution.quantity *
        execution.execution_price;


    if (!std::isfinite(
            execution.turnover) ||
        execution.turnover <= 0.0)
    {
        throw std::runtime_error(
            "Invalid closing turnover.");
    }


    execution.slippage_cost =
        execution.quantity *
        std::fabs(
            execution.execution_price -
            execution.market_price);


    if (!std::isfinite(
            execution.slippage_cost) ||
        execution.slippage_cost < 0.0)
    {
        throw std::runtime_error(
            "Invalid closing slippage cost.");
    }


    execution.charges =
        brokerage_engine_.calculate(
            execution.side,
            execution.turnover);


    execution.total_execution_cost =
        execution.charges.total +
        execution.slippage_cost;


    if (!std::isfinite(
            execution.total_execution_cost) ||
        execution.total_execution_cost < 0.0)
    {
        throw std::runtime_error(
            "Invalid closing execution cost.");
    }


    // ========================================================================
    // Closing slippage
    //
    // Equity has already been marked to current market price by step().
    //
    // We now reconcile market price -> actual execution price.
    // ========================================================================

    const double exit_slippage_pnl =
        old_position *
        old_quantity *
        (
            execution.execution_price -
            market_price
        );


    if (!std::isfinite(
            exit_slippage_pnl))
    {
        throw std::runtime_error(
            "Invalid closing slippage P&L.");
    }


    state_.equity +=
        exit_slippage_pnl;


    if (!std::isfinite(
            state_.equity) ||
        state_.equity <= 0.0)
    {
        throw std::runtime_error(
            "TradingEnvironment equity invalid "
            "after closing slippage.");
    }


    // ------------------------------------------------------------------------
    // Closed-trade gross P&L using actual execution prices.
    //
    // Entry and exit slippage are therefore represented in the trade's
    // realized gross P&L.
    //
    // This value is accumulated for reporting only. It is NOT added to
    // equity here because equity has already been marked incrementally.
    // ------------------------------------------------------------------------

    const double realized_trade_gross_pnl =
        old_position *
        old_quantity *
        (
            execution.execution_price -
            old_entry_price
        );


    if (!std::isfinite(
            realized_trade_gross_pnl))
    {
        throw std::runtime_error(
            "Invalid realized trade P&L.");
    }


    state_.realized_gross_pnl +=
        realized_trade_gross_pnl;


    // ------------------------------------------------------------------------
    // Brokerage/statutory charges.
    //
    // Closing slippage has already been economically applied above.
    // ------------------------------------------------------------------------

    applyCharges(
        execution);


    // ------------------------------------------------------------------------
    // Flat position.
    // ------------------------------------------------------------------------

    state_.position =
        DEV_POSITION_FLAT;

    state_.quantity =
        0.0;

    state_.entry_price =
        0.0;

    state_.unrealized_gross_pnl =
        0.0;

    state_.holding_bars =
        0;


    return execution;
}


// ============================================================================
// Update unrealized P&L
//
// This is reporting state.
//
// Equity itself is maintained incrementally in step().
// ============================================================================

void TradingEnvironment::updateUnrealizedPnl(
    double current_market_price)
{
    if (!std::isfinite(
            current_market_price) ||
        current_market_price <= 0.0)
    {
        throw std::invalid_argument(
            "Current market price must be finite and positive.");
    }


    if (state_.position ==
        DEV_POSITION_FLAT)
    {
        state_.unrealized_gross_pnl =
            0.0;

        return;
    }


    state_.unrealized_gross_pnl =
        state_.position *
        state_.quantity *
        (
            current_market_price -
            state_.entry_price
        );


    if (!std::isfinite(
            state_.unrealized_gross_pnl))
    {
        throw std::runtime_error(
            "Invalid unrealized P&L.");
    }
}


// ============================================================================
// Update drawdown
// ============================================================================

void TradingEnvironment::updateDrawdown()
{
    if (!std::isfinite(
            state_.equity) ||
        state_.equity <= 0.0)
    {
        throw std::runtime_error(
            "Cannot calculate drawdown from invalid equity.");
    }


    if (!std::isfinite(
            state_.peak_equity) ||
        state_.peak_equity <= 0.0)
    {
        throw std::runtime_error(
            "Cannot calculate drawdown from invalid peak equity.");
    }


    if (state_.equity >
        state_.peak_equity)
    {
        state_.peak_equity =
            state_.equity;
    }


    state_.drawdown =
        (
            state_.peak_equity -
            state_.equity
        ) /
        state_.peak_equity;


    if (!std::isfinite(
            state_.drawdown))
    {
        throw std::runtime_error(
            "Invalid drawdown.");
    }


    if (state_.drawdown < 0.0)
    {
        state_.drawdown =
            0.0;
    }
}


// ============================================================================
// Step
//
// Causal order:
//
// 1. Existing position earns price(t-1) -> price(t)
// 2. Current market is now observable
// 3. DevAction(t) determines target exposure
// 4. Position change executes at t
// 5. New position cannot earn anything until a later price arrives
// 6. Costs are applied
// 7. Reward is calculated from resulting net equity return
//
// This prevents same-step look-ahead profit.
// ============================================================================

TradingStepResult
TradingEnvironment::step(
    DevAction action,
    std::int64_t decision_time,
    double current_market_price)
{
    if (!state_.initialized)
    {
        throw std::logic_error(
            "TradingEnvironment must be reset before step().");
    }


    if (!isValidAction(
            action))
    {
        throw std::invalid_argument(
            "Invalid Dev action.");
    }


    if (decision_time <=
        state_.decision_time)
    {
        throw std::invalid_argument(
            "Decision time must move strictly forward.");
    }


    if (!std::isfinite(
            current_market_price) ||
        current_market_price <= 0.0)
    {
        throw std::invalid_argument(
            "Current market price must be finite and positive.");
    }


    TradingStepResult result;


    result.decision_time =
        decision_time;

    result.action =
        action;

    result.market_price =
        current_market_price;

    result.previous_position =
        state_.position;

    result.equity_before =
        state_.equity;

    result.previous_drawdown =
        state_.drawdown;


    // ========================================================================
    // 1. Existing position earns only the newly observed market movement.
    // ========================================================================

    state_.step_gross_pnl =
        state_.position *
        state_.quantity *
        (
            current_market_price -
            state_.previous_market_price
        );


    if (!std::isfinite(
            state_.step_gross_pnl))
    {
        throw std::runtime_error(
            "Invalid step gross P&L.");
    }


    state_.equity +=
        state_.step_gross_pnl;


    if (!std::isfinite(
            state_.equity) ||
        state_.equity <= 0.0)
    {
        throw std::runtime_error(
            "TradingEnvironment equity invalid "
            "after market movement.");
    }


    result.gross_pnl =
        state_.step_gross_pnl;


    // ========================================================================
    // 2. Existing holding period advances.
    // ========================================================================

    if (state_.position !=
        DEV_POSITION_FLAT)
    {
        ++state_.holding_bars;
    }


    // ========================================================================
    // 3. Mark existing position to current observable market price.
    // ========================================================================

    updateUnrealizedPnl(
        current_market_price);


    // ========================================================================
    // 4. Determine desired target position.
    // ========================================================================

    result.target_position =
        targetPosition(
            action,
            state_.position);


    result.turnover =
        actionTurnover(
            result.previous_position,
            result.target_position);


    // ========================================================================
    // 5. Execute position change.
    //
    // Reversal:
    //
    // LONG -> SHORT
    //     close LONG
    //     open SHORT
    //
    // SHORT -> LONG
    //     close SHORT
    //     open LONG
    // ========================================================================

    if (result.target_position !=
        result.previous_position)
    {
        if (result.previous_position !=
            DEV_POSITION_FLAT)
        {
            result.executions.push_back(
                executeClose(
                    current_market_price));
        }


        if (result.target_position !=
            DEV_POSITION_FLAT)
        {
            result.executions.push_back(
                executeOpen(
                    result.target_position,
                    current_market_price));
        }
    }


    // ========================================================================
    // 6. Aggregate execution costs for this step.
    // ========================================================================

    result.brokerage_charges =
        0.0;

    result.slippage_cost =
        0.0;

    result.execution_cost =
        0.0;


    for (const auto& execution :
         result.executions)
    {
        result.brokerage_charges +=
            execution.charges.total;


        result.slippage_cost +=
            execution.slippage_cost;


        result.execution_cost +=
            execution.total_execution_cost;
    }


    // ========================================================================
    // 7. Account-level net P&L
    // ========================================================================

    state_.net_pnl =
        state_.equity -
        state_.initial_equity;


    result.equity_after =
        state_.equity;


    result.net_pnl =
        result.equity_after -
        result.equity_before;


    if (!std::isfinite(
            result.net_pnl))
    {
        throw std::runtime_error(
            "Invalid step net P&L.");
    }


    if (!std::isfinite(
            result.equity_before) ||
        result.equity_before <= 0.0)
    {
        throw std::runtime_error(
            "Invalid equity-before value.");
    }


    result.net_equity_return =
        result.net_pnl /
        result.equity_before;


    if (!std::isfinite(
            result.net_equity_return))
    {
        throw std::runtime_error(
            "Invalid net equity return.");
    }


    // ========================================================================
    // 8. Drawdown
    // ========================================================================

    updateDrawdown();


    result.current_drawdown =
        state_.drawdown;


    // ========================================================================
    // 9. Reward
    //
    // Reward receives net account return AFTER:
    //
    // - market P&L
    // - entry/exit slippage
    // - brokerage/statutory charges
    //
    // RewardEngine does NOT calculate execution costs itself.
    // ========================================================================

    RewardInput reward_input;

    reward_input.net_equity_return =
        result.net_equity_return;

    reward_input.turnover =
        result.turnover;

    reward_input.previous_drawdown =
        result.previous_drawdown;

    reward_input.current_drawdown =
        result.current_drawdown;


    result.reward =
        reward_engine_.calculate(
            reward_input);


    // ========================================================================
    // 10. Advance causal clock
    // ========================================================================

    state_.decision_time =
        decision_time;


    state_.previous_market_price =
        current_market_price;


    // ========================================================================
    // 11. Portfolio observation state
    // ========================================================================

    result.portfolio =
        portfolioState();


    return result;
}


// ============================================================================
// State
// ============================================================================

const TradingEnvironmentState&
TradingEnvironment::state() const noexcept
{
    return state_;
}


// ============================================================================
// Configuration
// ============================================================================

const TradingEnvironmentConfig&
TradingEnvironment::config() const noexcept
{
    return config_;
}


// ============================================================================
// Portfolio state for Dev observation
// ============================================================================

DevPortfolioState
TradingEnvironment::portfolioState() const
{
    DevPortfolioState portfolio;


    if (!state_.initialized)
    {
        return portfolio;
    }


    portfolio.position =
        state_.position;


    portfolio.unrealized_return =
        state_.unrealized_gross_pnl /
        state_.initial_equity;


    // ------------------------------------------------------------------------
    // Account-level realized return representation.
    //
    // realized_gross_pnl already uses actual execution prices and therefore
    // includes execution-price slippage for CLOSED trades.
    //
    // Brokerage/statutory charges are deducted separately.
    // ------------------------------------------------------------------------

    portfolio.realized_return =
        (
            state_.realized_gross_pnl -
            state_.cumulative_brokerage_charges
        ) /
        state_.initial_equity;


    portfolio.equity_return =
        state_.net_pnl /
        state_.initial_equity;


    portfolio.drawdown =
        state_.drawdown;


    portfolio.holding_period =
        static_cast<double>(
            state_.holding_bars);


    return portfolio;
}


} // namespace devai::strategy