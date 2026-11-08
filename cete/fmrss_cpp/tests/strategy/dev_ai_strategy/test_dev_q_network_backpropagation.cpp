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
        std::cout << "[PASS] " << description << '\n';
    }
    else
    {
        ++stats.failures;
        std::cerr << "[FAIL] " << description << '\n';
    }
}

template <typename Function>
void expectThrow(
    Function&& function,
    const std::string& description,
    TestStats& stats)
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

    require(threw, description, stats);
}

bool approximatelyEqual(
    const double left,
    const double right,
    const double tolerance = 1.0e-12)
{
    return std::fabs(left - right) <= tolerance;
}

DevAIObservation makeObservation()
{
    DevAIObservation observation{};

    // A valid, ready frozen observation.  The normalizer owns flattening.
    observation.numerical_integrity = true;
    observation.ready = true;

    // Make a small deterministic signal while remaining compatible with
    // default-initialized observation fields.
    observation.market[0] = 0.5;
    observation.availability[0] = 1.0;

    return observation;
}

DevQNetwork makeExactPathNetwork()
{
    DevQNetworkConfig config{};
    config.zero_initialize = true;

    DevQNetwork network(config);
    auto parameters = network.parameters();

    // Frozen 3.7 row-major path:
    // normalized input[0] -> h1[0] -> h2[0] -> Q(LONG)
    parameters.weights1[0 * DEV_Q_INPUT_SIZE + 0] = 2.0;
    parameters.weights2[0 * DEV_Q_HIDDEN1_SIZE + 0] = 3.0;
    parameters.weights3[
        static_cast<std::size_t>(DevAction::LONG) *
            DEV_Q_HIDDEN2_SIZE +
        0] = 2.0;

    network.setParameters(parameters);
    return network;
}

void testFrozenLayout(TestStats& stats)
{
    DevQNetworkConfig config{};
    config.zero_initialize = true;
    DevQNetwork network(config);

    auto p = network.parameters();

    p.weights1[0] = 2.0;
    p.weights2[0] = 3.0;

    p.weights3[0 * DEV_Q_HIDDEN2_SIZE] = 1.0;
    p.weights3[1 * DEV_Q_HIDDEN2_SIZE] = 2.0;
    p.weights3[2 * DEV_Q_HIDDEN2_SIZE] = 3.0;
    p.weights3[3 * DEV_Q_HIDDEN2_SIZE] = 4.0;

    network.setParameters(p);

    std::array<double, DEV_Q_INPUT_SIZE> input{};
    input[0] = 0.5;

    const auto q = network.predict(input);

    require(approximatelyEqual(q.values[0], 3.0),
            "Frozen W3 layout preserves exact HOLD Q-value", stats);
    require(approximatelyEqual(q.values[1], 6.0),
            "Frozen W3 layout preserves exact LONG Q-value", stats);
    require(approximatelyEqual(q.values[2], 9.0),
            "Frozen W3 layout preserves exact SHORT Q-value", stats);
    require(approximatelyEqual(q.values[3], 12.0),
            "Frozen W3 layout preserves exact EXIT Q-value", stats);
}

void testHuberQuadraticRegion(TestStats& stats)
{
    auto network = makeExactPathNetwork();
    auto observation = makeObservation();

    const double before =
        network.predict(observation).value(DevAction::LONG);

    DevQTrainingConfig config{};
    config.learning_rate = 0.001;
    config.huber_delta = 1.0;
    config.gradient_clip = 0.0;

    const double target = before + 0.5;

    const auto result =
        network.trainSingle(
            observation,
            DevAction::LONG,
            target,
            config);

    require(result.numerical_integrity,
            "Quadratic-region training result has numerical integrity", stats);

    require(approximatelyEqual(result.td_error, 0.5),
            "TD error equals target minus prediction", stats);

    require(approximatelyEqual(result.loss, 0.125),
            "Huber loss is quadratic inside delta", stats);

    require(result.predicted_q_after > result.predicted_q_before,
            "Positive TD error moves selected Q-value upward", stats);

    require(result.gradient_l2_norm > 0.0,
            "Backpropagation produces non-zero gradient", stats);
}

void testHuberLinearRegion(TestStats& stats)
{
    auto network = makeExactPathNetwork();
    auto observation = makeObservation();

    const double before =
        network.predict(observation).value(DevAction::LONG);

    DevQTrainingConfig config{};
    config.learning_rate = 0.001;
    config.huber_delta = 1.0;
    config.gradient_clip = 0.0;

    const auto result =
        network.trainSingle(
            observation,
            DevAction::LONG,
            before + 3.0,
            config);

    require(approximatelyEqual(result.td_error, 3.0),
            "Large TD error is preserved in result", stats);

    require(approximatelyEqual(result.loss, 2.5),
            "Huber loss is linear outside delta", stats);
}

void testNegativeErrorMovesDown(TestStats& stats)
{
    auto network = makeExactPathNetwork();
    auto observation = makeObservation();

    const double before =
        network.predict(observation).value(DevAction::LONG);

    const auto result =
        network.trainSingle(
            observation,
            DevAction::LONG,
            before - 0.5);

    require(result.td_error < 0.0,
            "Negative target produces negative TD error", stats);

    require(result.predicted_q_after < result.predicted_q_before,
            "Negative TD error moves selected Q-value downward", stats);
}

void testOnlySelectedOutputDirectlyUpdated(TestStats& stats)
{
    auto network = makeExactPathNetwork();
    auto observation = makeObservation();

    const auto before =
        network.parameters();

    const double target =
        network.predict(observation).value(DevAction::LONG) + 0.5;

    (void)network.trainSingle(
        observation,
        DevAction::LONG,
        target);

    const auto after =
        network.parameters();

    bool other_output_rows_unchanged = true;

    for (std::size_t action = 0;
         action < DEV_Q_OUTPUT_SIZE;
         ++action)
    {
        if (action == static_cast<std::size_t>(DevAction::LONG))
        {
            continue;
        }

        if (!approximatelyEqual(
                before.biases3[action],
                after.biases3[action]))
        {
            other_output_rows_unchanged = false;
        }

        for (std::size_t hidden2 = 0;
             hidden2 < DEV_Q_HIDDEN2_SIZE;
             ++hidden2)
        {
            const std::size_t index =
                action * DEV_Q_HIDDEN2_SIZE + hidden2;

            if (!approximatelyEqual(
                    before.weights3[index],
                    after.weights3[index]))
            {
                other_output_rows_unchanged = false;
            }
        }
    }

    require(other_output_rows_unchanged,
            "Non-selected output rows receive no direct Q-loss update", stats);
}

void testGradientClipping(TestStats& stats)
{
    auto network = makeExactPathNetwork();
    auto observation = makeObservation();

    const double before =
        network.predict(observation).value(DevAction::LONG);

    DevQTrainingConfig config{};
    config.learning_rate = 0.001;
    config.huber_delta = 100.0;
    config.gradient_clip = 0.01;

    const auto result =
        network.trainSingle(
            observation,
            DevAction::LONG,
            before + 50.0,
            config);

    require(
        result.clipped_gradient_l2_norm <=
            result.gradient_l2_norm + 1.0e-12,
        "Gradient clipping cannot increase L2 norm",
        stats);

    require(
        result.clipped_gradient_l2_norm <
            result.gradient_l2_norm,
        "Small clipping threshold reduces large gradient norm",
        stats);
}

void testZeroInitializedLearning(TestStats& stats)
{
    DevQNetworkConfig network_config{};
    network_config.zero_initialize = true;

    DevQNetwork network(network_config);
    auto observation = makeObservation();

    const auto result =
        network.trainSingle(
            observation,
            DevAction::SHORT,
            1.0);

    const auto q =
        network.predict(observation);

    require(
        result.predicted_q_before == 0.0,
        "Zero network begins selected Q-value at zero",
        stats);

    require(
        q.value(DevAction::SHORT) > 0.0,
        "Output bias allows zero-initialized network selected Q to move",
        stats);
}

void testInvalidTrainingInputs(TestStats& stats)
{
    auto observation = makeObservation();

    expectThrow(
        [&]()
        {
            DevQNetwork network;
            (void)network.trainSingle(
                observation,
                DevAction::HOLD,
                std::numeric_limits<double>::quiet_NaN());
        },
        "NaN target Q-value is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevQNetwork network;
            DevQTrainingConfig config{};
            config.learning_rate = 0.0;
            (void)network.trainSingle(
                observation,
                DevAction::HOLD,
                0.0,
                config);
        },
        "Zero learning rate is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevQNetwork network;
            DevQTrainingConfig config{};
            config.huber_delta = 0.0;
            (void)network.trainSingle(
                observation,
                DevAction::HOLD,
                0.0,
                config);
        },
        "Zero Huber delta is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevQNetwork network;
            DevQTrainingConfig config{};
            config.gradient_clip =
                std::numeric_limits<double>::infinity();

            (void)network.trainSingle(
                observation,
                DevAction::HOLD,
                0.0,
                config);
        },
        "Infinite gradient clip is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevQNetwork network;
            auto invalid = observation;
            invalid.ready = false;

            (void)network.trainSingle(
                invalid,
                DevAction::HOLD,
                0.0);
        },
        "Unready observation is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevQNetwork network;
            auto invalid = observation;
            invalid.numerical_integrity = false;

            (void)network.trainSingle(
                invalid,
                DevAction::HOLD,
                0.0);
        },
        "Observation integrity failure is rejected",
        stats);
}

void testParameterCountUnchanged(TestStats& stats)
{
    DevQNetwork network;

    require(
        network.parameterCount() == 7652,
        "Phase 3.8B preserves frozen 7,652-parameter architecture",
        stats);
}

} // namespace

int main()
{
    TestStats stats{};

    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.8B — Q-NETWORK GRADIENT / BACKPROPAGATION TEST\n"
        << "======================================================================\n";

    try
    {
        testFrozenLayout(stats);
        testHuberQuadraticRegion(stats);
        testHuberLinearRegion(stats);
        testNegativeErrorMovesDown(stats);
        testOnlySelectedOutputDirectlyUpdated(stats);
        testGradientClipping(stats);
        testZeroInitializedLearning(stats);
        testInvalidTrainingInputs(stats);
        testParameterCountUnchanged(stats);
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
        << "======================================================================\n"
        << "PHASE 3.8B TEST SUMMARY\n"
        << "======================================================================\n"
        << "Checks   : " << stats.checks << '\n'
        << "Failures : " << stats.failures << '\n'
        << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.8B Q-NETWORK GRADIENT / BACKPROPAGATION: PASSED\n"
            << "======================================================================\n";
        return 0;
    }

    std::cerr
        << "DEVAI PHASE 3.8B Q-NETWORK GRADIENT / BACKPROPAGATION: FAILED\n"
        << "======================================================================\n";

    return 1;
}
