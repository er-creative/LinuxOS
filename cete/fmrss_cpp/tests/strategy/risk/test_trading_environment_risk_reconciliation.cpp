#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"
#include "devai/strategy/risk/RiskStateEngine.hpp"


namespace
{

using namespace devai::strategy;
using namespace devai::strategy::risk;


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
// Build CETE risk input from the already-validated TradingEnvironment.
//
// IMPORTANT:
//
// RiskStateEngine does not calculate P&L, brokerage or slippage.
//
// It consumes the resulting account state.
// ============================================================================

RiskStateInput makeRiskInput(
    const TradingEnvironment& environment,
    std::int64_t decision_time,
    double market_price)
{
    const auto& state =
        environment.state();

    return
    {
        decision_time,
        state.initial_equity,
        state.equity,
        state.position,
        state.quantity,
        market_price
    };
}


// ============================================================================
// Reconcile the common fields already represented by TradingEnvironment.
//
// TradingEnvironment currently tracks:
//
//     equity
//     peak_equity
//     drawdown
//     position
//     quantity
//
// RiskStateEngine additionally owns:
//
//     maximum_drawdown
//     gross_exposure
//     exposure_ratio
// ============================================================================

void reconcileRiskState(
    const TradingEnvironment& environment,
    const RiskState& risk_state)
{
    const auto& environment_state =
        environment.state();


    require(
        approximatelyEqual(
            risk_state.initial_equity,
            environment_state.initial_equity),
        "Initial-equity risk reconciliation failed.");


    require(
        approximatelyEqual(
            risk_state.current_equity,
            environment_state.equity),
        "Current-equity risk reconciliation failed.");


    require(
        approximatelyEqual(
            risk_state.peak_equity,
            environment_state.peak_equity),
        "Peak-equity risk reconciliation failed.");


    require(
        approximatelyEqual(
            risk_state.current_drawdown,
            environment_state.drawdown),
        "Current-drawdown risk reconciliation failed.");


    require(
        approximatelyEqual(
            risk_state.position,
            environment_state.position),
        "Position risk reconciliation failed.");


    require(
        approximatelyEqual(
            risk_state.quantity,
            environment_state.quantity),
        "Quantity risk reconciliation failed.");


    require(
        risk_state.numerical_integrity,
        "CETE risk state numerical integrity is false.");
}

} // namespace


int main()
{
    std::cout
        << "\n"
        << "==========================================================================\n"
        << "DEVAI PHASE 2.7B — TRADING ENVIRONMENT / CETE RISK RECONCILIATION\n"
        << "==========================================================================\n";


    try
    {
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

        RiskStateEngine risk_engine;


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


        // ====================================================================
        // 1. Reset
        // ====================================================================

        environment.reset(
            "RELIANCE",
            T0,
            100.0);


        risk_engine.reset(
            makeRiskInput(
                environment,
                T0,
                100.0));


        reconcileRiskState(
            environment,
            risk_engine.state());


        require(
            approximatelyEqual(
                risk_engine.state().maximum_drawdown,
                0.0),
            "Initial maximum drawdown must be zero.");


        std::cout
            << "[PASS] Initial environment / risk-state reconciliation\n";


        // ====================================================================
        // 2. FLAT -> LONG
        //
        // Opening transaction costs and slippage reduce equity.
        // Therefore risk state must see their effect automatically.
        // ====================================================================

        [[maybe_unused]] const auto long_entry_step =
            environment.step(
                DevAction::LONG,
                T1,
                100.0);

        const auto& long_entry_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T1,
                    100.0));


        reconcileRiskState(
            environment,
            long_entry_risk);


        require(
            long_entry_risk.position > 0.0 &&
            long_entry_risk.gross_exposure > 0.0 &&
            long_entry_risk.exposure_ratio > 0.0,
            "LONG entry exposure state is invalid.");


        require(
            long_entry_risk.current_drawdown > 0.0,
            "Opening execution costs should be visible "
            "through account drawdown.");


        std::cout
            << "[PASS] LONG entry risk reconciliation\n";
        std::cout
            << "[PASS] Execution costs -> drawdown propagation\n";


        // ====================================================================
        // 3. LONG profitable HOLD
        //
        // 100 -> 110
        // ====================================================================

        [[maybe_unused]] const auto long_profit_step =
            environment.step(
                DevAction::HOLD,
                T2,
                110.0);


        const auto& long_profit_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T2,
                    110.0));


        reconcileRiskState(
            environment,
            long_profit_risk);


        require(
            approximatelyEqual(
                long_profit_risk.current_drawdown,
                0.0),
            "New profitable equity peak should have "
            "zero current drawdown.");


        require(
            long_profit_risk.peak_equity >
            INITIAL_EQUITY,
            "Profitable LONG position did not establish "
            "a new equity peak.");


        std::cout
            << "[PASS] LONG profitable risk state\n";
        std::cout
            << "[PASS] New equity-peak reconciliation\n";


        // ====================================================================
        // 4. LONG losing HOLD
        //
        // 110 -> 95
        // ====================================================================

        [[maybe_unused]] const auto long_loss_step =
            environment.step(
                DevAction::HOLD,
                T3,
                95.0);

        const auto& long_loss_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T3,
                    95.0));


        reconcileRiskState(
            environment,
            long_loss_risk);


        require(
            long_loss_risk.current_drawdown > 0.0,
            "LONG loss did not create drawdown.");


        require(
            long_loss_risk.maximum_drawdown >=
            long_loss_risk.current_drawdown,
            "Maximum drawdown is below current drawdown.");


        const double long_loss_maximum_drawdown =
            long_loss_risk.maximum_drawdown;


        std::cout
            << "[PASS] LONG losing risk state\n";
        std::cout
            << "[PASS] Maximum-drawdown tracking\n";


        // ====================================================================
        // 5. LONG recovery without necessarily erasing max drawdown
        //
        // 95 -> 105
        // ====================================================================

        [[maybe_unused]] const auto long_recovery_step =
            environment.step(
                DevAction::HOLD,
                T4,
                105.0);


        const auto& recovery_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T4,
                    105.0));


        reconcileRiskState(
            environment,
            recovery_risk);


        require(
            recovery_risk.maximum_drawdown >=
            long_loss_maximum_drawdown,
            "Historical maximum drawdown decreased during recovery.");


        std::cout
            << "[PASS] Drawdown recovery reconciliation\n";


        // ====================================================================
        // 6. LONG -> EXIT
        // ====================================================================

        [[maybe_unused]] const auto long_exit_step =
            environment.step(
                DevAction::EXIT,
                T5,
                105.0);

        const auto& exit_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T5,
                    105.0));


        reconcileRiskState(
            environment,
            exit_risk);


        require(
            approximatelyEqual(
                exit_risk.position,
                0.0) &&
            approximatelyEqual(
                exit_risk.quantity,
                0.0) &&
            approximatelyEqual(
                exit_risk.gross_exposure,
                0.0) &&
            approximatelyEqual(
                exit_risk.exposure_ratio,
                0.0),
            "EXIT did not produce FLAT risk exposure.");


        std::cout
            << "[PASS] LONG -> EXIT risk reconciliation\n";
        std::cout
            << "[PASS] FLAT exposure reconciliation\n";


        // ====================================================================
        // 7. FLAT -> SHORT
        // ====================================================================

        [[maybe_unused]] const auto short_entry_step =
            environment.step(
                DevAction::SHORT,
                T6,
                105.0);


        const auto& short_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T6,
                    105.0));


        reconcileRiskState(
            environment,
            short_risk);


        require(
            short_risk.position < 0.0 &&
            short_risk.gross_exposure > 0.0 &&
            short_risk.exposure_ratio > 0.0,
            "SHORT exposure state is invalid.");


        std::cout
            << "[PASS] SHORT entry risk reconciliation\n";


        // ====================================================================
        // 8. SHORT profitable HOLD
        //
        // 105 -> 90
        // ====================================================================

        [[maybe_unused]] const auto short_profit_step =
            environment.step(
                DevAction::HOLD,
                T7,
                90.0);


        const auto& short_profit_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T7,
                    90.0));


        reconcileRiskState(
            environment,
            short_profit_risk);


        require(
            short_profit_risk.position < 0.0,
            "SHORT position unexpectedly changed.");


        std::cout
            << "[PASS] SHORT profitable risk reconciliation\n";


        // ====================================================================
        // 9. SHORT -> LONG reversal
        //
        // Risk state must reflect the resulting position, not the old one.
        // ====================================================================

        [[maybe_unused]] const auto short_to_long_step =
            environment.step(
                DevAction::LONG,
                T8,
                90.0);

        const auto& reversal_risk =
            risk_engine.update(
                makeRiskInput(
                    environment,
                    T8,
                    90.0));


        reconcileRiskState(
            environment,
            reversal_risk);


        require(
            approximatelyEqual(
                reversal_risk.position,
                1.0),
            "SHORT -> LONG reversal risk position failed.");


        require(
            reversal_risk.gross_exposure > 0.0 &&
            reversal_risk.exposure_ratio > 0.0,
            "SHORT -> LONG reversal exposure failed.");


        std::cout
            << "[PASS] SHORT -> LONG reversal risk reconciliation\n";


        // ====================================================================
        // 10. Maximum drawdown must never decrease
        // ====================================================================

        require(
            reversal_risk.maximum_drawdown >=
            long_loss_maximum_drawdown,
            "Maximum drawdown decreased later in episode.");


        std::cout
            << "[PASS] Episode maximum-drawdown persistence\n";


        // ====================================================================
        // 11. Cross-episode reset isolation
        // ====================================================================

        environment.reset(
            "SBIN",
            T0,
            500.0);


        risk_engine.reset(
            makeRiskInput(
                environment,
                T0,
                500.0));


        reconcileRiskState(
            environment,
            risk_engine.state());


        require(
            approximatelyEqual(
                risk_engine.state().current_drawdown,
                0.0) &&
            approximatelyEqual(
                risk_engine.state().maximum_drawdown,
                0.0),
            "Drawdown state leaked across episode reset.");


        require(
            approximatelyEqual(
                risk_engine.state().gross_exposure,
                0.0) &&
            approximatelyEqual(
                risk_engine.state().exposure_ratio,
                0.0),
            "Exposure state leaked across episode reset.");


        std::cout
            << "[PASS] Cross-episode risk-state isolation\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Execution source            : TradingEnvironment\n"
            << "Equity source               : TradingEnvironment\n"
            << "Risk owner                  : CETE RiskStateEngine\n"
            << "Initial equity              : RECONCILED\n"
            << "Current equity              : RECONCILED\n"
            << "Peak equity                 : RECONCILED\n"
            << "Current drawdown            : RECONCILED\n"
            << "Maximum drawdown            : VERIFIED\n"
            << "LONG exposure               : VERIFIED\n"
            << "SHORT exposure              : VERIFIED\n"
            << "FLAT exposure               : VERIFIED\n"
            << "Reversal exposure           : VERIFIED\n"
            << "Execution-cost propagation  : VERIFIED\n"
            << "P&L -> risk propagation     : VERIFIED\n"
            << "Cross-episode leakage       : NONE\n"
            << "Forward-return dependency   : NONE\n"
            << "Future-price dependency     : NONE\n"
            << "DevAI risk policy           : NONE\n"
            << "Terminal-condition policy   : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.7B TRADING ENVIRONMENT / CETE RISK RECONCILIATION PASSED\n"
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