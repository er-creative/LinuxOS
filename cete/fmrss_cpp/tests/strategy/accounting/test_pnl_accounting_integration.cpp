#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "devai/strategy/accounting/PnLAccountingIntegration.hpp"

namespace
{

using namespace devai::strategy::accounting;


// ============================================================================
// Helpers
// ============================================================================

void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}


bool approximatelyEqual(
    double left,
    double right,
    double absolute_tolerance = 1.0e-8,
    double relative_tolerance = 1.0e-10)
{
    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            });

    return
        std::fabs(left - right) <=
        absolute_tolerance +
        relative_tolerance * scale;
}


template <typename Function>
void requireThrows(
    Function&& function,
    const std::string& message)
{
    bool threw = false;

    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw = true;
    }

    require(
        threw,
        message);
}

} // namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout
        << "\n"
        << "==========================================================================\n"
        << "CETE PHASE 2.6 — SHARED P&L INTEGRATION VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        PnLAccountingIntegration integration;


        // ====================================================================
        // 1. FLAT / zero-P&L integration
        // ====================================================================

        PnLIntegrationInput flat_input;

        flat_input.initial_equity =
            100000.0;


        const auto flat =
            integration.integrate(
                flat_input);


        require(
            approximatelyEqual(
                flat.accounting.gross_pnl,
                0.0) &&
            approximatelyEqual(
                flat.accounting.execution_costs,
                0.0) &&
            approximatelyEqual(
                flat.accounting.net_pnl,
                0.0) &&
            approximatelyEqual(
                flat.accounting.equity,
                100000.0),
            "FLAT accounting integration failed.");


        std::cout
            << "[PASS] FLAT zero-P&L integration\n";


        // ====================================================================
        // 2. Realized profitable trade
        // ====================================================================

        PnLIntegrationInput realized_profit_input;

        realized_profit_input.initial_equity =
            100000.0;

        realized_profit_input.realized_gross_pnl =
            1000.0;


        const auto realized_profit =
            integration.integrate(
                realized_profit_input);


        require(
            approximatelyEqual(
                realized_profit.accounting.gross_pnl,
                1000.0) &&
            approximatelyEqual(
                realized_profit.accounting.net_pnl,
                1000.0) &&
            approximatelyEqual(
                realized_profit.accounting.equity,
                101000.0),
            "Realized profitable trade integration failed.");


        std::cout
            << "[PASS] Realized profitable P&L integration\n";


        // ====================================================================
        // 3. Unrealized profitable position
        // ====================================================================

        PnLIntegrationInput unrealized_profit_input;

        unrealized_profit_input.initial_equity =
            100000.0;

        unrealized_profit_input.unrealized_gross_pnl =
            750.0;


        const auto unrealized_profit =
            integration.integrate(
                unrealized_profit_input);


        require(
            approximatelyEqual(
                unrealized_profit.accounting.gross_pnl,
                750.0) &&
            approximatelyEqual(
                unrealized_profit.accounting.net_pnl,
                750.0),
            "Unrealized profitable position integration failed.");


        std::cout
            << "[PASS] Unrealized profitable P&L integration\n";


        // ====================================================================
        // 4. Realized + unrealized reconciliation
        // ====================================================================

        PnLIntegrationInput mixed_input;

        mixed_input.initial_equity =
            100000.0;

        mixed_input.realized_gross_pnl =
            1200.0;

        mixed_input.unrealized_gross_pnl =
            -200.0;


        const auto mixed =
            integration.integrate(
                mixed_input);


        require(
            approximatelyEqual(
                mixed.accounting.gross_pnl,
                1000.0),
            "Mixed gross-P&L reconciliation failed.");


        std::cout
            << "[PASS] Realized / unrealized P&L reconciliation\n";


        // ====================================================================
        // 5. Transaction-cost integration
        // ====================================================================

        PnLIntegrationInput transaction_input;

        transaction_input.initial_equity =
            100000.0;

        transaction_input.realized_gross_pnl =
            1000.0;

        transaction_input.transaction_costs =
            100.0;


        const auto transaction =
            integration.integrate(
                transaction_input);


        require(
            approximatelyEqual(
                transaction.accounting.execution_costs,
                100.0) &&
            approximatelyEqual(
                transaction.accounting.net_pnl,
                900.0),
            "Transaction-cost integration failed.");


        std::cout
            << "[PASS] Transaction-cost integration\n";


        // ====================================================================
        // 6. Slippage-cost integration
        // ====================================================================

        PnLIntegrationInput slippage_input;

        slippage_input.initial_equity =
            100000.0;

        slippage_input.realized_gross_pnl =
            1000.0;

        slippage_input.slippage_cost =
            50.0;


        const auto slippage =
            integration.integrate(
                slippage_input);


        require(
            approximatelyEqual(
                slippage.accounting.execution_costs,
                50.0) &&
            approximatelyEqual(
                slippage.accounting.net_pnl,
                950.0),
            "Slippage-cost integration failed.");


        std::cout
            << "[PASS] Slippage-cost integration\n";


        // ====================================================================
        // 7. Combined execution costs
        // ====================================================================

        PnLIntegrationInput combined_input;

        combined_input.initial_equity =
            100000.0;

        combined_input.realized_gross_pnl =
            1200.0;

        combined_input.unrealized_gross_pnl =
            300.0;

        combined_input.transaction_costs =
            100.0;

        combined_input.slippage_cost =
            50.0;


        const auto combined =
            integration.integrate(
                combined_input);


        require(
            approximatelyEqual(
                combined.accounting.gross_pnl,
                1500.0),
            "Combined gross P&L mismatch.");


        require(
            approximatelyEqual(
                combined.accounting.execution_costs,
                150.0),
            "Combined execution costs mismatch.");


        require(
            approximatelyEqual(
                combined.accounting.net_pnl,
                1350.0),
            "Combined net P&L mismatch.");


        require(
            approximatelyEqual(
                combined.accounting.equity,
                101350.0),
            "Combined equity mismatch.");


        std::cout
            << "[PASS] Combined transaction / slippage integration\n";


        // ====================================================================
        // 8. Profitable-account reconciliation
        // ====================================================================

        PnLReconciliationInput profitable_reconciliation;

        profitable_reconciliation.integration =
            combined_input;

        profitable_reconciliation.expected_net_pnl =
            1350.0;

        profitable_reconciliation.expected_equity =
            101350.0;


        const auto profitable =
            integration.reconcile(
                profitable_reconciliation);


        require(
            profitable.reconciled &&
            profitable.net_pnl_matches &&
            profitable.equity_matches,
            "Profitable account reconciliation failed.");


        std::cout
            << "[PASS] Profitable account reconciliation\n";


        // ====================================================================
        // 9. Losing-account reconciliation
        // ====================================================================

        PnLIntegrationInput losing_input;

        losing_input.initial_equity =
            100000.0;

        losing_input.realized_gross_pnl =
            -1500.0;

        losing_input.unrealized_gross_pnl =
            -500.0;

        losing_input.transaction_costs =
            100.0;

        losing_input.slippage_cost =
            50.0;


        PnLReconciliationInput losing_reconciliation;

        losing_reconciliation.integration =
            losing_input;

        losing_reconciliation.expected_net_pnl =
            -2150.0;

        losing_reconciliation.expected_equity =
            97850.0;


        const auto losing =
            integration.reconcile(
                losing_reconciliation);


        require(
            losing.reconciled,
            "Losing-account reconciliation failed.");


        std::cout
            << "[PASS] Losing account reconciliation\n";


        // ====================================================================
        // 10. Gross-return / net-return propagation
        // ====================================================================

        require(
            approximatelyEqual(
                combined.accounting.gross_return,
                0.015),
            "Gross-return propagation failed.");


        require(
            approximatelyEqual(
                combined.accounting.net_return,
                0.0135),
            "Net-return propagation failed.");


        std::cout
            << "[PASS] Gross / net return propagation\n";


        // ====================================================================
        // 11. Integration integrity flags
        // ====================================================================

        require(
            combined.gross_pnl_reconciled &&
            combined.execution_costs_reconciled &&
            combined.net_pnl_reconciled &&
            combined.equity_reconciled &&
            combined.numerical_integrity,
            "Integration integrity flags failed.");


        std::cout
            << "[PASS] Integration reconciliation integrity\n";


        // ====================================================================
        // 12. Negative transaction-cost rejection
        // ====================================================================

        requireThrows(
            [&integration]()
            {
                PnLIntegrationInput input;

                input.initial_equity =
                    100000.0;

                input.transaction_costs =
                    -1.0;

                (void)integration.integrate(
                    input);
            },
            "Negative transaction cost was not rejected.");


        std::cout
            << "[PASS] Invalid transaction-cost rejection\n";


        // ====================================================================
        // 13. Negative slippage rejection
        // ====================================================================

        requireThrows(
            [&integration]()
            {
                PnLIntegrationInput input;

                input.initial_equity =
                    100000.0;

                input.slippage_cost =
                    -1.0;

                (void)integration.integrate(
                    input);
            },
            "Negative slippage cost was not rejected.");


        std::cout
            << "[PASS] Invalid slippage-cost rejection\n";


        // ====================================================================
        // 14. Non-finite P&L rejection
        // ====================================================================

        requireThrows(
            [&integration]()
            {
                PnLIntegrationInput input;

                input.initial_equity =
                    100000.0;

                input.realized_gross_pnl =
                    std::numeric_limits<double>::quiet_NaN();

                (void)integration.integrate(
                    input);
            },
            "Non-finite P&L was not rejected.");


        std::cout
            << "[PASS] Non-finite P&L rejection\n";


        // ====================================================================
        // 15. Reconciliation mismatch detection
        // ====================================================================

        requireThrows(
            [&integration, &combined_input]()
            {
                PnLReconciliationInput input;

                input.integration =
                    combined_input;

                input.expected_net_pnl =
                    999999.0;

                input.expected_equity =
                    100000.0;

                (void)integration.reconcile(
                    input);
            },
            "External accounting mismatch was not detected.");


        std::cout
            << "[PASS] External reconciliation mismatch detection\n";


        // ====================================================================
        // 16. Determinism
        // ====================================================================

        const auto deterministic_a =
            integration.integrate(
                combined_input);

        const auto deterministic_b =
            integration.integrate(
                combined_input);


        require(
            approximatelyEqual(
                deterministic_a.accounting.gross_pnl,
                deterministic_b.accounting.gross_pnl) &&
            approximatelyEqual(
                deterministic_a.accounting.execution_costs,
                deterministic_b.accounting.execution_costs) &&
            approximatelyEqual(
                deterministic_a.accounting.net_pnl,
                deterministic_b.accounting.net_pnl) &&
            approximatelyEqual(
                deterministic_a.accounting.equity,
                deterministic_b.accounting.equity),
            "Shared P&L integration is not deterministic.");


        std::cout
            << "[PASS] Deterministic P&L integration\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Integration ownership      : CETE SHARED BASE\n"
            << "Accounting engine          : PnLAccountingEngine\n"
            << "Strategy dependency        : NONE\n"
            << "Gross P&L integration      : VERIFIED\n"
            << "Realized P&L integration   : VERIFIED\n"
            << "Unrealized P&L integration : VERIFIED\n"
            << "Transaction costs          : VERIFIED\n"
            << "Slippage costs             : VERIFIED\n"
            << "Execution-cost aggregation : VERIFIED\n"
            << "Net P&L                    : VERIFIED\n"
            << "Equity                     : VERIFIED\n"
            << "Gross / net returns        : VERIFIED\n"
            << "External reconciliation    : VERIFIED\n"
            << "Mismatch detection         : VERIFIED\n"
            << "Numerical integrity        : VERIFIED\n"
            << "Deterministic integration  : VERIFIED\n"
            << "Brokerage recalculation    : NONE\n"
            << "Slippage recalculation     : NONE\n"
            << "DevAI-specific logic       : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "CETE PHASE 2.6 SHARED P&L INTEGRATION VALIDATION PASSED\n"
            << "==========================================================================\n";


        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n[FAIL] "
            << exception.what()
            << "\n";

        return 1;
    }
}