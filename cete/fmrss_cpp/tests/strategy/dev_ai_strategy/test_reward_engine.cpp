#include "devai/strategy/dev_ai_strategy/RewardEngine.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using devai::strategy::RewardConfig;
using devai::strategy::RewardEngine;
using devai::strategy::RewardInput;
using devai::strategy::RewardResult;


// ============================================================
// TEST UTILITIES
// ============================================================

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
    double lhs,
    double rhs,
    double tolerance = 1e-12)
{
    return
        std::abs(lhs - rhs) <= tolerance;
}


void requireEqual(
    double actual,
    double expected,
    const std::string& message)
{
    require(
        approximatelyEqual(
            actual,
            expected),
        message);
}


template <typename Function>
void requireInvalidArgument(
    Function&& function,
    const std::string& message)
{
    bool threw = false;

    try
    {
        function();
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    require(
        threw,
        message);
}


// ============================================================
// TEST 1
// DEFAULT CONFIGURATION
// ============================================================

void testDefaultConfiguration()
{
    std::cout
        << "[TEST] Default configuration..."
        << std::endl;

    RewardEngine engine;

    requireEqual(
        engine.config().turnover_penalty,
        0.00005,
        "Default turnover penalty mismatch.");

    requireEqual(
        engine.config().drawdown_penalty,
        0.10,
        "Default drawdown penalty mismatch.");
}


// ============================================================
// TEST 2
// ZERO REWARD
// ============================================================

void testZeroReward()
{
    std::cout
        << "[TEST] Zero reward..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    const RewardResult result =
        engine.calculate(input);

    requireEqual(
        result.pnl_component,
        0.0,
        "Zero P&L component failed.");

    requireEqual(
        result.turnover_component,
        0.0,
        "Zero turnover component failed.");

    requireEqual(
        result.drawdown_component,
        0.0,
        "Zero drawdown component failed.");

    requireEqual(
        result.total,
        0.0,
        "Zero total reward failed.");
}


// ============================================================
// TEST 3
// PROFITABLE STEP
// ============================================================

void testProfitableStep()
{
    std::cout
        << "[TEST] Profitable step..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.net_equity_return =
        0.002;

    input.turnover =
        0.0;

    input.previous_drawdown =
        0.0;

    input.current_drawdown =
        0.0;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.pnl_component,
        0.002,
        "Profitable P&L component mismatch.");

    requireEqual(
        result.turnover_component,
        0.0,
        "Unexpected turnover penalty.");

    requireEqual(
        result.drawdown_component,
        0.0,
        "Unexpected drawdown penalty.");

    requireEqual(
        result.total,
        0.002,
        "Profitable reward mismatch.");
}


// ============================================================
// TEST 4
// LOSING STEP
// ============================================================

void testLosingStep()
{
    std::cout
        << "[TEST] Losing step..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.net_equity_return =
        -0.002;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.pnl_component,
        -0.002,
        "Losing P&L component mismatch.");

    requireEqual(
        result.total,
        -0.002,
        "Losing reward mismatch.");
}


// ============================================================
// TEST 5
// TURNOVER PENALTY
// ============================================================

void testTurnoverPenalty()
{
    std::cout
        << "[TEST] Turnover penalty..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.turnover =
        1.0;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.turnover_component,
        -0.00005,
        "Turnover penalty mismatch.");

    requireEqual(
        result.total,
        -0.00005,
        "Turnover-only reward mismatch.");
}


// ============================================================
// TEST 6
// REVERSAL TURNOVER PENALTY
// ============================================================

void testReversalTurnoverPenalty()
{
    std::cout
        << "[TEST] Reversal turnover penalty..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    // LONG -> SHORT or SHORT -> LONG
    input.turnover =
        2.0;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.turnover_component,
        -0.00010,
        "Reversal turnover penalty mismatch.");

    requireEqual(
        result.total,
        -0.00010,
        "Reversal reward mismatch.");
}


// ============================================================
// TEST 7
// DRAWDOWN INCREASE
// ============================================================

void testDrawdownIncreasePenalty()
{
    std::cout
        << "[TEST] Drawdown-increase penalty..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.previous_drawdown =
        0.05;

    input.current_drawdown =
        0.08;

    const auto result =
        engine.calculate(input);

    // Increase = 0.03
    // Penalty  = -0.10 * 0.03 = -0.003

    requireEqual(
        result.drawdown_component,
        -0.003,
        "Drawdown-increase penalty mismatch.");

    requireEqual(
        result.total,
        -0.003,
        "Drawdown-only reward mismatch.");
}


// ============================================================
// TEST 8
// DRAWDOWN UNCHANGED
// ============================================================

void testDrawdownUnchanged()
{
    std::cout
        << "[TEST] Unchanged drawdown..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.previous_drawdown =
        0.05;

    input.current_drawdown =
        0.05;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.drawdown_component,
        0.0,
        "Unchanged drawdown must not be penalized.");
}


// ============================================================
// TEST 9
// DRAWDOWN RECOVERY
// ============================================================

void testDrawdownRecovery()
{
    std::cout
        << "[TEST] Drawdown recovery..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.previous_drawdown =
        0.10;

    input.current_drawdown =
        0.04;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.drawdown_component,
        0.0,
        "Drawdown recovery must not be penalized.");
}


// ============================================================
// TEST 10
// COMBINED REWARD
// ============================================================

void testCombinedReward()
{
    std::cout
        << "[TEST] Combined reward..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.net_equity_return =
        0.005;

    input.turnover =
        1.0;

    input.previous_drawdown =
        0.02;

    input.current_drawdown =
        0.03;

    const auto result =
        engine.calculate(input);

    // P&L:
    //   +0.005
    //
    // Turnover:
    //   -0.00005
    //
    // Drawdown increase:
    //   0.03 - 0.02 = 0.01
    //
    // Drawdown penalty:
    //   -0.10 * 0.01
    //   = -0.001
    //
    // Total:
    //   0.005 - 0.00005 - 0.001
    //   = 0.00395

    requireEqual(
        result.pnl_component,
        0.005,
        "Combined P&L component mismatch.");

    requireEqual(
        result.turnover_component,
        -0.00005,
        "Combined turnover component mismatch.");

    requireEqual(
        result.drawdown_component,
        -0.001,
        "Combined drawdown component mismatch.");

    requireEqual(
        result.total,
        0.00395,
        "Combined total reward mismatch.");
}


// ============================================================
// TEST 11
// CUSTOM CONFIGURATION
// ============================================================

void testCustomConfiguration()
{
    std::cout
        << "[TEST] Custom reward configuration..."
        << std::endl;

    RewardConfig config;

    config.turnover_penalty =
        0.001;

    config.drawdown_penalty =
        0.50;

    RewardEngine engine(
        config);

    RewardInput input;

    input.net_equity_return =
        0.01;

    input.turnover =
        2.0;

    input.previous_drawdown =
        0.02;

    input.current_drawdown =
        0.04;

    const auto result =
        engine.calculate(input);

    // P&L:
    //   +0.01
    //
    // Turnover:
    //   -0.001 * 2 = -0.002
    //
    // Drawdown:
    //   -(0.50 * 0.02) = -0.01
    //
    // Total:
    //   0.01 - 0.002 - 0.01
    //   = -0.002

    requireEqual(
        result.pnl_component,
        0.01,
        "Custom P&L component mismatch.");

    requireEqual(
        result.turnover_component,
        -0.002,
        "Custom turnover penalty mismatch.");

    requireEqual(
        result.drawdown_component,
        -0.01,
        "Custom drawdown penalty mismatch.");

    requireEqual(
        result.total,
        -0.002,
        "Custom reward mismatch.");
}


// ============================================================
// TEST 12
// DISABLED BEHAVIORAL PENALTIES
// ============================================================

void testDisabledPenalties()
{
    std::cout
        << "[TEST] Disabled behavioral penalties..."
        << std::endl;

    RewardConfig config;

    config.turnover_penalty =
        0.0;

    config.drawdown_penalty =
        0.0;

    RewardEngine engine(
        config);

    RewardInput input;

    input.net_equity_return =
        0.0075;

    input.turnover =
        2.0;

    input.previous_drawdown =
        0.0;

    input.current_drawdown =
        0.50;

    const auto result =
        engine.calculate(input);

    requireEqual(
        result.turnover_component,
        0.0,
        "Disabled turnover penalty failed.");

    requireEqual(
        result.drawdown_component,
        0.0,
        "Disabled drawdown penalty failed.");

    requireEqual(
        result.total,
        0.0075,
        "Disabled-penalty reward mismatch.");
}


// ============================================================
// TEST 13
// NEGATIVE TURNOVER REJECTION
// ============================================================

void testNegativeTurnoverRejection()
{
    std::cout
        << "[TEST] Negative turnover rejection..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.turnover =
        -1.0;

    requireInvalidArgument(
        [&]()
        {
            static_cast<void>(
                engine.calculate(input));
        },
        "Negative turnover was accepted.");
}


// ============================================================
// TEST 14
// NEGATIVE DRAWDOWN REJECTION
// ============================================================

void testNegativeDrawdownRejection()
{
    std::cout
        << "[TEST] Negative drawdown rejection..."
        << std::endl;

    RewardEngine engine;

    {
        RewardInput input;

        input.previous_drawdown =
            -0.01;

        requireInvalidArgument(
            [&]()
            {
                static_cast<void>(
                    engine.calculate(input));
            },
            "Negative previous drawdown was accepted.");
    }

    {
        RewardInput input;

        input.current_drawdown =
            -0.01;

        requireInvalidArgument(
            [&]()
            {
                static_cast<void>(
                    engine.calculate(input));
            },
            "Negative current drawdown was accepted.");
    }
}


// ============================================================
// TEST 15
// INVALID CONFIGURATION
// ============================================================

void testInvalidConfiguration()
{
    std::cout
        << "[TEST] Invalid configuration rejection..."
        << std::endl;

    {
        RewardConfig config;

        config.turnover_penalty =
            -0.01;

        requireInvalidArgument(
            [&]()
            {
                RewardEngine engine(config);
                static_cast<void>(engine);
            },
            "Negative turnover penalty was accepted.");
    }

    {
        RewardConfig config;

        config.drawdown_penalty =
            -0.01;

        requireInvalidArgument(
            [&]()
            {
                RewardEngine engine(config);
                static_cast<void>(engine);
            },
            "Negative drawdown penalty was accepted.");
    }
}


// ============================================================
// TEST 16
// NON-FINITE INPUT REJECTION
// ============================================================

void testNonFiniteInputRejection()
{
    std::cout
        << "[TEST] Non-finite input rejection..."
        << std::endl;

    RewardEngine engine;

    const double nan =
        std::numeric_limits<double>::quiet_NaN();

    const double infinity =
        std::numeric_limits<double>::infinity();


    // Net equity return
    {
        RewardInput input;

        input.net_equity_return =
            nan;

        requireInvalidArgument(
            [&]()
            {
                static_cast<void>(
                    engine.calculate(input));
            },
            "NaN net equity return was accepted.");
    }


    // Turnover
    {
        RewardInput input;

        input.turnover =
            infinity;

        requireInvalidArgument(
            [&]()
            {
                static_cast<void>(
                    engine.calculate(input));
            },
            "Infinite turnover was accepted.");
    }


    // Previous drawdown
    {
        RewardInput input;

        input.previous_drawdown =
            nan;

        requireInvalidArgument(
            [&]()
            {
                static_cast<void>(
                    engine.calculate(input));
            },
            "NaN previous drawdown was accepted.");
    }


    // Current drawdown
    {
        RewardInput input;

        input.current_drawdown =
            infinity;

        requireInvalidArgument(
            [&]()
            {
                static_cast<void>(
                    engine.calculate(input));
            },
            "Infinite current drawdown was accepted.");
    }
}


// ============================================================
// TEST 17
// NON-FINITE CONFIGURATION REJECTION
// ============================================================

void testNonFiniteConfigurationRejection()
{
    std::cout
        << "[TEST] Non-finite configuration rejection..."
        << std::endl;

    const double nan =
        std::numeric_limits<double>::quiet_NaN();

    const double infinity =
        std::numeric_limits<double>::infinity();


    {
        RewardConfig config;

        config.turnover_penalty =
            nan;

        requireInvalidArgument(
            [&]()
            {
                RewardEngine engine(config);
                static_cast<void>(engine);
            },
            "NaN turnover penalty was accepted.");
    }


    {
        RewardConfig config;

        config.drawdown_penalty =
            infinity;

        requireInvalidArgument(
            [&]()
            {
                RewardEngine engine(config);
                static_cast<void>(engine);
            },
            "Infinite drawdown penalty was accepted.");
    }
}


// ============================================================
// TEST 18
// DETERMINISM
// ============================================================

void testDeterminism()
{
    std::cout
        << "[TEST] Deterministic reward calculation..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.net_equity_return =
        0.00325;

    input.turnover =
        1.0;

    input.previous_drawdown =
        0.015;

    input.current_drawdown =
        0.021;

    const auto first =
        engine.calculate(input);

    const auto second =
        engine.calculate(input);

    requireEqual(
        first.pnl_component,
        second.pnl_component,
        "P&L component is not deterministic.");

    requireEqual(
        first.turnover_component,
        second.turnover_component,
        "Turnover component is not deterministic.");

    requireEqual(
        first.drawdown_component,
        second.drawdown_component,
        "Drawdown component is not deterministic.");

    requireEqual(
        first.total,
        second.total,
        "Total reward is not deterministic.");
}


// ============================================================
// TEST 19
// COMPONENT SUM INTEGRITY
// ============================================================

void testComponentSumIntegrity()
{
    std::cout
        << "[TEST] Reward component-sum integrity..."
        << std::endl;

    RewardEngine engine;

    RewardInput input;

    input.net_equity_return =
        -0.0015;

    input.turnover =
        2.0;

    input.previous_drawdown =
        0.04;

    input.current_drawdown =
        0.055;

    const auto result =
        engine.calculate(input);

    const double expected =
        result.pnl_component +
        result.turnover_component +
        result.drawdown_component;

    requireEqual(
        result.total,
        expected,
        "Reward total does not equal component sum.");
}

} // namespace


// ============================================================
// MAIN
// ============================================================

int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.6 — REWARD ENGINE VALIDATION\n"
            << "============================================================\n"
            << "Agent                       : Dev\n"
            << "Reward Basis                : Net Equity Return\n"
            << "Turnover Penalty            : ENABLED / CONFIGURABLE\n"
            << "Drawdown-Increase Penalty   : ENABLED / CONFIGURABLE\n"
            << "Brokerage Calculation       : NOT IN RewardEngine\n"
            << "Slippage Calculation        : NOT IN RewardEngine\n"
            << "Future Return Input         : NONE\n"
            << "============================================================\n\n";

        testDefaultConfiguration();
        testZeroReward();

        testProfitableStep();
        testLosingStep();

        testTurnoverPenalty();
        testReversalTurnoverPenalty();

        testDrawdownIncreasePenalty();
        testDrawdownUnchanged();
        testDrawdownRecovery();

        testCombinedReward();
        testCustomConfiguration();
        testDisabledPenalties();

        testNegativeTurnoverRejection();
        testNegativeDrawdownRejection();
        testInvalidConfiguration();

        testNonFiniteInputRejection();
        testNonFiniteConfigurationRejection();

        testDeterminism();
        testComponentSumIntegrity();

        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.6 VALIDATION SUMMARY\n"
            << "============================================================\n"
            << "Default Configuration             : PASSED\n"
            << "Zero Reward                       : PASSED\n"
            << "Profitable-Step Reward            : PASSED\n"
            << "Losing-Step Reward                : PASSED\n"
            << "Turnover Penalty                  : PASSED\n"
            << "Reversal Turnover Penalty         : PASSED\n"
            << "Drawdown-Increase Penalty         : PASSED\n"
            << "Drawdown Recovery                 : PASSED\n"
            << "Custom Configuration              : PASSED\n"
            << "Disabled Penalties                : PASSED\n"
            << "Negative-Input Rejection          : PASSED\n"
            << "Non-Finite Rejection              : PASSED\n"
            << "Deterministic Reward              : PASSED\n"
            << "Component-Sum Integrity           : PASSED\n"
            << "Brokerage Double Counting         : NONE\n"
            << "Future/Forward Return Input       : NONE\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.6 REWARD ENGINE VALIDATION PASSED\n"
            << "============================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.6 REWARD ENGINE VALIDATION FAILED\n"
            << "============================================================\n"
            << "Reason: "
            << exception.what()
            << "\n"
            << "============================================================\n";

        return 1;
    }
}