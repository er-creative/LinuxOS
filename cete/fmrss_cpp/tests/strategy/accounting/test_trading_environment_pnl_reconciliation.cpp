#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "devai/strategy/accounting/PnLAccountingIntegration.hpp"

#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/SlippageModel.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"
#include "devai/strategy/dev_ai_strategy/TransactionCostModel.hpp"


namespace
{

using namespace devai::strategy;
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
    double absolute_tolerance = 1.0e-7,
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


// ============================================================================
// Cumulative accounting tracker
//
// IMPORTANT:
//
// cumulative_market_gross_pnl
//
// is accumulated from:
//
//     TradingStepResult::gross_pnl
//
// This represents market-movement gross P&L before separately accounting
// for transaction costs and slippage.
//
// We deliberately do NOT feed TradingEnvironmentState::realized_gross_pnl
// directly into shared accounting and then subtract slippage again.
// ============================================================================

struct AccountingTracker
{
    double cumulative_market_gross_pnl{0.0};

    double cumulative_transaction_costs{0.0};

    double cumulative_slippage_cost{0.0};
};


// ============================================================================
// Reconcile one completed TradingEnvironment step against:
//
//   Phase 2.4 TransactionCostModel
//   Phase 2.5 SlippageModel
//   CETE PnLAccountingIntegration
//
// The tracker is cumulative across the episode.
// ============================================================================

PnLReconciliationResult reconcileStep(
    const TradingStepResult& step,
    const TradingEnvironment& environment,
    const TransactionCostModel& transaction_cost_model,
    const SlippageModel& slippage_model,
    const PnLAccountingIntegration& pnl_integration,
    AccountingTracker& tracker,
    double initial_equity)
{
    // ------------------------------------------------------------------------
    // Phase 2.4 — actual transaction costs
    // ------------------------------------------------------------------------

    const auto transaction_summary =
        transaction_cost_model.summarize(
            step);


    transaction_cost_model.validate(
        step);


    // ------------------------------------------------------------------------
    // Phase 2.5 — actual slippage
    // ------------------------------------------------------------------------

    const auto slippage_summary =
        slippage_model.summarize(
            step);


    slippage_model.validate(
        step);


    // ------------------------------------------------------------------------
    // Step-level reconciliation against TradingStepResult
    // ------------------------------------------------------------------------

    require(
        approximatelyEqual(
            transaction_summary.total_charges,
            step.brokerage_charges),
        "Phase 2.4 transaction costs do not match "
        "TradingStepResult brokerage charges.");


    require(
        approximatelyEqual(
            slippage_summary.total_slippage_cost,
            step.slippage_cost),
        "Phase 2.5 slippage does not match "
        "TradingStepResult slippage cost.");


    require(
        approximatelyEqual(
            step.execution_cost,
            step.brokerage_charges +
            step.slippage_cost),
        "TradingStepResult execution-cost identity failed.");


    // ------------------------------------------------------------------------
    // Accumulate independent episode-level economic components.
    //
    // gross_pnl:
    //     market movement earned by the position that existed before the
    //     current action.
    //
    // transaction costs:
    //     actual Phase 2.4 charges.
    //
    // slippage:
    //     actual Phase 2.5 execution-price slippage.
    // ------------------------------------------------------------------------

    tracker.cumulative_market_gross_pnl +=
        step.gross_pnl;


    tracker.cumulative_transaction_costs +=
        transaction_summary.total_charges;


    tracker.cumulative_slippage_cost +=
        slippage_summary.total_slippage_cost;


    // ------------------------------------------------------------------------
    // Feed shared CETE accounting.
    //
    // For this account-level reconciliation test, cumulative market gross P&L
    // is supplied as gross realized accounting input and unrealized is zero.
    //
    // The purpose here is TOTAL economic reconciliation, not classification
    // of realized vs unrealized state.
    //
    // Realized/unrealized classification remains separately validated by the
    // existing TradingEnvironment tests and PnLAccountingEngine unit tests.
    // ------------------------------------------------------------------------

    PnLReconciliationInput reconciliation_input;

    reconciliation_input.integration.initial_equity =
        initial_equity;

    reconciliation_input.integration.realized_gross_pnl =
        tracker.cumulative_market_gross_pnl;

    reconciliation_input.integration.unrealized_gross_pnl =
        0.0;

    reconciliation_input.integration.transaction_costs =
        tracker.cumulative_transaction_costs;

    reconciliation_input.integration.slippage_cost =
        tracker.cumulative_slippage_cost;


    // ------------------------------------------------------------------------
    // TradingEnvironment is the external account state being reconciled.
    // ------------------------------------------------------------------------

    const auto& environment_state =
        environment.state();


    reconciliation_input.expected_net_pnl =
        environment_state.net_pnl;

    reconciliation_input.expected_equity =
        environment_state.equity;


    const auto reconciliation =
        pnl_integration.reconcile(
            reconciliation_input);


    // ------------------------------------------------------------------------
    // Explicit no-double-slippage identity.
    //
    // Environment net P&L must equal:
    //
    // market gross
    // - transaction costs
    // - slippage
    //
    // NOT:
    //
    // market gross
    // - transaction costs
    // - slippage
    // - slippage
    // ------------------------------------------------------------------------

    const double expected_net_pnl =
        tracker.cumulative_market_gross_pnl -
        tracker.cumulative_transaction_costs -
        tracker.cumulative_slippage_cost;


    require(
        approximatelyEqual(
            environment_state.net_pnl,
            expected_net_pnl),
        "TradingEnvironment net P&L violates "
        "gross - transaction costs - slippage identity.");


    const double expected_equity =
        initial_equity +
        expected_net_pnl;


    require(
        approximatelyEqual(
            environment_state.equity,
            expected_equity),
        "TradingEnvironment equity reconciliation failed.");


    require(
        reconciliation.reconciled,
        "Shared CETE accounting failed to reconcile "
        "with TradingEnvironment.");


    return reconciliation;
}


// ============================================================================
// Verify tracker and environment cumulative cost states agree.
// ============================================================================

void verifyCumulativeCosts(
    const TradingEnvironment& environment,
    const AccountingTracker& tracker)
{
    const auto& state =
        environment.state();


    require(
        approximatelyEqual(
            state.cumulative_brokerage_charges,
            tracker.cumulative_transaction_costs),
        "Cumulative transaction-cost reconciliation failed.");


    require(
        approximatelyEqual(
            state.cumulative_slippage_cost,
            tracker.cumulative_slippage_cost),
        "Cumulative slippage reconciliation failed.");


    require(
        approximatelyEqual(
            state.cumulative_execution_cost,
            tracker.cumulative_transaction_costs +
            tracker.cumulative_slippage_cost),
        "Cumulative execution-cost reconciliation failed.");
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
        << "DEVAI PHASE 2.6B — TRADING ENVIRONMENT / CETE P&L RECONCILIATION\n"
        << "==========================================================================\n";


    try
    {
        // ====================================================================
        // Configuration
        // ====================================================================

        constexpr double INITIAL_EQUITY =
            100000.0;


        TradingEnvironmentConfig config;

        config.initial_equity =
            INITIAL_EQUITY;

        config.exposure_fraction =
            1.0;

        config.slippage_rate =
            0.0002;


        TradingEnvironment environment(
            config);


        TransactionCostModel transaction_cost_model;

        SlippageModel slippage_model;

        PnLAccountingIntegration pnl_integration;

        AccountingTracker tracker;


        constexpr std::int64_t T0 =
            1000000;

        constexpr std::int64_t T1 =
            T0 + 60;

        constexpr std::int64_t T2 =
            T1 + 60;

        constexpr std::int64_t T3 =
            T2 + 60;

        constexpr std::int64_t T4 =
            T3 + 60;

        constexpr std::int64_t T5 =
            T4 + 60;

        constexpr std::int64_t T6 =
            T5 + 60;

        constexpr std::int64_t T7 =
            T6 + 60;

        constexpr std::int64_t T8 =
            T7 + 60;

        constexpr std::int64_t T9 =
            T8 + 60;


        // ====================================================================
        // 1. Reset
        // ====================================================================

        environment.reset(
            "RELIANCE",
            T0,
            100.0);


        require(
            approximatelyEqual(
                environment.state().equity,
                INITIAL_EQUITY),
            "Initial environment equity mismatch.");


        require(
            approximatelyEqual(
                environment.state().net_pnl,
                0.0),
            "Initial environment net P&L must be zero.");


        std::cout
            << "[PASS] Initial accounting state\n";


        // ====================================================================
        // 2. FLAT -> LONG
        //
        // No market movement has yet been earned.
        //
        // Equity should decrease only by:
        //
        //   BUY transaction costs
        //   + opening BUY slippage
        // ====================================================================

        const auto long_entry =
            environment.step(
                DevAction::LONG,
                T1,
                100.0);


        const auto long_entry_reconciliation =
            reconcileStep(
                long_entry,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            approximatelyEqual(
                long_entry.gross_pnl,
                0.0),
            "LONG entry must not receive same-step "
            "market-movement P&L.");


        require(
            long_entry.slippage_cost > 0.0,
            "LONG entry must contain BUY slippage.");


        require(
            long_entry.brokerage_charges > 0.0,
            "LONG entry must contain transaction costs.");


        require(
            long_entry_reconciliation.reconciled,
            "LONG entry CETE reconciliation failed.");


        verifyCumulativeCosts(
            environment,
            tracker);


        std::cout
            << "[PASS] FLAT -> LONG accounting reconciliation\n";


        // ====================================================================
        // 3. LONG HOLD — profitable market movement
        //
        // 100 -> 105
        // ====================================================================

        const auto long_profit =
            environment.step(
                DevAction::HOLD,
                T2,
                105.0);


        const auto long_profit_reconciliation =
            reconcileStep(
                long_profit,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            long_profit.gross_pnl > 0.0,
            "LONG position should profit when price rises.");


        require(
            approximatelyEqual(
                long_profit.brokerage_charges,
                0.0) &&
            approximatelyEqual(
                long_profit.slippage_cost,
                0.0),
            "LONG HOLD must not generate execution costs.");


        require(
            long_profit_reconciliation.reconciled,
            "LONG profitable HOLD reconciliation failed.");


        std::cout
            << "[PASS] LONG profitable HOLD reconciliation\n";


        // ====================================================================
        // 4. LONG HOLD — losing market movement
        //
        // 105 -> 103
        // ====================================================================

        const auto long_loss =
            environment.step(
                DevAction::HOLD,
                T3,
                103.0);


        const auto long_loss_reconciliation =
            reconcileStep(
                long_loss,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            long_loss.gross_pnl < 0.0,
            "LONG position should lose when price falls.");


        require(
            long_loss_reconciliation.reconciled,
            "LONG losing HOLD reconciliation failed.");


        std::cout
            << "[PASS] LONG losing HOLD reconciliation\n";


        // ====================================================================
        // 5. LONG -> EXIT
        //
        // Price remains 103.
        //
        // Therefore this step has no market-movement gross P&L.
        // Only exit transaction costs + exit slippage affect equity.
        // ====================================================================

        const auto long_exit =
            environment.step(
                DevAction::EXIT,
                T4,
                103.0);


        const auto long_exit_reconciliation =
            reconcileStep(
                long_exit,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            approximatelyEqual(
                long_exit.gross_pnl,
                0.0),
            "Constant-price LONG EXIT must have zero "
            "market-movement gross P&L.");


        require(
            long_exit.brokerage_charges > 0.0,
            "LONG EXIT must contain transaction costs.");


        require(
            long_exit.slippage_cost > 0.0,
            "LONG EXIT must contain SELL slippage.");


        require(
            approximatelyEqual(
                environment.state().position,
                0.0),
            "LONG EXIT did not leave environment FLAT.");


        require(
            long_exit_reconciliation.reconciled,
            "LONG EXIT reconciliation failed.");


        verifyCumulativeCosts(
            environment,
            tracker);


        std::cout
            << "[PASS] LONG -> EXIT accounting reconciliation\n";


        // ====================================================================
        // 6. FLAT -> SHORT
        // ====================================================================

        const auto short_entry =
            environment.step(
                DevAction::SHORT,
                T5,
                103.0);


        const auto short_entry_reconciliation =
            reconcileStep(
                short_entry,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            approximatelyEqual(
                short_entry.gross_pnl,
                0.0),
            "SHORT entry must not receive same-step "
            "market-movement P&L.");


        require(
            short_entry.slippage_cost > 0.0,
            "SHORT entry must contain SELL slippage.");


        require(
            short_entry_reconciliation.reconciled,
            "SHORT entry reconciliation failed.");


        std::cout
            << "[PASS] FLAT -> SHORT accounting reconciliation\n";


        // ====================================================================
        // 7. SHORT HOLD — profitable market movement
        //
        // 103 -> 98
        // ====================================================================

        const auto short_profit =
            environment.step(
                DevAction::HOLD,
                T6,
                98.0);


        const auto short_profit_reconciliation =
            reconcileStep(
                short_profit,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            short_profit.gross_pnl > 0.0,
            "SHORT position should profit when price falls.");


        require(
            approximatelyEqual(
                short_profit.brokerage_charges,
                0.0) &&
            approximatelyEqual(
                short_profit.slippage_cost,
                0.0),
            "SHORT HOLD must not generate execution costs.");


        require(
            short_profit_reconciliation.reconciled,
            "SHORT profitable HOLD reconciliation failed.");


        std::cout
            << "[PASS] SHORT profitable HOLD reconciliation\n";


        // ====================================================================
        // 8. SHORT HOLD — losing market movement
        //
        // 98 -> 100
        // ====================================================================

        const auto short_loss =
            environment.step(
                DevAction::HOLD,
                T7,
                100.0);


        const auto short_loss_reconciliation =
            reconcileStep(
                short_loss,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            short_loss.gross_pnl < 0.0,
            "SHORT position should lose when price rises.");


        require(
            short_loss_reconciliation.reconciled,
            "SHORT losing HOLD reconciliation failed.");


        std::cout
            << "[PASS] SHORT losing HOLD reconciliation\n";


        // ====================================================================
        // 9. SHORT -> LONG direct reversal
        //
        // Constant market price:
        //
        //   close SHORT = BUY
        //   open LONG   = BUY
        //
        // Two executions.
        // ====================================================================

        const auto short_to_long =
            environment.step(
                DevAction::LONG,
                T8,
                100.0);


        const auto short_to_long_reconciliation =
            reconcileStep(
                short_to_long,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            short_to_long.executions.size() == 2,
            "SHORT -> LONG reversal must contain "
            "two executions.");


        require(
            short_to_long.brokerage_charges > 0.0,
            "SHORT -> LONG reversal must contain "
            "transaction costs.");


        require(
            short_to_long.slippage_cost > 0.0,
            "SHORT -> LONG reversal must contain "
            "slippage.");


        require(
            approximatelyEqual(
                environment.state().position,
                1.0),
            "SHORT -> LONG reversal did not produce "
            "LONG position.");


        require(
            short_to_long_reconciliation.reconciled,
            "SHORT -> LONG reconciliation failed.");


        verifyCumulativeCosts(
            environment,
            tracker);


        std::cout
            << "[PASS] SHORT -> LONG reversal reconciliation\n";


        // ====================================================================
        // 10. LONG -> SHORT direct reversal
        //
        // Constant market price:
        //
        //   close LONG = SELL
        //   open SHORT = SELL
        //
        // Two executions.
        // ====================================================================

        const auto long_to_short =
            environment.step(
                DevAction::SHORT,
                T9,
                100.0);


        const auto long_to_short_reconciliation =
            reconcileStep(
                long_to_short,
                environment,
                transaction_cost_model,
                slippage_model,
                pnl_integration,
                tracker,
                INITIAL_EQUITY);


        require(
            long_to_short.executions.size() == 2,
            "LONG -> SHORT reversal must contain "
            "two executions.");


        require(
            long_to_short.brokerage_charges > 0.0,
            "LONG -> SHORT reversal must contain "
            "transaction costs.");


        require(
            long_to_short.slippage_cost > 0.0,
            "LONG -> SHORT reversal must contain "
            "slippage.");


        require(
            approximatelyEqual(
                environment.state().position,
                -1.0),
            "LONG -> SHORT reversal did not produce "
            "SHORT position.");


        require(
            long_to_short_reconciliation.reconciled,
            "LONG -> SHORT reconciliation failed.");


        verifyCumulativeCosts(
            environment,
            tracker);


        std::cout
            << "[PASS] LONG -> SHORT reversal reconciliation\n";


        // ====================================================================
        // 11. Final cumulative accounting reconciliation
        // ====================================================================

        const auto& final_state =
            environment.state();


        const double expected_net_pnl =
            tracker.cumulative_market_gross_pnl -
            tracker.cumulative_transaction_costs -
            tracker.cumulative_slippage_cost;


        require(
            approximatelyEqual(
                final_state.net_pnl,
                expected_net_pnl),
            "Final cumulative net P&L reconciliation failed.");


        require(
            approximatelyEqual(
                final_state.equity,
                INITIAL_EQUITY +
                expected_net_pnl),
            "Final cumulative equity reconciliation failed.");


        std::cout
            << "[PASS] Final cumulative P&L / equity reconciliation\n";


        // ====================================================================
        // 12. Explicit no-double-slippage verification
        // ====================================================================

        const double single_slippage_net_pnl =
            tracker.cumulative_market_gross_pnl -
            tracker.cumulative_transaction_costs -
            tracker.cumulative_slippage_cost;


        const double double_slippage_net_pnl =
            tracker.cumulative_market_gross_pnl -
            tracker.cumulative_transaction_costs -
            (2.0 * tracker.cumulative_slippage_cost);


        require(
            approximatelyEqual(
                final_state.net_pnl,
                single_slippage_net_pnl),
            "Environment does not match single-slippage accounting.");


        if (tracker.cumulative_slippage_cost > 0.0)
        {
            require(
                !approximatelyEqual(
                    final_state.net_pnl,
                    double_slippage_net_pnl),
                "Double-slippage deduction detected.");
        }


        std::cout
            << "[PASS] No-double-slippage verification\n";


        // ====================================================================
        // 13. Cost-source reconciliation
        // ====================================================================

        verifyCumulativeCosts(
            environment,
            tracker);


        std::cout
            << "[PASS] Phase 2.4 / 2.5 cumulative cost reconciliation\n";


        // ====================================================================
        // 14. Cross-episode reset isolation
        // ====================================================================

        environment.reset(
            "SBIN",
            T0,
            500.0);


        tracker =
            AccountingTracker{};


        require(
            approximatelyEqual(
                environment.state().net_pnl,
                0.0),
            "Environment net P&L leaked across reset.");


        require(
            approximatelyEqual(
                environment.state().cumulative_brokerage_charges,
                0.0),
            "Transaction costs leaked across reset.");


        require(
            approximatelyEqual(
                environment.state().cumulative_slippage_cost,
                0.0),
            "Slippage costs leaked across reset.");


        require(
            approximatelyEqual(
                environment.state().cumulative_execution_cost,
                0.0),
            "Execution costs leaked across reset.");


        require(
            approximatelyEqual(
                environment.state().equity,
                INITIAL_EQUITY),
            "Equity leaked across reset.");


        std::cout
            << "[PASS] Cross-episode accounting isolation\n";


        // ====================================================================
        // 15. Fresh episode shared-accounting reconciliation
        // ====================================================================

        PnLReconciliationInput reset_reconciliation;

        reset_reconciliation.integration.initial_equity =
            INITIAL_EQUITY;

        reset_reconciliation.integration.realized_gross_pnl =
            0.0;

        reset_reconciliation.integration.unrealized_gross_pnl =
            0.0;

        reset_reconciliation.integration.transaction_costs =
            0.0;

        reset_reconciliation.integration.slippage_cost =
            0.0;

        reset_reconciliation.expected_net_pnl =
            environment.state().net_pnl;

        reset_reconciliation.expected_equity =
            environment.state().equity;


        const auto reset_result =
            pnl_integration.reconcile(
                reset_reconciliation);


        require(
            reset_result.reconciled,
            "Fresh-episode CETE accounting reconciliation failed.");


        std::cout
            << "[PASS] Fresh-episode CETE accounting reconciliation\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Execution owner             : TradingEnvironment\n"
            << "Transaction-cost source     : Phase 2.4 TransactionCostModel\n"
            << "Slippage source             : Phase 2.5 SlippageModel\n"
            << "Accounting base             : CETE PnLAccountingEngine\n"
            << "Integration layer           : CETE PnLAccountingIntegration\n"
            << "LONG profitable P&L         : VERIFIED\n"
            << "LONG losing P&L             : VERIFIED\n"
            << "LONG exit                   : VERIFIED\n"
            << "SHORT profitable P&L        : VERIFIED\n"
            << "SHORT losing P&L            : VERIFIED\n"
            << "SHORT entry                 : VERIFIED\n"
            << "SHORT -> LONG reversal      : VERIFIED\n"
            << "LONG -> SHORT reversal      : VERIFIED\n"
            << "Transaction-cost flow       : VERIFIED\n"
            << "Slippage-cost flow          : VERIFIED\n"
            << "Cumulative gross P&L        : VERIFIED\n"
            << "Cumulative net P&L          : VERIFIED\n"
            << "Equity reconciliation       : VERIFIED\n"
            << "Single slippage deduction   : VERIFIED\n"
            << "Double slippage deduction   : NONE\n"
            << "Cross-episode leakage       : NONE\n"
            << "Duplicate P&L engine        : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.6B TRADING ENVIRONMENT / CETE P&L RECONCILIATION PASSED\n"
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