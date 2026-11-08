#include "devai/strategy/accounting/PnLAccountingEngine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy::accounting
{

namespace
{

constexpr double ABSOLUTE_TOLERANCE =
    1.0e-9;

constexpr double RELATIVE_TOLERANCE =
    1.0e-10;

} // namespace


// ============================================================================
// Numeric helpers
// ============================================================================

bool
PnLAccountingEngine::finite(
    double value) noexcept
{
    return std::isfinite(value);
}


bool
PnLAccountingEngine::finiteNonNegative(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value >= 0.0;
}


bool
PnLAccountingEngine::approximatelyEqual(
    double left,
    double right) noexcept
{
    if (!finite(left) ||
        !finite(right))
    {
        return false;
    }

    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            });

    return
        std::fabs(left - right) <=
        ABSOLUTE_TOLERANCE +
        RELATIVE_TOLERANCE * scale;
}


// ============================================================================
// Position validation
// ============================================================================

void
PnLAccountingEngine::validatePosition(
    double position)
{
    if (!finite(position))
    {
        throw std::invalid_argument(
            "PnLAccountingEngine position must be finite.");
    }

    if (!approximatelyEqual(position, -1.0) &&
        !approximatelyEqual(position,  0.0) &&
        !approximatelyEqual(position,  1.0))
    {
        throw std::invalid_argument(
            "PnLAccountingEngine position must be "
            "-1, 0, or +1.");
    }
}


// ============================================================================
// Generic position mark-to-market P&L
//
// Formula:
//
// position × quantity × (current_price - entry_price)
//
// LONG:
//
// +1 × Q × (Current - Entry)
//
// SHORT:
//
// -1 × Q × (Current - Entry)
//   = Q × (Entry - Current)
// ============================================================================

double
PnLAccountingEngine::calculatePositionPnL(
    const PositionPnLInput& input) const
{
    validatePosition(
        input.position);


    if (!finiteNonNegative(
            input.quantity))
    {
        throw std::invalid_argument(
            "PnLAccountingEngine quantity must be "
            "finite and non-negative.");
    }


    // ------------------------------------------------------------------------
    // FLAT position
    //
    // A flat position must not carry quantity.
    // Prices are irrelevant to P&L once there is no position.
    // ------------------------------------------------------------------------

    if (approximatelyEqual(
            input.position,
            0.0))
    {
        if (!approximatelyEqual(
                input.quantity,
                0.0))
        {
            throw std::invalid_argument(
                "FLAT position cannot have non-zero quantity.");
        }

        return 0.0;
    }


    // ------------------------------------------------------------------------
    // Open position
    // ------------------------------------------------------------------------

    if (input.quantity <= 0.0)
    {
        throw std::invalid_argument(
            "Open position requires positive quantity.");
    }


    if (!finite(input.entry_price) ||
        input.entry_price <= 0.0)
    {
        throw std::invalid_argument(
            "Open position requires positive finite "
            "entry price.");
    }


    if (!finite(input.current_price) ||
        input.current_price <= 0.0)
    {
        throw std::invalid_argument(
            "Open position requires positive finite "
            "current price.");
    }


    const double pnl =
        input.position *
        input.quantity *
        (
            input.current_price -
            input.entry_price
        );


    if (!finite(pnl))
    {
        throw std::runtime_error(
            "PnLAccountingEngine produced non-finite "
            "position P&L.");
    }


    return pnl;
}


// ============================================================================
// Complete account calculation
// ============================================================================

PnLAccountingResult
PnLAccountingEngine::calculate(
    const PnLAccountingInput& input) const
{
    // ------------------------------------------------------------------------
    // Validate inputs
    // ------------------------------------------------------------------------

    if (!finite(input.initial_equity) ||
        input.initial_equity <= 0.0)
    {
        throw std::invalid_argument(
            "PnLAccountingEngine initial equity must be "
            "positive and finite.");
    }


    if (!finite(
            input.realized_gross_pnl))
    {
        throw std::invalid_argument(
            "Realized gross P&L must be finite.");
    }


    if (!finite(
            input.unrealized_gross_pnl))
    {
        throw std::invalid_argument(
            "Unrealized gross P&L must be finite.");
    }


    if (!finiteNonNegative(
            input.transaction_costs))
    {
        throw std::invalid_argument(
            "Transaction costs must be finite and "
            "non-negative.");
    }


    if (!finiteNonNegative(
            input.slippage_cost))
    {
        throw std::invalid_argument(
            "Slippage cost must be finite and "
            "non-negative.");
    }


    // ------------------------------------------------------------------------
    // Calculate
    // ------------------------------------------------------------------------

    PnLAccountingResult result;

    result.initial_equity =
        input.initial_equity;

    result.realized_gross_pnl =
        input.realized_gross_pnl;

    result.unrealized_gross_pnl =
        input.unrealized_gross_pnl;


    result.gross_pnl =
        result.realized_gross_pnl +
        result.unrealized_gross_pnl;


    result.transaction_costs =
        input.transaction_costs;

    result.slippage_cost =
        input.slippage_cost;


    result.execution_costs =
        result.transaction_costs +
        result.slippage_cost;


    result.net_pnl =
        result.gross_pnl -
        result.execution_costs;


    result.equity =
        result.initial_equity +
        result.net_pnl;


    result.gross_return =
        result.gross_pnl /
        result.initial_equity;


    result.net_return =
        result.net_pnl /
        result.initial_equity;


    // ------------------------------------------------------------------------
    // Numerical integrity
    // ------------------------------------------------------------------------

    if (!finite(result.gross_pnl) ||
        !finite(result.execution_costs) ||
        !finite(result.net_pnl) ||
        !finite(result.equity) ||
        !finite(result.gross_return) ||
        !finite(result.net_return))
    {
        throw std::runtime_error(
            "PnLAccountingEngine produced non-finite "
            "accounting output.");
    }


    // CETE does not permit a non-positive account state.
    if (result.equity <= 0.0)
    {
        throw std::runtime_error(
            "PnLAccountingEngine equity became "
            "non-positive.");
    }


    result.numerical_integrity =
        true;


    validate(result);


    return result;
}


// ============================================================================
// Accounting reconciliation
// ============================================================================

void
PnLAccountingEngine::validate(
    const PnLAccountingResult& result) const
{
    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "PnLAccountingResult numerical integrity "
            "is false.");
    }


    if (!finite(result.initial_equity) ||
        result.initial_equity <= 0.0)
    {
        throw std::runtime_error(
            "Invalid accounting initial equity.");
    }


    if (!finite(result.realized_gross_pnl) ||
        !finite(result.unrealized_gross_pnl) ||
        !finite(result.gross_pnl) ||
        !finiteNonNegative(
            result.transaction_costs) ||
        !finiteNonNegative(
            result.slippage_cost) ||
        !finiteNonNegative(
            result.execution_costs) ||
        !finite(result.net_pnl) ||
        !finite(result.equity) ||
        !finite(result.gross_return) ||
        !finite(result.net_return))
    {
        throw std::runtime_error(
            "PnLAccountingResult contains invalid "
            "numerical values.");
    }


    const double expected_gross_pnl =
        result.realized_gross_pnl +
        result.unrealized_gross_pnl;


    if (!approximatelyEqual(
            result.gross_pnl,
            expected_gross_pnl))
    {
        throw std::runtime_error(
            "Gross P&L reconciliation failed.");
    }


    const double expected_execution_costs =
        result.transaction_costs +
        result.slippage_cost;


    if (!approximatelyEqual(
            result.execution_costs,
            expected_execution_costs))
    {
        throw std::runtime_error(
            "Execution-cost reconciliation failed.");
    }


    const double expected_net_pnl =
        result.gross_pnl -
        result.execution_costs;


    if (!approximatelyEqual(
            result.net_pnl,
            expected_net_pnl))
    {
        throw std::runtime_error(
            "Net P&L reconciliation failed.");
    }


    const double expected_equity =
        result.initial_equity +
        result.net_pnl;


    if (!approximatelyEqual(
            result.equity,
            expected_equity))
    {
        throw std::runtime_error(
            "Equity reconciliation failed.");
    }


    const double expected_gross_return =
        result.gross_pnl /
        result.initial_equity;


    if (!approximatelyEqual(
            result.gross_return,
            expected_gross_return))
    {
        throw std::runtime_error(
            "Gross-return reconciliation failed.");
    }


    const double expected_net_return =
        result.net_pnl /
        result.initial_equity;


    if (!approximatelyEqual(
            result.net_return,
            expected_net_return))
    {
        throw std::runtime_error(
            "Net-return reconciliation failed.");
    }


    if (result.equity <= 0.0)
    {
        throw std::runtime_error(
            "Accounting equity must remain positive.");
    }
}

} // namespace devai::strategy::accounting