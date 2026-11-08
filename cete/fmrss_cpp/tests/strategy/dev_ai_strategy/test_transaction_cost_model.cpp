#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "devai/strategy/dev_ai_strategy/PositionExposureState.hpp"
#include "devai/strategy/dev_ai_strategy/TransactionCostModel.hpp"

namespace
{

using namespace devai::strategy;
using devai::strategy::brokerage::ExecutionSide;


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


double componentSum(
    const TransactionCostSummary& summary)
{
    return
        summary.brokerage +
        summary.stt +
        summary.exchange_transaction_charge +
        summary.sebi_charge +
        summary.stamp_duty +
        summary.gst;
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
        << "DEVAI PHASE 2.4 — TRANSACTION-COST MODEL VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        // ====================================================================
        // Configuration
        //
        // Slippage is intentionally disabled.
        //
        // Phase 2.4 must isolate transaction charges from Phase 2.5 slippage.
        // ====================================================================

        TradingEnvironmentConfig config;

        config.initial_equity =
            100000.0;

        config.exposure_fraction =
            1.0;

        config.slippage_rate =
            0.0;


        PositionExposureState position_state(
            config);

        TransactionCostModel
            transaction_cost_model;


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


        constexpr double PRICE =
            100.0;


        // ====================================================================
        // Reset
        // ====================================================================

        position_state.reset(
            "RELIANCE",
            T0,
            PRICE);


        const double initial_equity =
            position_state.
                environmentState().
                equity;


        require(
            approximatelyEqual(
                initial_equity,
                config.initial_equity),
            "Initial equity mismatch.");


        // ====================================================================
        // 2.4.1 BUY-side transaction costs
        //
        // FLAT -> LONG creates one BUY execution.
        // ====================================================================

        const auto long_entry =
            position_state.step(
                DevAction::LONG,
                T1,
                PRICE);


        const auto buy_summary =
            transaction_cost_model.summarize(
                long_entry);


        require(
            buy_summary.execution_count == 1,
            "FLAT -> LONG must create exactly one execution.");

        require(
            buy_summary.has_execution,
            "BUY transaction was not detected.");

        require(
            long_entry.executions.size() == 1,
            "LONG entry execution count mismatch.");

        require(
            long_entry.executions.front().side ==
                ExecutionSide::BUY,
            "FLAT -> LONG must execute on BUY side.");

        require(
            buy_summary.turnover > 0.0,
            "BUY turnover must be positive.");

        require(
            buy_summary.brokerage > 0.0,
            "BUY brokerage must be positive.");

        require(
            buy_summary.exchange_transaction_charge > 0.0,
            "BUY exchange charge must be positive.");

        require(
            buy_summary.sebi_charge > 0.0,
            "BUY SEBI charge must be positive.");

        require(
            buy_summary.stamp_duty > 0.0,
            "BUY stamp duty must be positive.");

        require(
            buy_summary.gst > 0.0,
            "BUY GST must be positive.");

        require(
            approximatelyEqual(
                buy_summary.stt,
                0.0),
            "BUY-side STT must be zero for the configured "
            "intraday brokerage model.");

        require(
            buy_summary.total_charges > 0.0,
            "BUY total charges must be positive.");

        require(
            buy_summary.numerical_integrity,
            "BUY transaction-cost numerical integrity failed.");

        std::cout
            << "[PASS] BUY-side transaction costs\n";


        // ====================================================================
        // 2.4.2 Component-sum integrity
        // ====================================================================

        require(
            approximatelyEqual(
                componentSum(
                    buy_summary),
                buy_summary.total_charges),
            "BUY charge components do not sum to total.");

        require(
            approximatelyEqual(
                buy_summary.total_charges,
                long_entry.brokerage_charges),
            "BUY aggregate does not match TradingEnvironment.");

        std::cout
            << "[PASS] Transaction-cost component-sum integrity\n";


        // ====================================================================
        // 2.4.3 Equity propagation exactly once
        //
        // Slippage = 0
        // Market price has not moved
        //
        // Therefore the only equity reduction on entry must be the
        // transaction charges.
        // ====================================================================

        require(
            approximatelyEqual(
                initial_equity -
                    long_entry.equity_after,
                buy_summary.total_charges),
            "BUY transaction charges were not propagated "
            "to equity exactly once.");

        std::cout
            << "[PASS] BUY transaction-cost equity propagation\n";


        // ====================================================================
        // 2.4.4 HOLD creates zero transaction costs
        // ====================================================================

        const double equity_before_hold =
            position_state.
                environmentState().
                equity;


        const auto hold_result =
            position_state.step(
                DevAction::HOLD,
                T2,
                PRICE);


        const auto hold_summary =
            transaction_cost_model.summarize(
                hold_result);


        require(
            hold_summary.execution_count == 0,
            "HOLD unexpectedly created an execution.");

        require(
            !hold_summary.has_execution,
            "HOLD incorrectly reports an execution.");

        require(
            approximatelyEqual(
                hold_summary.turnover,
                0.0),
            "HOLD turnover must be zero.");

        require(
            approximatelyEqual(
                hold_summary.total_charges,
                0.0),
            "HOLD transaction charges must be zero.");

        require(
            approximatelyEqual(
                hold_result.brokerage_charges,
                0.0),
            "TradingEnvironment charged brokerage on HOLD.");

        require(
            approximatelyEqual(
                hold_result.equity_after,
                equity_before_hold),
            "HOLD changed equity with unchanged market price.");

        std::cout
            << "[PASS] HOLD produces zero transaction costs\n";


        // ====================================================================
        // 2.4.5 LONG -> SHORT direct reversal
        //
        // This must contain:
        //
        //   SELL to close LONG
        //   SELL to open SHORT
        //
        // Therefore two execution legs.
        // ====================================================================

        const double equity_before_reversal =
            position_state.
                environmentState().
                equity;


        const auto reversal =
            position_state.step(
                DevAction::SHORT,
                T3,
                PRICE);


        const auto reversal_summary =
            transaction_cost_model.summarize(
                reversal);


        require(
            reversal_summary.execution_count == 2,
            "LONG -> SHORT reversal must create "
            "exactly two executions.");

        require(
            reversal.executions.size() == 2,
            "Reversal execution vector must contain two legs.");

        require(
            reversal.executions[0].side ==
                ExecutionSide::SELL,
            "LONG close must be a SELL execution.");

        require(
            reversal.executions[1].side ==
                ExecutionSide::SELL,
            "SHORT open must be a SELL execution.");

        require(
            reversal_summary.turnover > 0.0,
            "Reversal turnover must be positive.");

        require(
            reversal_summary.stt > 0.0,
            "SELL-side reversal must contain STT.");

        require(
            approximatelyEqual(
                reversal_summary.stamp_duty,
                0.0),
            "SELL-only reversal must not contain BUY stamp duty.");

        require(
            reversal_summary.total_charges > 0.0,
            "Reversal transaction costs must be positive.");

        require(
            approximatelyEqual(
                reversal_summary.total_charges,
                reversal.brokerage_charges),
            "Reversal aggregate does not match "
            "TradingEnvironment charges.");

        std::cout
            << "[PASS] LONG -> SHORT two-leg transaction costs\n";


        // ====================================================================
        // 2.4.6 Reversal equity propagation
        //
        // Price unchanged + slippage zero.
        //
        // Therefore reversal equity change must equal transaction charges.
        // ====================================================================

        require(
            approximatelyEqual(
                equity_before_reversal -
                    reversal.equity_after,
                reversal_summary.total_charges),
            "Reversal transaction charges were not propagated "
            "to equity exactly once.");

        std::cout
            << "[PASS] Reversal transaction-cost equity propagation\n";


        // ====================================================================
        // 2.4.7 SHORT HOLD => zero transaction costs
        // ====================================================================

        const auto short_hold =
            position_state.step(
                DevAction::HOLD,
                T4,
                PRICE);


        const auto short_hold_summary =
            transaction_cost_model.summarize(
                short_hold);


        require(
            short_hold_summary.execution_count == 0,
            "SHORT HOLD unexpectedly created execution.");

        require(
            approximatelyEqual(
                short_hold_summary.total_charges,
                0.0),
            "SHORT HOLD transaction cost must be zero.");

        std::cout
            << "[PASS] SHORT HOLD produces zero transaction costs\n";


        // ====================================================================
        // 2.4.8 SHORT -> EXIT
        //
        // Closing SHORT requires BUY.
        // ====================================================================

        const double equity_before_exit =
            position_state.
                environmentState().
                equity;


        const auto short_exit =
            position_state.step(
                DevAction::EXIT,
                T5,
                PRICE);


        const auto exit_summary =
            transaction_cost_model.summarize(
                short_exit);


        require(
            exit_summary.execution_count == 1,
            "SHORT -> EXIT must create one execution.");

        require(
            short_exit.executions.size() == 1,
            "SHORT exit execution count mismatch.");

        require(
            short_exit.executions.front().side ==
                ExecutionSide::BUY,
            "SHORT close must execute on BUY side.");

        require(
            approximatelyEqual(
                exit_summary.stt,
                0.0),
            "BUY-side SHORT exit must not contain SELL STT.");

        require(
            exit_summary.stamp_duty > 0.0,
            "BUY-side SHORT exit must contain stamp duty.");

        require(
            exit_summary.total_charges > 0.0,
            "SHORT exit transaction charges must be positive.");

        std::cout
            << "[PASS] SHORT -> EXIT BUY-side transaction costs\n";


        // ====================================================================
        // 2.4.9 EXIT equity propagation
        // ====================================================================

        require(
            approximatelyEqual(
                equity_before_exit -
                    short_exit.equity_after,
                exit_summary.total_charges),
            "EXIT transaction charges were not propagated "
            "to equity exactly once.");

        std::cout
            << "[PASS] EXIT transaction-cost equity propagation\n";


        // ====================================================================
        // 2.4.10 Cumulative transaction-cost accounting
        // ====================================================================

        const double expected_cumulative_charges =
            buy_summary.total_charges +
            reversal_summary.total_charges +
            exit_summary.total_charges;


        const auto& final_state =
            position_state.environmentState();


        require(
            approximatelyEqual(
                final_state.
                    cumulative_brokerage_charges,
                expected_cumulative_charges),
            "Cumulative brokerage charges mismatch.");

        std::cout
            << "[PASS] Cumulative transaction-cost accounting\n";


        // ====================================================================
        // 2.4.11 Final equity reconciliation
        //
        // Every test step used the same market price and slippage = 0.
        //
        // There is therefore no market P&L and no slippage P&L.
        //
        // Final equity must equal:
        //
        // initial equity - all transaction charges
        // ====================================================================

        const double expected_final_equity =
            initial_equity -
            expected_cumulative_charges;


        require(
            approximatelyEqual(
                final_state.equity,
                expected_final_equity),
            "Final equity does not reconcile with "
            "cumulative transaction charges.");

        require(
            approximatelyEqual(
                final_state.net_pnl,
                -expected_cumulative_charges),
            "Final net P&L does not reconcile with "
            "transaction charges.");

        std::cout
            << "[PASS] Final equity / transaction-cost reconciliation\n";


        // ====================================================================
        // 2.4.12 BUY / SELL asymmetry
        // ====================================================================

        require(
            buy_summary.stamp_duty > 0.0,
            "BUY-side stamp-duty rule not observed.");

        require(
            approximatelyEqual(
                buy_summary.stt,
                0.0),
            "BUY-side STT asymmetry failed.");

        require(
            reversal_summary.stt > 0.0,
            "SELL-side STT rule not observed.");

        require(
            approximatelyEqual(
                reversal_summary.stamp_duty,
                0.0),
            "SELL-side stamp-duty asymmetry failed.");

        std::cout
            << "[PASS] BUY / SELL statutory-charge asymmetry\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Cost source of truth       : CETE BrokerageChargesEngine\n"
            << "Execution owner            : TradingEnvironment\n"
            << "Phase-2.4 role             : Integration / aggregation / validation\n"
            << "BUY-side charges           : VERIFIED\n"
            << "SELL-side charges          : VERIFIED\n"
            << "Brokerage                  : VERIFIED\n"
            << "STT                        : VERIFIED\n"
            << "Exchange transaction charge: VERIFIED\n"
            << "SEBI charge                : VERIFIED\n"
            << "Stamp duty                 : VERIFIED\n"
            << "GST                        : VERIFIED\n"
            << "Component-sum integrity    : VERIFIED\n"
            << "HOLD zero-cost behavior    : VERIFIED\n"
            << "Two-leg reversal costs     : VERIFIED\n"
            << "Cumulative charges         : VERIFIED\n"
            << "Equity propagation         : VERIFIED\n"
            << "Double charging            : NONE\n"
            << "Slippage in Phase 2.4      : EXCLUDED\n"
            << "Duplicate brokerage engine : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.4 TRANSACTION-COST MODEL VALIDATION PASSED\n"
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