#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "devai/strategy/dev_ai_strategy/TerminalConditionEngine.hpp"


namespace
{

using namespace devai::strategy;


void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
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


TerminalConditionInput makeInput(
    std::int64_t decision_time,
    std::size_t completed_steps,
    bool has_next_state,
    double initial_equity,
    double current_equity,
    double current_drawdown,
    double maximum_drawdown)
{
    TerminalConditionInput input;

    input.decision_time =
        decision_time;

    input.completed_steps =
        completed_steps;

    input.has_next_state =
        has_next_state;

    input.initial_equity =
        initial_equity;

    input.current_equity =
        current_equity;

    input.current_drawdown =
        current_drawdown;

    input.maximum_drawdown =
        maximum_drawdown;

    return input;
}

} // namespace


int main()
{
    std::cout
        << "\n"
        << "==========================================================================\n"
        << "DEVAI PHASE 2.8 — TERMINAL CONDITIONS VALIDATION\n"
        << "==========================================================================\n";


    try
    {
        // ====================================================================
        // Configuration used by the validation.
        //
        // These values exist to test policy mechanics.
        // They are NOT declared optimal trading parameters.
        // ====================================================================

        TerminalConditionConfig config;

        config.maximum_episode_steps =
            100;

        config.maximum_drawdown =
            0.10;

        config.minimum_equity_fraction =
            0.50;


        TerminalConditionEngine engine(
            config);


        // ====================================================================
        // 1. Normal continuation
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1000,
                        25,
                        true,
                        100000.0,
                        105000.0,
                        0.02,
                        0.05));


            require(
                !result.terminal,
                "Normal episode state terminated unexpectedly.");


            require(
                result.reason ==
                    TerminalReason::NONE,
                "Normal continuation returned wrong reason.");


            require(
                result.numerical_integrity,
                "Normal continuation numerical integrity failed.");


            std::cout
                << "[PASS] Normal episode continuation\n";
        }


        // ====================================================================
        // 2. End of data
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1060,
                        25,
                        false,
                        100000.0,
                        105000.0,
                        0.02,
                        0.05));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::END_OF_DATA,
                "END_OF_DATA condition failed.");


            std::cout
                << "[PASS] End-of-data termination\n";
        }


        // ====================================================================
        // 3. Maximum drawdown below threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1120,
                        50,
                        true,
                        100000.0,
                        91000.0,
                        0.09,
                        0.09));


            require(
                !result.terminal,
                "Drawdown below threshold terminated unexpectedly.");


            std::cout
                << "[PASS] Drawdown below threshold continues\n";
        }


        // ====================================================================
        // 4. Maximum drawdown exactly at threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1180,
                        50,
                        true,
                        100000.0,
                        90000.0,
                        0.10,
                        0.10));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MAXIMUM_DRAWDOWN,
                "Exact drawdown threshold did not terminate.");


            std::cout
                << "[PASS] Exact maximum-drawdown threshold termination\n";
        }


        // ====================================================================
        // 5. Maximum drawdown above threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1240,
                        50,
                        true,
                        100000.0,
                        88000.0,
                        0.12,
                        0.12));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MAXIMUM_DRAWDOWN,
                "Drawdown above threshold did not terminate.");


            std::cout
                << "[PASS] Maximum-drawdown breach termination\n";
        }


        // ====================================================================
        // 6. Minimum equity above threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1300,
                        50,
                        true,
                        100000.0,
                        51000.0,
                        0.05,
                        0.05));


            require(
                !result.terminal,
                "Equity above minimum threshold terminated unexpectedly.");


            std::cout
                << "[PASS] Equity above minimum continues\n";
        }


        // ====================================================================
        // 7. Minimum equity exactly at threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1360,
                        50,
                        true,
                        100000.0,
                        50000.0,
                        0.05,
                        0.05));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MINIMUM_EQUITY,
                "Exact minimum-equity threshold did not terminate.");


            std::cout
                << "[PASS] Exact minimum-equity threshold termination\n";
        }


        // ====================================================================
        // 8. Minimum equity below threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1420,
                        50,
                        true,
                        100000.0,
                        49000.0,
                        0.05,
                        0.05));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MINIMUM_EQUITY,
                "Minimum-equity breach did not terminate.");


            std::cout
                << "[PASS] Minimum-equity breach termination\n";
        }


        // ====================================================================
        // 9. Maximum steps below threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1480,
                        99,
                        true,
                        100000.0,
                        100000.0,
                        0.0,
                        0.0));


            require(
                !result.terminal,
                "Episode terminated before maximum step count.");


            std::cout
                << "[PASS] Step count below limit continues\n";
        }


        // ====================================================================
        // 10. Maximum steps exactly at threshold
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1540,
                        100,
                        true,
                        100000.0,
                        100000.0,
                        0.0,
                        0.0));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MAXIMUM_STEPS,
                "Maximum-step threshold did not terminate.");


            std::cout
                << "[PASS] Maximum-step termination\n";
        }


        // ====================================================================
        // 11. END_OF_DATA priority
        //
        // All conditions are true, but END_OF_DATA has highest reporting
        // priority.
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1600,
                        100,
                        false,
                        100000.0,
                        40000.0,
                        0.20,
                        0.20));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::END_OF_DATA,
                "END_OF_DATA priority failed.");


            std::cout
                << "[PASS] END_OF_DATA priority\n";
        }


        // ====================================================================
        // 12. Drawdown priority over minimum equity / max steps
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1660,
                        100,
                        true,
                        100000.0,
                        40000.0,
                        0.20,
                        0.20));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MAXIMUM_DRAWDOWN,
                "Maximum-drawdown priority failed.");


            std::cout
                << "[PASS] Maximum-drawdown reporting priority\n";
        }


        // ====================================================================
        // 13. Minimum-equity priority over maximum steps
        // ====================================================================

        {
            const auto result =
                engine.evaluate(
                    makeInput(
                        1720,
                        100,
                        true,
                        100000.0,
                        50000.0,
                        0.05,
                        0.05));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::MINIMUM_EQUITY,
                "Minimum-equity priority failed.");


            std::cout
                << "[PASS] Minimum-equity reporting priority\n";
        }


        // ====================================================================
        // 14. Disabled optional policies
        //
        // All zeros disable configurable policy thresholds.
        // End-of-data remains fundamental.
        // ====================================================================

        {
            TerminalConditionEngine disabled_engine;


            const auto result =
                disabled_engine.evaluate(
                    makeInput(
                        1780,
                        1000000,
                        true,
                        100000.0,
                        1000.0,
                        0.90,
                        0.90));


            require(
                !result.terminal &&
                result.reason ==
                    TerminalReason::NONE,
                "Disabled terminal policies were applied.");


            std::cout
                << "[PASS] Optional-policy disabling\n";
        }


        // ====================================================================
        // 15. END_OF_DATA cannot be disabled
        // ====================================================================

        {
            TerminalConditionEngine disabled_engine;


            const auto result =
                disabled_engine.evaluate(
                    makeInput(
                        1840,
                        0,
                        false,
                        100000.0,
                        100000.0,
                        0.0,
                        0.0));


            require(
                result.terminal &&
                result.reason ==
                    TerminalReason::END_OF_DATA,
                "END_OF_DATA was incorrectly disabled.");


            std::cout
                << "[PASS] Fundamental end-of-data termination\n";
        }


        // ====================================================================
        // 16. Invalid configuration rejection
        // ====================================================================

        requireThrows(
            []
            {
                TerminalConditionConfig invalid;

                invalid.maximum_drawdown =
                    1.0;

                TerminalConditionEngine local(
                    invalid);
            },
            "Invalid maximum_drawdown configuration was accepted.");


        requireThrows(
            []
            {
                TerminalConditionConfig invalid;

                invalid.minimum_equity_fraction =
                    1.1;

                TerminalConditionEngine local(
                    invalid);
            },
            "Invalid minimum_equity_fraction configuration was accepted.");


        std::cout
            << "[PASS] Invalid-configuration rejection\n";


        // ====================================================================
        // 17. Invalid decision time rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
               [[maybe_unused]] const auto result =
                    engine.evaluate(
                        makeInput(
                            0,
                            1,
                            true,
                            100000.0,
                            100000.0,
                            0.0,
                            0.0));
                },
                "Invalid decision time was accepted.");


        std::cout
            << "[PASS] Invalid decision-time rejection\n";


        // ====================================================================
        // 18. Invalid equity rejection
        // ====================================================================



        requireThrows(
            [&engine]()
            {
                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        makeInput(
                            1900,
                            1,
                            true,
                            100000.0,
                            0.0,
                            0.0,
                            0.0));
                },
                "Invalid current equity was accepted.");


        std::cout
            << "[PASS] Invalid-equity rejection\n";


        // ====================================================================
        // 19. Invalid drawdown relationship rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        makeInput(
                            1960,
                            1,
                            true,
                            100000.0,
                            90000.0,
                            0.10,
                            0.05));
                },
                "maximum_drawdown < current_drawdown was accepted.");


        std::cout
            << "[PASS] Drawdown-integrity rejection\n";


        // ====================================================================
        // 20. Non-finite input rejection
        // ====================================================================

        requireThrows(
            [&engine]()
            {
                [[maybe_unused]] const auto result =
                    engine.evaluate(
                        makeInput(
                            2020,
                            1,
                            true,
                            100000.0,
                            std::numeric_limits<double>::quiet_NaN(),
                            0.0,
                            0.0));
                },
                "Non-finite input was accepted.");


        std::cout
            << "[PASS] Non-finite input rejection\n";


        // ====================================================================
        // 21. Determinism
        // ====================================================================

        {
            const auto input =
                makeInput(
                    2080,
                    75,
                    true,
                    100000.0,
                    92000.0,
                    0.08,
                    0.09);


            const auto first =
                engine.evaluate(input);

            const auto second =
                engine.evaluate(input);


            require(
                first.terminal ==
                    second.terminal &&
                first.reason ==
                    second.reason &&
                first.decision_time ==
                    second.decision_time &&
                first.completed_steps ==
                    second.completed_steps &&
                first.current_equity ==
                    second.current_equity &&
                first.current_drawdown ==
                    second.current_drawdown &&
                first.numerical_integrity ==
                    second.numerical_integrity,
                "TerminalConditionEngine is not deterministic.");


            std::cout
                << "[PASS] Deterministic terminal evaluation\n";
        }


        // ====================================================================
        // 22. Reason-name mapping
        // ====================================================================

        require(
            std::string(
                TerminalConditionEngine::reasonName(
                    TerminalReason::NONE)) ==
                "NONE",
            "NONE reason-name mapping failed.");


        require(
            std::string(
                TerminalConditionEngine::reasonName(
                    TerminalReason::END_OF_DATA)) ==
                "END_OF_DATA",
            "END_OF_DATA reason-name mapping failed.");


        require(
            std::string(
                TerminalConditionEngine::reasonName(
                    TerminalReason::MAXIMUM_STEPS)) ==
                "MAXIMUM_STEPS",
            "MAXIMUM_STEPS reason-name mapping failed.");


        require(
            std::string(
                TerminalConditionEngine::reasonName(
                    TerminalReason::MAXIMUM_DRAWDOWN)) ==
                "MAXIMUM_DRAWDOWN",
            "MAXIMUM_DRAWDOWN reason-name mapping failed.");


        require(
            std::string(
                TerminalConditionEngine::reasonName(
                    TerminalReason::MINIMUM_EQUITY)) ==
                "MINIMUM_EQUITY",
            "MINIMUM_EQUITY reason-name mapping failed.");


        std::cout
            << "[PASS] Terminal-reason mapping\n";


        // ====================================================================
        // Final report
        // ====================================================================

        std::cout
            << "\n"
            << "--------------------------------------------------------------------------\n"
            << "Component owner            : DevAI Strategy\n"
            << "END_OF_DATA                : VERIFIED\n"
            << "MAXIMUM_STEPS              : VERIFIED\n"
            << "MAXIMUM_DRAWDOWN           : VERIFIED\n"
            << "MINIMUM_EQUITY             : VERIFIED\n"
            << "Threshold boundaries       : VERIFIED\n"
            << "Reporting priority         : VERIFIED\n"
            << "Optional policy disabling  : VERIFIED\n"
            << "Invalid input protection   : VERIFIED\n"
            << "Numerical integrity        : VERIFIED\n"
            << "Determinism                : VERIFIED\n"
            << "P&L calculation            : NONE\n"
            << "Drawdown calculation       : NONE\n"
            << "Brokerage calculation      : NONE\n"
            << "Slippage calculation       : NONE\n"
            << "Reward calculation         : NONE\n"
            << "Forced trade execution     : NONE\n"
            << "ForwardReturn_N dependency : NONE\n"
            << "Future-price dependency    : NONE\n"
            << "--------------------------------------------------------------------------\n"
            << "DEVAI PHASE 2.8 TERMINAL CONDITIONS VALIDATION PASSED\n"
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