#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "devai/strategy/accounting/PnLAccountingEngine.hpp"

namespace
{

using namespace devai::strategy::accounting;


// ============================================================================
// Test helpers
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
        << "CETE SHARED P&L ACCOUNTING ENGINE VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        PnLAccountingEngine engine;


        // ====================================================================
        // 1. LONG profitable position
        // ====================================================================

        PositionPnLInput long_profit;

        long_profit.position =
            1.0;

        long_profit.quantity =
            100.0;

        long_profit.entry_price =
            100.0;

        long_profit.current_price =
            110.0;


        require(
            approximatelyEqual(
                engine.calculatePositionPnL(
                    long_profit),
                1000.0),
            "LONG profitable P&L calculation failed.");


        std::cout
            << "[PASS] LONG profitable position P&L\n";


        // ====================================================================
        // 2. LONG losing position
        // ====================================================================

        PositionPnLInput long_loss;

        long_loss.position =
            1.0;

        long_loss.quantity =
            100.0;

        long_loss.entry_price =
            100.0;

        long_loss.current_price =
            90.0;


        require(
            approximatelyEqual(
                engine.calculatePositionPnL(
                    long_loss),
                -1000.0),
            "LONG losing P&L calculation failed.");


        std::cout
            << "[PASS] LONG losing position P&L\n";


        // ====================================================================
        // 3. SHORT profitable position
        // ====================================================================

        PositionPnLInput short_profit;

        short_profit.position =
            -1.0;

        short_profit.quantity =
            100.0;

        short_profit.entry_price =
            100.0;

        short_profit.current_price =
            90.0;


        require(
            approximatelyEqual(
                engine.calculatePositionPnL(
                    short_profit),
                1000.0),
            "SHORT profitable P&L calculation failed.");


        std::cout
            << "[PASS] SHORT profitable position P&L\n";


        // ====================================================================
        // 4. SHORT losing position
        // ====================================================================

        PositionPnLInput short_loss;

        short_loss.position =
            -1.0;

        short_loss.quantity =
            100.0;

        short_loss.entry_price =
            100.0;

        short_loss.current_price =
            110.0;


        require(
            approximatelyEqual(
                engine.calculatePositionPnL(
                    short_loss),
                -1000.0),
            "SHORT losing P&L calculation failed.");


        std::cout
            << "[PASS] SHORT losing position P&L\n";


        // ====================================================================
        // 5. FLAT position
        // ====================================================================

        PositionPnLInput flat;

        flat.position =
            0.0;

        flat.quantity =
            0.0;


        require(
            approximatelyEqual(
                engine.calculatePositionPnL(
                    flat),
                0.0),
            "FLAT position must have zero P&L.");


        std::cout
            << "[PASS] FLAT zero P&L\n";


        // ====================================================================
        // 6. Gross P&L reconciliation
        // ====================================================================

        PnLAccountingInput gross_input;

        gross_input.initial_equity =
            100000.0;

        gross_input.realized_gross_pnl =
            1500.0;

        gross_input.unrealized_gross_pnl =
            500.0;


        const auto gross_result =
            engine.calculate(
                gross_input);


        require(
            approximatelyEqual(
                gross_result.gross_pnl,
                2000.0),
            "Gross P&L reconciliation failed.");


        std::cout
            << "[PASS] Realized / unrealized gross-P&L reconciliation\n";


        // ====================================================================
        // 7. Transaction-cost integration
        // ====================================================================

        PnLAccountingInput transaction_input;

        transaction_input.initial_equity =
            100000.0;

        transaction_input.realized_gross_pnl =
            1000.0;

        transaction_input.transaction_costs =
            100.0;


        const auto transaction_result =
            engine.calculate(
                transaction_input);


        require(
            approximatelyEqual(
                transaction_result.net_pnl,
                900.0),
            "Transaction-cost integration failed.");


        std::cout
            << "[PASS] Transaction-cost integration\n";


        // ====================================================================
        // 8. Slippage integration
        // ====================================================================

        PnLAccountingInput slippage_input;

        slippage_input.initial_equity =
            100000.0;

        slippage_input.realized_gross_pnl =
            1000.0;

        slippage_input.slippage_cost =
            50.0;


        const auto slippage_result =
            engine.calculate(
                slippage_input);


        require(
            approximatelyEqual(
                slippage_result.net_pnl,
                950.0),
            "Slippage integration failed.");


        std::cout
            << "[PASS] Slippage-cost integration\n";


        // ====================================================================
        // 9. Combined execution-cost accounting
        // ====================================================================

        PnLAccountingInput combined_input;

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


        const auto combined_result =
            engine.calculate(
                combined_input);


        require(
            approximatelyEqual(
                combined_result.gross_pnl,
                1500.0),
            "Combined gross P&L mismatch.");


        require(
            approximatelyEqual(
                combined_result.execution_costs,
                150.0),
            "Combined execution-cost mismatch.");


        require(
            approximatelyEqual(
                combined_result.net_pnl,
                1350.0),
            "Combined net P&L mismatch.");


        require(
            approximatelyEqual(
                combined_result.equity,
                101350.0),
            "Combined equity mismatch.");


        std::cout
            << "[PASS] Combined execution-cost accounting\n";


        // ====================================================================
        // 10. Return calculations
        // ====================================================================

        require(
            approximatelyEqual(
                combined_result.gross_return,
                0.015),
            "Gross-return calculation failed.");


        require(
            approximatelyEqual(
                combined_result.net_return,
                0.0135),
            "Net-return calculation failed.");


        std::cout
            << "[PASS] Gross / net return calculation\n";


        // ====================================================================
        // 11. Losing account calculation
        // ====================================================================

        PnLAccountingInput loss_input;

        loss_input.initial_equity =
            100000.0;

        loss_input.realized_gross_pnl =
            -2000.0;

        loss_input.unrealized_gross_pnl =
            -500.0;

        loss_input.transaction_costs =
            100.0;

        loss_input.slippage_cost =
            50.0;


        const auto loss_result =
            engine.calculate(
                loss_input);


        require(
            approximatelyEqual(
                loss_result.gross_pnl,
                -2500.0),
            "Losing gross P&L mismatch.");


        require(
            approximatelyEqual(
                loss_result.net_pnl,
                -2650.0),
            "Losing net P&L mismatch.");


        require(
            approximatelyEqual(
                loss_result.equity,
                97350.0),
            "Losing equity mismatch.");


        std::cout
            << "[PASS] Losing-account P&L accounting\n";


        // ====================================================================
        // 12. Numerical/accounting integrity
        // ====================================================================

        require(
            combined_result.numerical_integrity,
            "Numerical integrity flag is false.");


        engine.validate(
            combined_result);


        std::cout
            << "[PASS] Accounting reconciliation integrity\n";


        // ====================================================================
        // 13. Invalid position rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                PositionPnLInput input;

                input.position =
                    0.5;

                input.quantity =
                    100.0;

                input.entry_price =
                    100.0;

                input.current_price =
                    101.0;

                (void)engine.calculatePositionPnL(
                    input);
            },
            "Invalid position was not rejected.");


        std::cout
            << "[PASS] Invalid-position rejection\n";


        // ====================================================================
        // 14. Invalid quantity rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                PositionPnLInput input;

                input.position =
                    1.0;

                input.quantity =
                    -1.0;

                input.entry_price =
                    100.0;

                input.current_price =
                    101.0;

                (void)engine.calculatePositionPnL(
                    input);
            },
            "Negative quantity was not rejected.");


        std::cout
            << "[PASS] Invalid-quantity rejection\n";


        // ====================================================================
        // 15. Invalid cost rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                PnLAccountingInput input;

                input.initial_equity =
                    100000.0;

                input.transaction_costs =
                    -1.0;

                (void)engine.calculate(
                    input);
            },
            "Negative transaction cost was not rejected.");


        std::cout
            << "[PASS] Invalid-cost rejection\n";


        // ====================================================================
        // 16. Non-finite input rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                PnLAccountingInput input;

                input.initial_equity =
                    100000.0;

                input.realized_gross_pnl =
                    std::numeric_limits<double>::quiet_NaN();

                (void)engine.calculate(
                    input);
            },
            "NaN accounting input was not rejected.");


        std::cout
            << "[PASS] Non-finite input rejection\n";


        // ====================================================================
        // 17. Non-positive equity protection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                PnLAccountingInput input;

                input.initial_equity =
                    1000.0;

                input.realized_gross_pnl =
                    -1000.0;

                (void)engine.calculate(
                    input);
            },
            "Non-positive equity was not rejected.");


        std::cout
            << "[PASS] Non-positive equity protection\n";


        // ====================================================================
        // 18. Determinism
        // ====================================================================

        const auto deterministic_a =
            engine.calculate(
                combined_input);

        const auto deterministic_b =
            engine.calculate(
                combined_input);


        require(
            approximatelyEqual(
                deterministic_a.gross_pnl,
                deterministic_b.gross_pnl) &&
            approximatelyEqual(
                deterministic_a.execution_costs,
                deterministic_b.execution_costs) &&
            approximatelyEqual(
                deterministic_a.net_pnl,
                deterministic_b.net_pnl) &&
            approximatelyEqual(
                deterministic_a.equity,
                deterministic_b.equity) &&
            approximatelyEqual(
                deterministic_a.net_return,
                deterministic_b.net_return),
            "Accounting calculation is not deterministic.");


        std::cout
            << "[PASS] Deterministic accounting\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Accounting ownership       : CETE SHARED BASE\n"
            << "Strategy dependency        : NONE\n"
            << "LONG P&L                   : VERIFIED\n"
            << "SHORT P&L                  : VERIFIED\n"
            << "FLAT P&L                   : VERIFIED\n"
            << "Realized gross P&L         : VERIFIED\n"
            << "Unrealized gross P&L       : VERIFIED\n"
            << "Gross P&L reconciliation   : VERIFIED\n"
            << "Transaction costs          : VERIFIED\n"
            << "Slippage costs             : VERIFIED\n"
            << "Execution costs            : VERIFIED\n"
            << "Net P&L                    : VERIFIED\n"
            << "Equity                     : VERIFIED\n"
            << "Gross / net returns        : VERIFIED\n"
            << "Numerical integrity        : VERIFIED\n"
            << "Invalid-input protection   : VERIFIED\n"
            << "Deterministic calculation  : VERIFIED\n"
            << "DevAI-specific logic       : NONE\n"
            << "Duplicate accounting logic : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "CETE SHARED P&L ACCOUNTING ENGINE VALIDATION PASSED\n"
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