#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <string>


namespace
{

using namespace devai::strategy;


struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};


void require(
    const bool condition,
    const std::string& description,
    TestStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << description
            << '\n';
    }
    else
    {
        ++stats.failures;

        std::cerr
            << "[FAIL] "
            << description
            << '\n';
    }
}


template <typename Function>
void expectThrow(
    Function&& function,
    const std::string& description,
    TestStats& stats)
{
    bool threw =
        false;

    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw =
            true;
    }

    require(
        threw,
        description,
        stats);
}


bool approximatelyEqual(
    const double left,
    const double right,
    const double tolerance = 1.0e-12)
{
    return
        std::fabs(left - right) <=
        tolerance;
}


// ============================================================================
// zeroInput
// ============================================================================

std::array<double, DEV_Q_INPUT_SIZE>
zeroInput()
{
    return {};
}


// ============================================================================
// patternedInput
// ============================================================================

std::array<double, DEV_Q_INPUT_SIZE>
patternedInput()
{
    std::array<double, DEV_Q_INPUT_SIZE>
        input{};


    for (std::size_t index = 0;
         index < input.size();
         ++index)
    {
        input[index] =
            static_cast<double>(
                static_cast<int>(index % 11) - 5) /
            10.0;
    }


    return
        input;
}


// ============================================================================
// Test dimensions
// ============================================================================

void testDimensions(
    TestStats& stats)
{
    require(
        DEV_Q_INPUT_SIZE == 84,
        "Q-network input size matches frozen 84-value observation",
        stats);

    require(
        DEV_Q_HIDDEN1_SIZE == 64,
        "Q-network first hidden layer contains 64 neurons",
        stats);

    require(
        DEV_Q_HIDDEN2_SIZE == 32,
        "Q-network second hidden layer contains 32 neurons",
        stats);

    require(
        DEV_Q_OUTPUT_SIZE == 4,
        "Q-network output matches four Dev actions",
        stats);
}


// ============================================================================
// Test parameter count
// ============================================================================

void testParameterCount(
    TestStats& stats)
{
    DevQNetwork network;


    constexpr std::size_t expected =
        (84 * 64) + 64 +
        (64 * 32) + 32 +
        (32 * 4) + 4;


    require(
        expected == 7652,
        "Expected architecture contains 7,652 trainable parameters",
        stats);


    require(
        network.parameterCount() ==
            expected,
        "Q-network reports exact parameter count",
        stats);
}


// ============================================================================
// Test deterministic initialization
// ============================================================================

void testDeterministicInitialization(
    TestStats& stats)
{
    DevQNetworkConfig config;

    config.random_seed =
        777U;


    DevQNetwork first(
        config);

    DevQNetwork second(
        config);


    const auto input =
        patternedInput();


    const auto q1 =
        first.predict(
            input);

    const auto q2 =
        second.predict(
            input);


    require(
        q1.numerical_integrity,
        "First deterministic network has numerical integrity",
        stats);


    require(
        q2.numerical_integrity,
        "Second deterministic network has numerical integrity",
        stats);


    for (std::size_t index = 0;
         index < DEV_Q_OUTPUT_SIZE;
         ++index)
    {
        require(
            approximatelyEqual(
                q1.values[index],
                q2.values[index]),
            "Same seed produces identical Q-value",
            stats);
    }
}


// ============================================================================
// Test different seed
// ============================================================================

void testDifferentSeeds(
    TestStats& stats)
{
    DevQNetworkConfig config1;
    DevQNetworkConfig config2;

    config1.random_seed =
        1U;

    config2.random_seed =
        2U;


    DevQNetwork first(
        config1);

    DevQNetwork second(
        config2);


    const auto input =
        patternedInput();


    const auto q1 =
        first.predict(
            input);

    const auto q2 =
        second.predict(
            input);


    bool any_difference =
        false;


    for (std::size_t index = 0;
         index < DEV_Q_OUTPUT_SIZE;
         ++index)
    {
        if (!approximatelyEqual(
                q1.values[index],
                q2.values[index]))
        {
            any_difference =
                true;

            break;
        }
    }


    require(
        any_difference,
        "Different initialization seeds produce different model outputs",
        stats);
}


// ============================================================================
// Test zero initialization
// ============================================================================

void testZeroInitialization(
    TestStats& stats)
{
    DevQNetworkConfig config;

    config.zero_initialize =
        true;


    DevQNetwork network(
        config);


    const auto q_values =
        network.predict(
            patternedInput());


    require(
        q_values.numerical_integrity,
        "Zero-initialized network remains numerically valid",
        stats);


    for (const double value :
         q_values.values)
    {
        require(
            approximatelyEqual(
                value,
                0.0),
            "Zero-initialized network produces zero Q-value",
            stats);
    }


    require(
        q_values.greedyAction() ==
            DevAction::HOLD,
        "Equal Q-values deterministically resolve to HOLD",
        stats);


    require(
        approximatelyEqual(
            q_values.maximumValue(),
            0.0),
        "Maximum zero-initialized Q-value is zero",
        stats);
}


// ============================================================================
// Test output action mapping
// ============================================================================

void testActionMapping(
    TestStats& stats)
{
    DevQNetworkConfig config;

    config.zero_initialize =
        true;


    DevQNetwork network(
        config);


    auto parameters =
        network.parameters();


    parameters.biases3[0] =
        1.0;

    parameters.biases3[1] =
        2.0;

    parameters.biases3[2] =
        3.0;

    parameters.biases3[3] =
        4.0;


    network.setParameters(
        parameters);


    const auto q_values =
        network.predict(
            zeroInput());


    require(
        approximatelyEqual(
            q_values.value(
                DevAction::HOLD),
            1.0),
        "Output index 0 maps to HOLD",
        stats);


    require(
        approximatelyEqual(
            q_values.value(
                DevAction::LONG),
            2.0),
        "Output index 1 maps to LONG",
        stats);


    require(
        approximatelyEqual(
            q_values.value(
                DevAction::SHORT),
            3.0),
        "Output index 2 maps to SHORT",
        stats);


    require(
        approximatelyEqual(
            q_values.value(
                DevAction::EXIT),
            4.0),
        "Output index 3 maps to EXIT",
        stats);


    require(
        q_values.greedyAction() ==
            DevAction::EXIT,
        "Greedy action selects maximum Q-value",
        stats);


    require(
        approximatelyEqual(
            q_values.maximumValue(),
            4.0),
        "maximumValue returns largest Q-value",
        stats);
}


// ============================================================================
// Test parameter copying
// ============================================================================

void testParameterCopy(
    TestStats& stats)
{
    DevQNetworkConfig first_config;
    DevQNetworkConfig second_config;

    first_config.random_seed =
        100U;

    second_config.random_seed =
        200U;


    DevQNetwork first(
        first_config);

    DevQNetwork second(
        second_config);


    const auto input =
        patternedInput();


    second.copyParametersFrom(
        first);


    const auto q1 =
        first.predict(
            input);

    const auto q2 =
        second.predict(
            input);


    bool identical =
        true;


    for (std::size_t index = 0;
         index < DEV_Q_OUTPUT_SIZE;
         ++index)
    {
        if (!approximatelyEqual(
                q1.values[index],
                q2.values[index]))
        {
            identical =
                false;

            break;
        }
    }


    require(
        identical,
        "Parameter copy produces identical Q-network inference",
        stats);
}


// ============================================================================
// Test manual parameter assignment
// ============================================================================

void testManualParameters(
    TestStats& stats)
{
    DevQNetworkConfig config;

    config.zero_initialize =
        true;


    DevQNetwork network(
        config);


    auto parameters =
        network.parameters();


    // --------------------------------------------------------
    // Create one exact active path:
    //
    // input[0]
    //   ↓ weight 2
    // hidden1[0]
    //   ↓ weight 3
    // hidden2[0]
    //   ↓ output weights
    //
    // input[0] = 0.5
    //
    // hidden1 = ReLU(2 * 0.5) = 1
    // hidden2 = ReLU(3 * 1)   = 3
    //
    // outputs:
    // HOLD  = 3 * 1 = 3
    // LONG  = 3 * 2 = 6
    // SHORT = 3 * 3 = 9
    // EXIT  = 3 * 4 = 12
    // --------------------------------------------------------

    parameters.weights1[0] =
        2.0;

    parameters.weights2[0] =
        3.0;

    parameters.weights3[
        0 * DEV_Q_HIDDEN2_SIZE] =
        1.0;

    parameters.weights3[
        1 * DEV_Q_HIDDEN2_SIZE] =
        2.0;

    parameters.weights3[
        2 * DEV_Q_HIDDEN2_SIZE] =
        3.0;

    parameters.weights3[
        3 * DEV_Q_HIDDEN2_SIZE] =
        4.0;


    network.setParameters(
        parameters);


    auto input =
        zeroInput();

    input[0] =
        0.5;


    const auto q_values =
        network.predict(
            input);


    require(
        approximatelyEqual(
            q_values.values[0],
            3.0),
        "Manual forward pass produces exact HOLD Q-value",
        stats);

    require(
        approximatelyEqual(
            q_values.values[1],
            6.0),
        "Manual forward pass produces exact LONG Q-value",
        stats);

    require(
        approximatelyEqual(
            q_values.values[2],
            9.0),
        "Manual forward pass produces exact SHORT Q-value",
        stats);

    require(
        approximatelyEqual(
            q_values.values[3],
            12.0),
        "Manual forward pass produces exact EXIT Q-value",
        stats);
}


// ============================================================================
// Test ReLU behavior
// ============================================================================

void testReLUBehavior(
    TestStats& stats)
{
    DevQNetworkConfig config;

    config.zero_initialize =
        true;


    DevQNetwork network(
        config);


    auto parameters =
        network.parameters();


    parameters.weights1[0] =
        2.0;

    parameters.weights2[0] =
        3.0;

    parameters.weights3[0] =
        5.0;


    network.setParameters(
        parameters);


    auto input =
        zeroInput();

    input[0] =
        -1.0;


    const auto q_values =
        network.predict(
            input);


    require(
        approximatelyEqual(
            q_values.values[0],
            0.0),
        "Negative hidden activation is clipped by ReLU",
        stats);
}


// ============================================================================
// Test invalid input
// ============================================================================

void testInvalidInput(
    TestStats& stats)
{
    DevQNetwork network;


    auto input =
        zeroInput();


    input[5] =
        std::numeric_limits<double>::quiet_NaN();


    expectThrow(
        [&]()
        {
            (void)network.predict(
                input);
        },
        "NaN network input is rejected",
        stats);


    input =
        zeroInput();


    input[7] =
        std::numeric_limits<double>::infinity();


    expectThrow(
        [&]()
        {
            (void)network.predict(
                input);
        },
        "Infinite network input is rejected",
        stats);
}


// ============================================================================
// Test invalid parameters
// ============================================================================

void testInvalidParameters(
    TestStats& stats)
{
    DevQNetwork network;


    {
        auto parameters =
            network.parameters();

        parameters.weights1.pop_back();


        expectThrow(
            [&]()
            {
                network.setParameters(
                    parameters);
            },
            "Incorrect weight-vector size is rejected",
            stats);
    }


    {
        auto parameters =
            network.parameters();

        parameters.weights2[0] =
            std::numeric_limits<double>::quiet_NaN();


        expectThrow(
            [&]()
            {
                network.setParameters(
                    parameters);
            },
            "NaN network parameter is rejected",
            stats);
    }


    {
        auto parameters =
            network.parameters();

        parameters.biases3[0] =
            std::numeric_limits<double>::infinity();


        expectThrow(
            [&]()
            {
                network.setParameters(
                    parameters);
            },
            "Infinite network parameter is rejected",
            stats);
    }
}


// ============================================================================
// Test Q-value integrity guard
// ============================================================================

void testQValueIntegrityGuard(
    TestStats& stats)
{
    DevQValues invalid;


    expectThrow(
        [&]()
        {
            (void)invalid.greedyAction();
        },
        "greedyAction rejects invalid Q-value state",
        stats);


    expectThrow(
        [&]()
        {
            (void)invalid.maximumValue();
        },
        "maximumValue rejects invalid Q-value state",
        stats);
}


} // namespace


int main()
{
    TestStats stats;


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.7 — DEV NEURAL NETWORK / Q MODEL TEST\n"
        << "============================================================\n";


    try
    {
        testDimensions(
            stats);

        testParameterCount(
            stats);

        testDeterministicInitialization(
            stats);

        testDifferentSeeds(
            stats);

        testZeroInitialization(
            stats);

        testActionMapping(
            stats);

        testParameterCopy(
            stats);

        testManualParameters(
            stats);

        testReLUBehavior(
            stats);

        testInvalidInput(
            stats);

        testInvalidParameters(
            stats);

        testQValueIntegrityGuard(
            stats);
    }
    catch (const std::exception& exception)
    {
        ++stats.failures;

        std::cerr
            << "\n[FATAL] Unexpected exception: "
            << exception.what()
            << '\n';
    }
    catch (...)
    {
        ++stats.failures;

        std::cerr
            << "\n[FATAL] Unknown unexpected exception.\n";
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 3.7 TEST SUMMARY\n"
        << "============================================================\n"
        << "Checks   : "
        << stats.checks
        << '\n'
        << "Failures : "
        << stats.failures
        << '\n'
        << "============================================================\n";


    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.7 DEV NEURAL NETWORK / Q MODEL: PASSED\n"
            << "============================================================\n";

        return 0;
    }


    std::cerr
        << "DEVAI PHASE 3.7 DEV NEURAL NETWORK / Q MODEL: FAILED\n"
        << "============================================================\n";

    return 1;
}