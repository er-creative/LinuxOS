#pragma once

#include "devai/strategy/accounting/PnLAccountingEngine.hpp"

namespace devai::strategy::accounting
{

// ============================================================================
// PnLIntegrationInput
//
// Shared CETE integration contract.
//
// IMPORTANT:
//
// realized_gross_pnl and unrealized_gross_pnl must represent economic
// gross P&L BEFORE transaction costs and BEFORE slippage-cost deduction.
//
// transaction_costs and slippage_cost are positive cost values.
//
// The integration layer does not calculate brokerage or slippage.
// It receives those values from the appropriate CETE components.
// ============================================================================
struct PnLIntegrationInput
{
    double initial_equity{0.0};

    double realized_gross_pnl{0.0};
    double unrealized_gross_pnl{0.0};

    double transaction_costs{0.0};
    double slippage_cost{0.0};
};


// ============================================================================
// PnLIntegrationResult
//
// Result returned by the shared CETE integration layer.
// ============================================================================
struct PnLIntegrationResult
{
    PnLAccountingResult accounting;

    double supplied_gross_pnl{0.0};
    double supplied_execution_costs{0.0};

    bool gross_pnl_reconciled{false};
    bool execution_costs_reconciled{false};
    bool net_pnl_reconciled{false};
    bool equity_reconciled{false};

    bool numerical_integrity{false};
};


// ============================================================================
// PnLReconciliationInput
//
// Optional reconciliation contract.
//
// This allows an execution environment, simulator, backtester, or strategy
// to prove that its externally observed account result agrees with the
// shared CETE accounting result.
//
// It does NOT make the external environment authoritative.
//
// expected_net_pnl:
//     externally observed cumulative net P&L.
//
// expected_equity:
//     externally observed account equity.
// ============================================================================
struct PnLReconciliationInput
{
    PnLIntegrationInput integration;

    double expected_net_pnl{0.0};
    double expected_equity{0.0};
};


// ============================================================================
// PnLReconciliationResult
// ============================================================================
struct PnLReconciliationResult
{
    PnLIntegrationResult integration;

    double expected_net_pnl{0.0};
    double expected_equity{0.0};

    bool net_pnl_matches{false};
    bool equity_matches{false};
    bool reconciled{false};
};


// ============================================================================
// PnLAccountingIntegration
//
// CETE SHARED COMPONENT.
//
// Responsibilities:
//
//   1. Accept strategy-independent P&L inputs.
//   2. Accept already-calculated transaction costs.
//   3. Accept already-calculated slippage costs.
//   4. Delegate economic calculations to PnLAccountingEngine.
//   5. Reconcile gross P&L.
//   6. Reconcile execution costs.
//   7. Reconcile net P&L.
//   8. Reconcile equity.
//   9. Optionally compare against an external execution environment.
//
// It deliberately does NOT:
//
//   - calculate brokerage
//   - calculate STT/GST/etc.
//   - calculate execution prices
//   - calculate slippage
//   - manage positions
//   - process DevAction
//   - calculate rewards
//   - know about DevAI
// ============================================================================
class PnLAccountingIntegration
{
public:
    PnLAccountingIntegration() = default;

    PnLIntegrationResult integrate(
        const PnLIntegrationInput& input) const;

    PnLReconciliationResult reconcile(
        const PnLReconciliationInput& input) const;

    void validate(
        const PnLIntegrationResult& result) const;

    void validate(
        const PnLReconciliationResult& result) const;

private:
    PnLAccountingEngine accounting_engine_;

    static bool finite(
        double value) noexcept;

    static bool finiteNonNegative(
        double value) noexcept;

    static bool approximatelyEqual(
        double left,
        double right) noexcept;
};

} // namespace devai::strategy::accounting