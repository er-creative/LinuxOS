#pragma once

namespace devai::strategy::accounting
{

// ============================================================================
// PnLAccountingInput
//
// Strategy-independent CETE accounting input.
//
// Sign convention:
//
//   Profit = positive
//   Loss   = negative
//
// Transaction costs and slippage are supplied as positive cost values.
// ============================================================================
struct PnLAccountingInput
{
    double initial_equity{0.0};

    double realized_gross_pnl{0.0};
    double unrealized_gross_pnl{0.0};

    double transaction_costs{0.0};
    double slippage_cost{0.0};
};


// ============================================================================
// PnLAccountingResult
//
// Shared CETE accounting result.
//
// gross_pnl
//     = realized_gross_pnl + unrealized_gross_pnl
//
// execution_costs
//     = transaction_costs + slippage_cost
//
// net_pnl
//     = gross_pnl - execution_costs
//
// equity
//     = initial_equity + net_pnl
// ============================================================================
struct PnLAccountingResult
{
    double initial_equity{0.0};

    double realized_gross_pnl{0.0};
    double unrealized_gross_pnl{0.0};
    double gross_pnl{0.0};

    double transaction_costs{0.0};
    double slippage_cost{0.0};
    double execution_costs{0.0};

    double net_pnl{0.0};
    double equity{0.0};

    double gross_return{0.0};
    double net_return{0.0};

    bool numerical_integrity{false};
};


// ============================================================================
// PositionPnLInput
//
// Generic mark-to-market calculation for a single position.
//
// position:
//
//   +1.0 = LONG
//    0.0 = FLAT
//   -1.0 = SHORT
//
// quantity is always non-negative.
// ============================================================================
struct PositionPnLInput
{
    double position{0.0};
    double quantity{0.0};

    double entry_price{0.0};
    double current_price{0.0};
};


// ============================================================================
// PnLAccountingEngine
//
// Shared CETE BASE accounting engine.
//
// Responsibilities:
//
//   - generic LONG/SHORT mark-to-market P&L
//   - realized + unrealized gross-P&L reconciliation
//   - transaction-cost integration
//   - slippage-cost integration
//   - net-P&L calculation
//   - equity calculation
//   - return calculation
//   - numerical/accounting integrity
//
// This engine deliberately knows nothing about:
//
//   - DevAI
//   - Dev Agent
//   - DevAction
//   - observations
//   - rewards
//   - CETE ranking
//   - strategy signals
//
// Strategies consume or extend this shared base.
// ============================================================================
class PnLAccountingEngine
{
public:
    PnLAccountingEngine() = default;

    double calculatePositionPnL(
        const PositionPnLInput& input) const;

    PnLAccountingResult calculate(
        const PnLAccountingInput& input) const;

    void validate(
        const PnLAccountingResult& result) const;

private:
    static bool finite(
        double value) noexcept;

    static bool finiteNonNegative(
        double value) noexcept;

    static bool approximatelyEqual(
        double left,
        double right) noexcept;

    static void validatePosition(
        double position);
};

} // namespace devai::strategy::accounting