#include "devai/strategy/accounting/PnLAccountingIntegration.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy::accounting
{

namespace
{

constexpr double ABSOLUTE_TOLERANCE =
    1.0e-8;

constexpr double RELATIVE_TOLERANCE =
    1.0e-10;

} // namespace


// ============================================================================
// Numeric helpers
// ============================================================================

bool
PnLAccountingIntegration::finite(
    double value) noexcept
{
    return std::isfinite(value);
}


bool
PnLAccountingIntegration::finiteNonNegative(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value >= 0.0;
}


bool
PnLAccountingIntegration::approximatelyEqual(
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
// integrate
// ============================================================================

PnLIntegrationResult
PnLAccountingIntegration::integrate(
    const PnLIntegrationInput& input) const
{
    // ------------------------------------------------------------------------
    // Input validation
    // ------------------------------------------------------------------------

    if (!finite(input.initial_equity) ||
        input.initial_equity <= 0.0)
    {
        throw std::invalid_argument(
            "PnLAccountingIntegration initial equity "
            "must be positive and finite.");
    }


    if (!finite(
            input.realized_gross_pnl))
    {
        throw std::invalid_argument(
            "PnLAccountingIntegration realized gross "
            "P&L must be finite.");
    }


    if (!finite(
            input.unrealized_gross_pnl))
    {
        throw std::invalid_argument(
            "PnLAccountingIntegration unrealized gross "
            "P&L must be finite.");
    }


    if (!finiteNonNegative(
            input.transaction_costs))
    {
        throw std::invalid_argument(
            "PnLAccountingIntegration transaction costs "
            "must be finite and non-negative.");
    }


    if (!finiteNonNegative(
            input.slippage_cost))
    {
        throw std::invalid_argument(
            "PnLAccountingIntegration slippage cost "
            "must be finite and non-negative.");
    }


    // ------------------------------------------------------------------------
    // Construct the shared accounting-engine input.
    // ------------------------------------------------------------------------

    PnLAccountingInput accounting_input;

    accounting_input.initial_equity =
        input.initial_equity;

    accounting_input.realized_gross_pnl =
        input.realized_gross_pnl;

    accounting_input.unrealized_gross_pnl =
        input.unrealized_gross_pnl;

    accounting_input.transaction_costs =
        input.transaction_costs;

    accounting_input.slippage_cost =
        input.slippage_cost;


    // ------------------------------------------------------------------------
    // Delegate all economic accounting to the shared base.
    // ------------------------------------------------------------------------

    PnLIntegrationResult result;

    result.accounting =
        accounting_engine_.calculate(
            accounting_input);


    // ------------------------------------------------------------------------
    // Independent reconciliation values.
    // ------------------------------------------------------------------------

    result.supplied_gross_pnl =
        input.realized_gross_pnl +
        input.unrealized_gross_pnl;


    result.supplied_execution_costs =
        input.transaction_costs +
        input.slippage_cost;


    result.gross_pnl_reconciled =
        approximatelyEqual(
            result.accounting.gross_pnl,
            result.supplied_gross_pnl);


    result.execution_costs_reconciled =
        approximatelyEqual(
            result.accounting.execution_costs,
            result.supplied_execution_costs);


    const double expected_net_pnl =
        result.supplied_gross_pnl -
        result.supplied_execution_costs;


    result.net_pnl_reconciled =
        approximatelyEqual(
            result.accounting.net_pnl,
            expected_net_pnl);


    const double expected_equity =
        input.initial_equity +
        expected_net_pnl;


    result.equity_reconciled =
        approximatelyEqual(
            result.accounting.equity,
            expected_equity);


    result.numerical_integrity =
        result.accounting.numerical_integrity &&
        result.gross_pnl_reconciled &&
        result.execution_costs_reconciled &&
        result.net_pnl_reconciled &&
        result.equity_reconciled;


    validate(result);


    return result;
}


// ============================================================================
// reconcile
//
// Compare shared CETE accounting with an external account state.
//
// This is especially useful during migration from the already-validated
// TradingEnvironment accounting path to the shared CETE accounting base.
// ============================================================================

PnLReconciliationResult
PnLAccountingIntegration::reconcile(
    const PnLReconciliationInput& input) const
{
    if (!finite(
            input.expected_net_pnl))
    {
        throw std::invalid_argument(
            "Expected net P&L must be finite.");
    }


    if (!finite(
            input.expected_equity) ||
        input.expected_equity <= 0.0)
    {
        throw std::invalid_argument(
            "Expected equity must be positive and finite.");
    }


    PnLReconciliationResult result;

    result.integration =
        integrate(
            input.integration);


    result.expected_net_pnl =
        input.expected_net_pnl;

    result.expected_equity =
        input.expected_equity;


    result.net_pnl_matches =
        approximatelyEqual(
            result.integration.accounting.net_pnl,
            input.expected_net_pnl);


    result.equity_matches =
        approximatelyEqual(
            result.integration.accounting.equity,
            input.expected_equity);


    result.reconciled =
        result.integration.numerical_integrity &&
        result.net_pnl_matches &&
        result.equity_matches;


    validate(result);


    return result;
}


// ============================================================================
// validate integration
// ============================================================================

void
PnLAccountingIntegration::validate(
    const PnLIntegrationResult& result) const
{
    accounting_engine_.validate(
        result.accounting);


    if (!finite(
            result.supplied_gross_pnl) ||
        !finiteNonNegative(
            result.supplied_execution_costs))
    {
        throw std::runtime_error(
            "PnL integration contains invalid "
            "reconciliation values.");
    }


    if (!result.gross_pnl_reconciled)
    {
        throw std::runtime_error(
            "P&L integration gross-P&L "
            "reconciliation failed.");
    }


    if (!result.execution_costs_reconciled)
    {
        throw std::runtime_error(
            "P&L integration execution-cost "
            "reconciliation failed.");
    }


    if (!result.net_pnl_reconciled)
    {
        throw std::runtime_error(
            "P&L integration net-P&L "
            "reconciliation failed.");
    }


    if (!result.equity_reconciled)
    {
        throw std::runtime_error(
            "P&L integration equity "
            "reconciliation failed.");
    }


    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "P&L integration numerical integrity "
            "is false.");
    }
}


// ============================================================================
// validate external reconciliation
// ============================================================================

void
PnLAccountingIntegration::validate(
    const PnLReconciliationResult& result) const
{
    validate(
        result.integration);


    if (!finite(
            result.expected_net_pnl))
    {
        throw std::runtime_error(
            "P&L reconciliation expected net P&L "
            "is invalid.");
    }


    if (!finite(
            result.expected_equity) ||
        result.expected_equity <= 0.0)
    {
        throw std::runtime_error(
            "P&L reconciliation expected equity "
            "is invalid.");
    }


    if (!result.net_pnl_matches)
    {
        throw std::runtime_error(
            "Shared CETE net P&L does not match "
            "external account net P&L.");
    }


    if (!result.equity_matches)
    {
        throw std::runtime_error(
            "Shared CETE equity does not match "
            "external account equity.");
    }


    if (!result.reconciled)
    {
        throw std::runtime_error(
            "Shared CETE P&L reconciliation failed.");
    }
}

} // namespace devai::strategy::accounting