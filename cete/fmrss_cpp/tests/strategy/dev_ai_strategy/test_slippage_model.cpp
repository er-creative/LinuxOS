#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "devai/strategy/dev_ai_strategy/PositionExposureState.hpp"
#include "devai/strategy/dev_ai_strategy/SlippageModel.hpp"

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


double expectedSlippage(
    const ExecutionRecord& execution)
{
    return
        std::fabs(
            execution.execution_price -
            execution.market_price) *
        execution.quantity;
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
        << "DEVAI PHASE 2.5 — SLIPPAGE MODEL VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        // ====================================================================
        // Configuration
        //
        // 0.0002 = 2 basis points.
        //
        // Brokerage remains active because Phase 2.5 must prove that
        // slippage and transaction costs remain separate.
        // ====================================================================

        TradingEnvironmentConfig config;

        config.initial_equity =
            100000.0;

        config.exposure_fraction =
            1.0;

        config.slippage_rate =
            0.0002;


        PositionExposureState position_state(
            config);

        SlippageModel slippage_model;


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


        // ====================================================================
        // 2.5.1 LONG entry
        //
        // FLAT -> LONG = BUY.
        //
        // BUY execution must occur ABOVE market.
        // ====================================================================

        const auto long_entry =
            position_state.step(
                DevAction::LONG,
                T1,
                PRICE);


        const auto long_summary =
            slippage_model.summarize(
                long_entry);


        require(
            long_summary.execution_count == 1,
            "LONG entry must contain one execution.");

        require(
            long_summary.buy_execution_count == 1,
            "LONG entry must contain one BUY.");

        require(
            long_summary.sell_execution_count == 0,
            "LONG entry unexpectedly contains SELL.");

        require(
            long_entry.executions.size() == 1,
            "LONG entry execution vector mismatch.");


        const auto& long_execution =
            long_entry.executions.front();


        require(
            long_execution.side ==
                ExecutionSide::BUY,
            "LONG entry must execute BUY.");

        require(
            long_execution.execution_price >
                long_execution.market_price,
            "BUY execution price must be above market "
            "when slippage is positive.");

        require(
            approximatelyEqual(
                long_execution.execution_price,
                PRICE *
                    (1.0 +
                     config.slippage_rate)),
            "LONG entry execution price does not match "
            "configured slippage.");

        require(
            approximatelyEqual(
                long_execution.slippage_cost,
                expectedSlippage(
                    long_execution)),
            "LONG entry slippage-cost identity failed.");

        require(
            long_summary.total_slippage_cost > 0.0,
            "LONG entry slippage must be positive.");

        std::cout
            << "[PASS] LONG-entry BUY slippage\n";


        // ====================================================================
        // 2.5.2 LONG opening slippage reaches equity
        //
        // Price has not moved yet.
        //
        // Equity loss consists of:
        //
        //   opening slippage
        //   +
        //   brokerage/statutory charges
        //
        // Slippage must appear exactly once.
        // ====================================================================

        const double expected_long_entry_loss =
            long_entry.slippage_cost +
            long_entry.brokerage_charges;


        require(
            approximatelyEqual(
                initial_equity -
                    long_entry.equity_after,
                expected_long_entry_loss),
            "LONG opening slippage was not propagated "
            "to equity exactly once.");

        std::cout
            << "[PASS] LONG-entry slippage equity propagation\n";


        // ====================================================================
        // 2.5.3 HOLD has no slippage
        // ====================================================================

        const double equity_before_hold =
            position_state.
                environmentState().
                equity;


        const auto long_hold =
            position_state.step(
                DevAction::HOLD,
                T2,
                PRICE);


        const auto hold_summary =
            slippage_model.summarize(
                long_hold);


        require(
            hold_summary.execution_count == 0,
            "HOLD unexpectedly created an execution.");

        require(
            !hold_summary.has_execution,
            "HOLD incorrectly reports execution.");

        require(
            approximatelyEqual(
                hold_summary.total_slippage_cost,
                0.0),
            "HOLD slippage must be zero.");

        require(
            approximatelyEqual(
                long_hold.slippage_cost,
                0.0),
            "TradingEnvironment reported slippage on HOLD.");

        require(
            approximatelyEqual(
                long_hold.equity_after,
                equity_before_hold),
            "HOLD changed equity at unchanged market price.");

        std::cout
            << "[PASS] HOLD produces zero slippage\n";


        // ====================================================================
        // 2.5.4 LONG -> SHORT direct reversal
        //
        // Two SELL executions:
        //
        //   1. close LONG
        //   2. open SHORT
        //
        // Both must execute BELOW market.
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
            slippage_model.summarize(
                reversal);


        require(
            reversal_summary.execution_count == 2,
            "LONG -> SHORT reversal must contain "
            "two executions.");

        require(
            reversal_summary.buy_execution_count == 0,
            "LONG -> SHORT reversal unexpectedly contains BUY.");

        require(
            reversal_summary.sell_execution_count == 2,
            "LONG -> SHORT reversal must contain two SELLs.");

        require(
            reversal.executions.size() == 2,
            "Reversal execution-vector size mismatch.");


        for (const auto& execution :
             reversal.executions)
        {
            require(
                execution.side ==
                    ExecutionSide::SELL,
                "LONG -> SHORT reversal execution "
                "must be SELL.");

            require(
                execution.execution_price <
                    execution.market_price,
                "SELL execution price must be below "
                "market with positive slippage.");

            require(
                approximatelyEqual(
                    execution.execution_price,
                    PRICE *
                        (1.0 -
                         config.slippage_rate)),
                "SELL execution price does not match "
                "configured slippage.");

            require(
                approximatelyEqual(
                    execution.slippage_cost,
                    expectedSlippage(
                        execution)),
                "Reversal slippage-cost identity failed.");
        }


        require(
            reversal_summary.total_slippage_cost > 0.0,
            "Reversal slippage must be positive.");

        require(
            approximatelyEqual(
                reversal_summary.total_slippage_cost,
                reversal.slippage_cost),
            "Reversal aggregate slippage mismatch.");

        std::cout
            << "[PASS] LONG -> SHORT two-leg SELL slippage\n";


        // ====================================================================
        // 2.5.5 Reversal equity propagation
        //
        // At unchanged market price:
        //
        // equity reduction =
        //
        //   exit slippage
        // + new-entry slippage
        // + brokerage/statutory charges
        //
        // exactly once.
        // ====================================================================

        const double expected_reversal_loss =
            reversal.slippage_cost +
            reversal.brokerage_charges;


        require(
            approximatelyEqual(
                equity_before_reversal -
                    reversal.equity_after,
                expected_reversal_loss),
            "Reversal slippage was not propagated "
            "to equity exactly once.");

        std::cout
            << "[PASS] Reversal slippage equity propagation\n";


        // ====================================================================
        // 2.5.6 SHORT HOLD
        // ====================================================================

        const double equity_before_short_hold =
            position_state.
                environmentState().
                equity;


        const auto short_hold =
            position_state.step(
                DevAction::HOLD,
                T4,
                PRICE);


        const auto short_hold_summary =
            slippage_model.summarize(
                short_hold);


        require(
            short_hold_summary.execution_count == 0,
            "SHORT HOLD unexpectedly created execution.");

        require(
            approximatelyEqual(
                short_hold_summary.total_slippage_cost,
                0.0),
            "SHORT HOLD slippage must be zero.");

        require(
            approximatelyEqual(
                short_hold.equity_after,
                equity_before_short_hold),
            "SHORT HOLD changed equity at unchanged price.");

        std::cout
            << "[PASS] SHORT HOLD produces zero slippage\n";


        // ====================================================================
        // 2.5.7 SHORT -> EXIT
        //
        // Closing SHORT requires BUY.
        // BUY executes ABOVE market.
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
            slippage_model.summarize(
                short_exit);


        require(
            exit_summary.execution_count == 1,
            "SHORT -> EXIT must contain one execution.");

        require(
            exit_summary.buy_execution_count == 1,
            "SHORT -> EXIT must contain one BUY.");

        require(
            exit_summary.sell_execution_count == 0,
            "SHORT -> EXIT unexpectedly contains SELL.");


        const auto& exit_execution =
            short_exit.executions.front();


        require(
            exit_execution.side ==
                ExecutionSide::BUY,
            "SHORT close must execute BUY.");

        require(
            exit_execution.execution_price >
                exit_execution.market_price,
            "SHORT-close BUY must execute above market.");

        require(
            approximatelyEqual(
                exit_execution.execution_price,
                PRICE *
                    (1.0 +
                     config.slippage_rate)),
            "SHORT-close execution price does not match "
            "configured slippage.");

        require(
            approximatelyEqual(
                exit_execution.slippage_cost,
                expectedSlippage(
                    exit_execution)),
            "SHORT-exit slippage-cost identity failed.");

        std::cout
            << "[PASS] SHORT -> EXIT BUY slippage\n";


        // ====================================================================
        // 2.5.8 EXIT equity propagation
        // ====================================================================

        const double expected_exit_loss =
            short_exit.slippage_cost +
            short_exit.brokerage_charges;


        require(
            approximatelyEqual(
                equity_before_exit -
                    short_exit.equity_after,
                expected_exit_loss),
            "SHORT-exit slippage was not propagated "
            "to equity exactly once.");

        std::cout
            << "[PASS] EXIT slippage equity propagation\n";


        // ====================================================================
        // 2.5.9 Cumulative slippage accounting
        // ====================================================================

        const double expected_cumulative_slippage =
            long_summary.total_slippage_cost +
            reversal_summary.total_slippage_cost +
            exit_summary.total_slippage_cost;


        const auto& final_state =
            position_state.environmentState();


        require(
            approximatelyEqual(
                final_state.
                    cumulative_slippage_cost,
                expected_cumulative_slippage),
            "Cumulative slippage accounting mismatch.");

        std::cout
            << "[PASS] Cumulative slippage accounting\n";


        // ====================================================================
        // 2.5.10 Transaction costs remain separate
        // ====================================================================

        require(
            long_entry.brokerage_charges > 0.0,
            "Brokerage unexpectedly absent.");

        require(
            long_entry.slippage_cost > 0.0,
            "Slippage unexpectedly absent.");

        require(
            approximatelyEqual(
                long_entry.execution_cost,
                long_entry.brokerage_charges +
                    long_entry.slippage_cost),
            "Execution cost does not reconcile brokerage "
            "and slippage.");

        require(
            approximatelyEqual(
                reversal.execution_cost,
                reversal.brokerage_charges +
                    reversal.slippage_cost),
            "Reversal execution cost does not reconcile.");

        require(
            approximatelyEqual(
                short_exit.execution_cost,
                short_exit.brokerage_charges +
                    short_exit.slippage_cost),
            "Exit execution cost does not reconcile.");

        std::cout
            << "[PASS] Brokerage / slippage separation\n";


        // ====================================================================
        // 2.5.11 Final account reconciliation
        //
        // Every market price in this test is exactly 100.
        //
        // Therefore:
        //
        // market P&L = 0
        //
        // Final loss must consist entirely of:
        //
        // cumulative brokerage/statutory charges
        // +
        // cumulative slippage
        // ====================================================================

        const double expected_total_loss =
            final_state.cumulative_brokerage_charges +
            final_state.cumulative_slippage_cost;


        require(
            approximatelyEqual(
                initial_equity -
                    final_state.equity,
                expected_total_loss),
            "Final equity does not reconcile with "
            "brokerage plus slippage.");

        require(
            approximatelyEqual(
                final_state.net_pnl,
                -expected_total_loss),
            "Final net P&L does not reconcile with "
            "brokerage plus slippage.");

        std::cout
            << "[PASS] Final equity / slippage reconciliation\n";


        // ====================================================================
        // 2.5.12 Reset isolation
        // ====================================================================

        position_state.reset(
            "SBIN",
            T0,
            200.0);


        const auto& reset_state =
            position_state.environmentState();


        require(
            approximatelyEqual(
                reset_state.
                    cumulative_slippage_cost,
                0.0),
            "Slippage leaked across episode reset.");

        require(
            approximatelyEqual(
                reset_state.
                    cumulative_brokerage_charges,
                0.0),
            "Brokerage leaked across episode reset.");

        require(
            approximatelyEqual(
                reset_state.equity,
                config.initial_equity),
            "Equity did not reset between episodes.");

        std::cout
            << "[PASS] Cross-episode slippage isolation\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Slippage source of truth    : TradingEnvironment\n"
            << "Phase-2.5 role              : Integration / aggregation / validation\n"
            << "Configured slippage rate    : "
            << config.slippage_rate
            << "\n"
            << "BUY adverse execution       : VERIFIED\n"
            << "SELL adverse execution      : VERIFIED\n"
            << "LONG-entry slippage         : VERIFIED\n"
            << "SHORT-entry slippage        : VERIFIED VIA REVERSAL\n"
            << "LONG-exit slippage          : VERIFIED VIA REVERSAL\n"
            << "SHORT-exit slippage         : VERIFIED\n"
            << "HOLD zero-slippage behavior : VERIFIED\n"
            << "Two-leg reversal slippage   : VERIFIED\n"
            << "Slippage-cost identity      : VERIFIED\n"
            << "Cumulative slippage         : VERIFIED\n"
            << "Equity propagation          : VERIFIED\n"
            << "Brokerage separation        : VERIFIED\n"
            << "Double slippage deduction   : NONE\n"
            << "Cross-episode isolation     : VERIFIED\n"
            << "Duplicate slippage engine   : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.5 SLIPPAGE MODEL VALIDATION PASSED\n"
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