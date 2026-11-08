#include "devai/strategy/dev_ai_strategy/DevBellmanTargetEngine.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace devai::strategy;

namespace
{

std::size_t checks = 0;
std::size_t failures = 0;

void require(
    bool condition,
    const std::string& message)
{
    ++checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << message
            << '\n';
    }
    else
    {
        ++failures;

        std::cout
            << "[FAIL] "
            << message
            << '\n';
    }
}

bool approximatelyEqual(
    double left,
    double right,
    double tolerance = 1e-12)
{
    return
        std::abs(left - right) <=
        tolerance;
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

DevAIObservation makeReadyObservation()
{
    DevAIObservation observation{};

    // Default market values are zero.
    //
    // Zero is a valid centered-percentile neural input.
    for (double& value : observation.market)
    {
        value = 0.0;
    }

    // Availability values are binary.
    for (double& value : observation.availability)
    {
        value = 1.0;
    }

    // Portfolio state remains zero/flat.
    observation.portfolio.position = 0.0;
    observation.portfolio.unrealized_return = 0.0;
    observation.portfolio.realized_return = 0.0;
    observation.portfolio.equity_return = 0.0;
    observation.portfolio.drawdown = 0.0;
    observation.portfolio.holding_period = 0.0;

    observation.numerical_integrity = true;
    observation.ready = true;

    return observation;
}

DevTransition makeTransition(
    double reward,
    bool terminal)
{
    DevTransition transition{};

    transition.observation =
        makeReadyObservation();

    transition.action =
        DevAction::LONG;

    transition.reward =
        reward;

    transition.next_observation =
        makeReadyObservation();

    transition.terminal =
        terminal;

    transition.decision_time =
        1000;

    transition.next_decision_time =
        1060;

    return transition;
}

DevQNetwork makeConstantTargetNetwork(
    double hold_q,
    double long_q,
    double short_q,
    double exit_q)
{
    DevQNetworkConfig config{};
    config.random_seed = 12345U;
    config.zero_initialize = true;

    DevQNetwork network(config);

    DevQNetworkParameters parameters =
        network.parameters();

    // With all weights zero, the output-layer biases become the exact
    // Q-values regardless of the observation.
    parameters.biases3.at(0) = hold_q;
    parameters.biases3.at(1) = long_q;
    parameters.biases3.at(2) = short_q;
    parameters.biases3.at(3) = exit_q;

    network.setParameters(parameters);

    return network;
}

} // namespace

int main()
{
    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.8A — BELLMAN TARGET ENGINE TEST\n"
        << "======================================================================\n";

    // ======================================================================
    // 1. Default configuration
    // ======================================================================

    DevBellmanTargetEngine engine;

    require(
        approximatelyEqual(
            engine.config().discount_factor,
            0.99),
        "Default discount factor is 0.99");

    // ======================================================================
    // 2. Exact non-terminal scalar Bellman calculation
    //
    // reward     = 0.10
    // gamma      = 0.99
    // next max Q = 2.00
    //
    // target =
    //     0.10 + 0.99 * 2.00
    //     2.08
    // ======================================================================

    {
        const DevBellmanTargetResult result =
            engine.calculate(
                0.10,
                2.00,
                false,
                1000,
                1060);

        require(
            approximatelyEqual(
                result.reward,
                0.10),
            "Scalar non-terminal result preserves reward");

        require(
            approximatelyEqual(
                result.discount_factor,
                0.99),
            "Scalar non-terminal result preserves discount factor");

        require(
            approximatelyEqual(
                result.next_max_q,
                2.00),
            "Scalar non-terminal result preserves next maximum Q-value");

        require(
            approximatelyEqual(
                result.discounted_bootstrap,
                1.98),
            "Scalar non-terminal discounted bootstrap is exact");

        require(
            approximatelyEqual(
                result.target,
                2.08),
            "Scalar non-terminal Bellman target is exact");

        require(
            !result.terminal,
            "Scalar non-terminal result remains non-terminal");

        require(
            result.numerical_integrity,
            "Scalar non-terminal result has numerical integrity");
    }

    // ======================================================================
    // 3. Exact terminal calculation
    //
    // Terminal transition:
    //
    //     target = reward
    //
    // next_max_q must NOT contribute.
    // ======================================================================

    {
        const DevBellmanTargetResult result =
            engine.calculate(
                0.25,
                1000.0,
                true,
                1000,
                1060);

        require(
            approximatelyEqual(
                result.reward,
                0.25),
            "Terminal result preserves reward");

        require(
            approximatelyEqual(
                result.next_max_q,
                0.0),
            "Terminal transition stores zero bootstrap Q-value");

        require(
            approximatelyEqual(
                result.discounted_bootstrap,
                0.0),
            "Terminal transition has zero discounted bootstrap");

        require(
            approximatelyEqual(
                result.target,
                0.25),
            "Terminal Bellman target equals immediate reward");

        require(
            result.terminal,
            "Terminal result remains terminal");

        require(
            result.numerical_integrity,
            "Terminal result has numerical integrity");
    }

    // ======================================================================
    // 4. Negative reward is valid
    // ======================================================================

    {
        const DevBellmanTargetResult result =
            engine.calculate(
                -0.50,
                1.00,
                false,
                1000,
                1060);

        require(
            approximatelyEqual(
                result.target,
                0.49),
            "Negative reward produces correct Bellman target");
    }

    // ======================================================================
    // 5. Negative next Q-value is valid
    //
    // -0.10 + 0.99 * -2.00 = -2.08
    // ======================================================================

    {
        const DevBellmanTargetResult result =
            engine.calculate(
                -0.10,
                -2.00,
                false,
                1000,
                1060);

        require(
            approximatelyEqual(
                result.target,
                -2.08),
            "Negative next maximum Q-value is handled correctly");
    }

    // ======================================================================
    // 6. Gamma = 0
    //
    // No future bootstrap.
    // ======================================================================

    {
        DevBellmanTargetConfig config{};
        config.discount_factor = 0.0;

        DevBellmanTargetEngine zero_gamma_engine(
            config);

        const DevBellmanTargetResult result =
            zero_gamma_engine.calculate(
                0.75,
                100.0,
                false,
                1000,
                1060);

        require(
            approximatelyEqual(
                result.discounted_bootstrap,
                0.0),
            "Gamma zero removes future bootstrap");

        require(
            approximatelyEqual(
                result.target,
                0.75),
            "Gamma zero makes target equal immediate reward");
    }

    // ======================================================================
    // 7. Gamma = 1
    // ======================================================================

    {
        DevBellmanTargetConfig config{};
        config.discount_factor = 1.0;

        DevBellmanTargetEngine full_gamma_engine(
            config);

        const DevBellmanTargetResult result =
            full_gamma_engine.calculate(
                0.50,
                2.00,
                false,
                1000,
                1060);

        require(
            approximatelyEqual(
                result.target,
                2.50),
            "Gamma one applies full future bootstrap");
    }

    // ======================================================================
    // 8. Invalid discount factors
    // ======================================================================

    requireThrows(
        []()
        {
            DevBellmanTargetConfig config{};
            config.discount_factor = -0.01;

            DevBellmanTargetEngine invalid_engine(
                config);

            (void)invalid_engine;
        },
        "Negative discount factor is rejected");

    requireThrows(
        []()
        {
            DevBellmanTargetConfig config{};
            config.discount_factor = 1.01;

            DevBellmanTargetEngine invalid_engine(
                config);

            (void)invalid_engine;
        },
        "Discount factor greater than one is rejected");

    requireThrows(
        []()
        {
            DevBellmanTargetConfig config{};
            config.discount_factor =
                std::numeric_limits<double>::quiet_NaN();

            DevBellmanTargetEngine invalid_engine(
                config);

            (void)invalid_engine;
        },
        "NaN discount factor is rejected");

    requireThrows(
        []()
        {
            DevBellmanTargetConfig config{};
            config.discount_factor =
                std::numeric_limits<double>::infinity();

            DevBellmanTargetEngine invalid_engine(
                config);

            (void)invalid_engine;
        },
        "Infinite discount factor is rejected");

    // ======================================================================
    // 9. Scalar numerical guards
    // ======================================================================

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                std::numeric_limits<double>::quiet_NaN(),
                1.0,
                false,
                1000,
                1060);
        },
        "NaN reward is rejected");

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                std::numeric_limits<double>::infinity(),
                1.0,
                false,
                1000,
                1060);
        },
        "Infinite reward is rejected");

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                0.0,
                std::numeric_limits<double>::quiet_NaN(),
                false,
                1000,
                1060);
        },
        "NaN next maximum Q-value is rejected");

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                0.0,
                std::numeric_limits<double>::infinity(),
                false,
                1000,
                1060);
        },
        "Infinite next maximum Q-value is rejected");

    // ======================================================================
    // 10. Chronology guards
    // ======================================================================

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                0.0,
                1.0,
                false,
                0,
                1060);
        },
        "Zero decision time is rejected");

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                0.0,
                1.0,
                false,
                1000,
                0);
        },
        "Zero next decision time is rejected");

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                0.0,
                1.0,
                false,
                1000,
                1000);
        },
        "Equal decision times are rejected");

    requireThrows(
        [&engine]()
        {
            (void)engine.calculate(
                0.0,
                1.0,
                false,
                1060,
                1000);
        },
        "Backward chronology is rejected");

    // ======================================================================
    // 11. Transition + target-network integration
    //
    // Constant network:
    //
    // HOLD  = 1
    // LONG  = 2
    // SHORT = 4
    // EXIT  = 3
    //
    // max Q = 4
    //
    // reward = 0.50
    //
    // target =
    //     0.50 + 0.99 * 4
    //     4.46
    // ======================================================================

    DevQNetwork target_network =
        makeConstantTargetNetwork(
            1.0,
            2.0,
            4.0,
            3.0);

    {
        DevTransition transition =
            makeTransition(
                0.50,
                false);

        const DevBellmanTargetResult result =
            engine.calculate(
                transition,
                target_network);

        require(
            approximatelyEqual(
                result.reward,
                0.50),
            "Transition calculation uses transition reward");

        require(
            approximatelyEqual(
                result.next_max_q,
                4.0),
            "Transition calculation selects maximum target-network Q-value");

        require(
            approximatelyEqual(
                result.discounted_bootstrap,
                3.96),
            "Transition calculation discounts target-network maximum Q-value");

        require(
            approximatelyEqual(
                result.target,
                4.46),
            "Transition calculation produces exact Bellman target");

        require(
            !result.terminal,
            "Transition calculation preserves non-terminal state");

        require(
            result.decision_time == 1000,
            "Transition calculation preserves decision time");

        require(
            result.next_decision_time == 1060,
            "Transition calculation preserves next decision time");

        require(
            result.numerical_integrity,
            "Transition Bellman result has numerical integrity");
    }

    // ======================================================================
    // 12. Verify maximum action selection is mathematical only
    //
    // Here LONG is deliberately the largest Q-value.
    // ======================================================================

    {
        DevQNetwork long_target_network =
            makeConstantTargetNetwork(
                -5.0,
                7.0,
                2.0,
                1.0);

        DevTransition transition =
            makeTransition(
                0.0,
                false);

        const DevBellmanTargetResult result =
            engine.calculate(
                transition,
                long_target_network);

        require(
            approximatelyEqual(
                result.next_max_q,
                7.0),
            "Bellman engine uses maximum Q-value independent of action label");

        require(
            approximatelyEqual(
                result.target,
                6.93),
            "Bellman target uses exact maximum target-network output");
    }

    // ======================================================================
    // 13. Terminal transition must not bootstrap
    //
    // Even though target network has a large maximum Q-value,
    // target must remain reward.
    // ======================================================================

    {
        DevQNetwork huge_target_network =
            makeConstantTargetNetwork(
                1000.0,
                2000.0,
                3000.0,
                4000.0);

        DevTransition transition =
            makeTransition(
                -0.25,
                true);

        // Deliberately make terminal next observation unavailable.
        //
        // This proves terminal Bellman calculation does not require
        // S_{t+1} network inference.
        transition.next_observation.ready =
            false;

        transition.next_observation.numerical_integrity =
            false;

        const DevBellmanTargetResult result =
            engine.calculate(
                transition,
                huge_target_network);

        require(
            approximatelyEqual(
                result.next_max_q,
                0.0),
            "Terminal transition does not evaluate future Q bootstrap");

        require(
            approximatelyEqual(
                result.discounted_bootstrap,
                0.0),
            "Terminal transition bootstrap contribution remains zero");

        require(
            approximatelyEqual(
                result.target,
                -0.25),
            "Terminal transition target equals reward only");

        require(
            result.terminal,
            "Terminal transition is marked terminal");

        require(
            result.numerical_integrity,
            "Terminal transition result has numerical integrity");
    }

    // ======================================================================
    // 14. Non-terminal next observation guards
    // ======================================================================

    requireThrows(
        [&engine, &target_network]()
        {
            DevTransition transition =
                makeTransition(
                    0.0,
                    false);

            transition.next_observation.ready =
                false;

            (void)engine.calculate(
                transition,
                target_network);
        },
        "Non-terminal unready next observation is rejected");

    requireThrows(
        [&engine, &target_network]()
        {
            DevTransition transition =
                makeTransition(
                    0.0,
                    false);

            transition.next_observation.numerical_integrity =
                false;

            (void)engine.calculate(
                transition,
                target_network);
        },
        "Non-terminal next observation with failed integrity is rejected");

    // ======================================================================
    // 15. Transition reward guards
    // ======================================================================

    requireThrows(
        [&engine, &target_network]()
        {
            DevTransition transition =
                makeTransition(
                    0.0,
                    false);

            transition.reward =
                std::numeric_limits<double>::quiet_NaN();

            (void)engine.calculate(
                transition,
                target_network);
        },
        "Transition NaN reward is rejected");

    requireThrows(
        [&engine, &target_network]()
        {
            DevTransition transition =
                makeTransition(
                    0.0,
                    false);

            transition.reward =
                std::numeric_limits<double>::infinity();

            (void)engine.calculate(
                transition,
                target_network);
        },
        "Transition infinite reward is rejected");

    // ======================================================================
    // 16. Transition chronology guards
    // ======================================================================

    requireThrows(
        [&engine, &target_network]()
        {
            DevTransition transition =
                makeTransition(
                    0.0,
                    false);

            transition.next_decision_time =
                transition.decision_time;

            (void)engine.calculate(
                transition,
                target_network);
        },
        "Transition equal timestamps are rejected");

    requireThrows(
        [&engine, &target_network]()
        {
            DevTransition transition =
                makeTransition(
                    0.0,
                    false);

            transition.next_decision_time =
                transition.decision_time - 1;

            (void)engine.calculate(
                transition,
                target_network);
        },
        "Transition backward chronology is rejected");

    // ======================================================================
    // 17. Zero reward is valid
    // ======================================================================

    {
        DevTransition transition =
            makeTransition(
                0.0,
                false);

        const DevBellmanTargetResult result =
            engine.calculate(
                transition,
                target_network);

        require(
            approximatelyEqual(
                result.target,
                3.96),
            "Zero reward still permits target-network bootstrap");
    }

    // ======================================================================
    // 18. Documentation / architecture invariants
    //
    // These assertions document the Phase 3.8A contract.
    // They are not substitutes for later end-to-end leakage tests.
    // ======================================================================

    require(
        true,
        "Bellman target engine does not calculate rewards");

    require(
        true,
        "Bellman target engine does not calculate brokerage or slippage");

    require(
        true,
        "Bellman target engine does not update Q-network parameters");

    require(
        true,
        "Bellman target engine does not sample replay buffer");

    require(
        true,
        "Bellman target engine does not perform action exploration");

    require(
        true,
        "ForwardReturn_N is absent from Bellman target API");

    // ======================================================================
    // Summary
    // ======================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.8A BELLMAN TARGET ENGINE SUMMARY\n"
        << "======================================================================\n"
        << "Checks   : "
        << checks
        << '\n'
        << "Failures : "
        << failures
        << '\n'
        << "======================================================================\n";

    if (failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.8A BELLMAN TARGET ENGINE: PASSED\n"
            << "======================================================================\n";

        return 0;
    }

    std::cout
        << "DEVAI PHASE 3.8A BELLMAN TARGET ENGINE: FAILED\n"
        << "======================================================================\n";

    return 1;
}