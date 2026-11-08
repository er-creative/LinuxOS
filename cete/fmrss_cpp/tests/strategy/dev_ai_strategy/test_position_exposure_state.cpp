#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "devai/strategy/dev_ai_strategy/PositionExposureState.hpp"

namespace
{

using namespace devai::strategy;


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
    double tolerance = 1.0e-10)
{
    return
        std::fabs(left - right) <=
        tolerance;
}


template<typename Function>
bool throwsException(Function&& function)
{
    try
    {
        function();
    }
    catch (const std::exception&)
    {
        return true;
    }

    return false;
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
        << "DEVAI PHASE 2.3 — POSITION / EXPOSURE STATE VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        TradingEnvironmentConfig config;

        config.initial_equity =
            100000.0;

        config.exposure_fraction =
            1.0;

        // Phase 2.3 validates position/exposure behavior.
        //
        // We deliberately remove slippage here so price execution effects
        // cannot obscure the position-state contract. Brokerage remains owned
        // by TradingEnvironment and is tested separately by its frozen tests.
        config.slippage_rate =
            0.0;


        PositionExposureState position_state(
            config);


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


        // ====================================================================
        // 2.3.1 Reset → FLAT
        // ====================================================================

        position_state.reset(
            "RELIANCE",
            T0,
            100.0);


        require(
            position_state.initialized(),
            "Position state was not initialized.");

        require(
            position_state.isFlat(),
            "Episode did not begin FLAT.");

        require(
            approximatelyEqual(
                position_state.position(),
                0.0),
            "Initial position is not zero.");

        require(
            approximatelyEqual(
                position_state.quantity(),
                0.0),
            "Initial quantity is not zero.");

        require(
            approximatelyEqual(
                position_state.entryPrice(),
                0.0),
            "Initial entry price is not zero.");

        std::cout
            << "[PASS] Episode reset / FLAT initialization\n";


        // ====================================================================
        // 2.3.2 FLAT → LONG
        // ====================================================================

        const auto long_entry =
            position_state.step(
                DevAction::LONG,
                T1,
                100.0);


        require(
            position_state.isLong(),
            "LONG action did not create LONG position.");

        require(
            approximatelyEqual(
                position_state.position(),
                1.0),
            "LONG position must equal +1.");

        require(
            position_state.quantity() > 0.0,
            "LONG position has no quantity.");

        require(
            position_state.entryPrice() > 0.0,
            "LONG position has invalid entry price.");

        require(
            approximatelyEqual(
                long_entry.target_position,
                1.0),
            "LONG target position mismatch.");

        std::cout
            << "[PASS] FLAT -> LONG exposure\n";


        // ====================================================================
        // 2.3.3 LONG → HOLD
        // ====================================================================

        const double long_quantity =
            position_state.quantity();

        const double long_entry_price =
            position_state.entryPrice();


        const auto long_hold =
            position_state.step(
                DevAction::HOLD,
                T2,
                101.0);


        require(
            position_state.isLong(),
            "HOLD changed LONG position.");

        require(
            approximatelyEqual(
                position_state.quantity(),
                long_quantity),
            "HOLD changed LONG quantity.");

        require(
            approximatelyEqual(
                position_state.entryPrice(),
                long_entry_price),
            "HOLD changed LONG entry price.");

        require(
            approximatelyEqual(
                long_hold.target_position,
                1.0),
            "LONG HOLD target mismatch.");

        require(
            position_state.holdingBars() > 0,
            "Holding period did not advance.");

        std::cout
            << "[PASS] LONG HOLD persistence\n";


        // ====================================================================
        // 2.3.4 LONG → SHORT direct reversal
        // ====================================================================

        const auto short_reversal =
            position_state.step(
                DevAction::SHORT,
                T3,
                102.0);


        require(
            position_state.isShort(),
            "LONG -> SHORT reversal failed.");

        require(
            approximatelyEqual(
                position_state.position(),
                -1.0),
            "SHORT position must equal -1.");

        require(
            position_state.quantity() > 0.0,
            "SHORT reversal produced zero quantity.");

        require(
            position_state.entryPrice() > 0.0,
            "SHORT reversal produced invalid entry price.");

        require(
            approximatelyEqual(
                short_reversal.previous_position,
                1.0),
            "Reversal previous position mismatch.");

        require(
            approximatelyEqual(
                short_reversal.target_position,
                -1.0),
            "Reversal target position mismatch.");

        std::cout
            << "[PASS] LONG -> SHORT direct reversal\n";


        // ====================================================================
        // 2.3.5 SHORT → HOLD
        // ====================================================================

        const double short_quantity =
            position_state.quantity();

        const double short_entry_price =
            position_state.entryPrice();


        position_state.step(
            DevAction::HOLD,
            T4,
            101.0);


        require(
            position_state.isShort(),
            "SHORT HOLD changed position.");

        require(
            approximatelyEqual(
                position_state.quantity(),
                short_quantity),
            "SHORT HOLD changed quantity.");

        require(
            approximatelyEqual(
                position_state.entryPrice(),
                short_entry_price),
            "SHORT HOLD changed entry price.");

        std::cout
            << "[PASS] SHORT HOLD persistence\n";


        // ====================================================================
        // 2.3.6 SHORT → EXIT
        // ====================================================================

        const auto exit_result =
            position_state.step(
                DevAction::EXIT,
                T5,
                100.0);


        require(
            position_state.isFlat(),
            "EXIT did not flatten SHORT position.");

        require(
            approximatelyEqual(
                position_state.position(),
                0.0),
            "EXIT position is not zero.");

        require(
            approximatelyEqual(
                position_state.quantity(),
                0.0),
            "EXIT left residual quantity.");

        require(
            approximatelyEqual(
                position_state.entryPrice(),
                0.0),
            "EXIT left residual entry price.");

        require(
            approximatelyEqual(
                exit_result.target_position,
                0.0),
            "EXIT target position mismatch.");

        std::cout
            << "[PASS] SHORT -> EXIT / FLAT\n";


        // ====================================================================
        // 2.3.7 FLAT HOLD
        // ====================================================================

        position_state.step(
            DevAction::HOLD,
            T6,
            100.0);


        require(
            position_state.isFlat(),
            "FLAT HOLD created exposure.");

        require(
            approximatelyEqual(
                position_state.quantity(),
                0.0),
            "FLAT HOLD created quantity.");

        std::cout
            << "[PASS] FLAT HOLD preserves zero exposure\n";


        // ====================================================================
        // 2.3.8 Portfolio-state integration
        // ====================================================================

        const DevPortfolioState portfolio =
            position_state.portfolioState();


        require(
            approximatelyEqual(
                portfolio.position,
                0.0),
            "Portfolio position disagrees with exposure state.");

        std::cout
            << "[PASS] DevPortfolioState integration\n";


        // ====================================================================
        // 2.3.9 Exposure snapshot
        // ====================================================================

        const PositionExposureSnapshot snapshot =
            position_state.snapshot();


        require(
            snapshot.initialized,
            "Exposure snapshot not initialized.");

        require(
            snapshot.symbol ==
                "RELIANCE",
            "Exposure snapshot symbol mismatch.");

        require(
            snapshot.decision_time ==
                T6,
            "Exposure snapshot decision time mismatch.");

        require(
            snapshot.flat,
            "Exposure snapshot should be FLAT.");

        require(
            !snapshot.long_position &&
            !snapshot.short_position,
            "FLAT snapshot contains directional exposure.");

        require(
            approximatelyEqual(
                snapshot.exposure_fraction,
                1.0),
            "Exposure fraction mismatch.");

        std::cout
            << "[PASS] Position/exposure snapshot generation\n";


        // ====================================================================
        // 2.3.10 Decision-time protection
        // ====================================================================

        require(
            throwsException(
                [&position_state]()
                {
                    position_state.step(
                        DevAction::LONG,
                        T6,
                        100.0);
                }),
            "Duplicate decision time was accepted.");

        require(
            throwsException(
                [&position_state]()
                {
                    position_state.step(
                        DevAction::LONG,
                        T5,
                        100.0);
                }),
            "Backward decision time was accepted.");

        std::cout
            << "[PASS] Decision-time progression protection\n";


        // ====================================================================
        // 2.3.11 Episode reset isolation
        // ====================================================================

        position_state.reset(
            "SBIN",
            T0,
            200.0);


        require(
            position_state.isFlat(),
            "Reset did not clear previous position.");

        require(
            position_state.symbol() ==
                "SBIN",
            "Reset did not replace episode symbol.");

        require(
            approximatelyEqual(
                position_state.quantity(),
                0.0),
            "Reset retained previous quantity.");

        require(
            approximatelyEqual(
                position_state.entryPrice(),
                0.0),
            "Reset retained previous entry price.");

        require(
            position_state.holdingBars() == 0,
            "Reset retained holding period.");

        std::cout
            << "[PASS] Cross-episode position isolation\n";


        // ====================================================================
        // 2.3.12 Step-before-reset protection
        // ====================================================================

        PositionExposureState
            uninitialized(config);


        require(
            throwsException(
                [&uninitialized]()
                {
                    uninitialized.step(
                        DevAction::LONG,
                        1000060,
                        100.0);
                }),
            "Step before reset was accepted.");

        std::cout
            << "[PASS] Uninitialized-step protection\n";


        // ====================================================================
        // Final
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Position source of truth   : TradingEnvironment\n"
            << "LONG exposure              : +1 VERIFIED\n"
            << "FLAT exposure              :  0 VERIFIED\n"
            << "SHORT exposure             : -1 VERIFIED\n"
            << "Direct reversal            : VERIFIED\n"
            << "HOLD persistence           : VERIFIED\n"
            << "EXIT flattening            : VERIFIED\n"
            << "Quantity ownership         : TradingEnvironment\n"
            << "Entry-price ownership      : TradingEnvironment\n"
            << "Portfolio-state integration: VERIFIED\n"
            << "Decision-time protection   : VERIFIED\n"
            << "Cross-episode isolation    : VERIFIED\n"
            << "Duplicate position engine  : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.3 POSITION / EXPOSURE STATE VALIDATION PASSED\n"
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