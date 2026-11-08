#include "devai/strategy/dev_ai_strategy/DevMiniBatchTrainer.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};

void require(
    bool condition,
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
    double left,
    double right,
    double tolerance = 1.0e-12)
{
    return std::fabs(left - right) <= tolerance;
}

DevAIObservation makeObservation()
{
    DevAIObservation observation{};
    observation.numerical_integrity = true;
    observation.ready = true;
    return observation;
}

DevTransition makeTerminalTransition(
    DevAction action,
    double reward,
    std::int64_t decision_time)
{
    DevTransition transition{};
    transition.observation = makeObservation();
    transition.next_observation = makeObservation();
    transition.action = action;
    transition.reward = reward;
    transition.terminal = true;
    transition.decision_time = decision_time;
    transition.next_decision_time = decision_time + 60;
    return transition;
}

DevQNetwork makeZeroNetwork()
{
    DevQNetworkConfig config{};
    config.zero_initialize = true;
    return DevQNetwork(config);
}

void testTrueAverageNotSequentialSGD(TestStats& stats)
{
    DevQNetwork network = makeZeroNetwork();

    DevQTrainingConfig config{};
    config.learning_rate = 0.1;
    config.huber_delta = 1.0;
    config.gradient_clip = 0.0;

    const DevAIObservation observation =
        makeObservation();

    std::vector<DevQTrainingSample> batch{
        {observation, DevAction::LONG, +0.5},
        {observation, DevAction::LONG, -0.5}
    };

    const auto before =
        network.parameters();

    const auto result =
        network.trainBatch(batch, config);

    const auto after =
        network.parameters();

    require(result.batch_size == 2,
            "Batch reports exact sample count", stats);

    require(approximatelyEqual(result.mean_td_error, 0.0),
            "Opposite TD errors average to zero", stats);

    require(approximatelyEqual(result.mean_loss, 0.125),
            "Mean Huber loss is calculated from common pre-update network", stats);

    require(approximatelyEqual(
                after.biases3[static_cast<std::size_t>(DevAction::LONG)],
                before.biases3[static_cast<std::size_t>(DevAction::LONG)]),
            "Opposite sample gradients cancel before one batch update", stats);

    require(approximatelyEqual(result.gradient_l2_norm, 0.0),
            "Cancelled true batch has zero averaged gradient norm", stats);
}

void testBatchGradientIsAveraged(TestStats& stats)
{
    DevQNetwork network = makeZeroNetwork();

    DevQTrainingConfig config{};
    config.learning_rate = 0.1;
    config.huber_delta = 1.0;
    config.gradient_clip = 0.0;

    const DevAIObservation observation =
        makeObservation();

    std::vector<DevQTrainingSample> batch{
        {observation, DevAction::SHORT, 0.5},
        {observation, DevAction::SHORT, 0.5}
    };

    const auto result =
        network.trainBatch(batch, config);

    const auto parameters =
        network.parameters();

    const std::size_t short_index =
        static_cast<std::size_t>(DevAction::SHORT);

    require(approximatelyEqual(
                parameters.biases3[short_index],
                0.05),
            "Two identical samples use averaged gradient, not summed gradient",
            stats);

    require(approximatelyEqual(result.mean_td_error, 0.5),
            "Mean TD error is exact", stats);

    require(approximatelyEqual(result.mean_absolute_td_error, 0.5),
            "Mean absolute TD error is exact", stats);
}

void testSingleItemBatchMatchesSingleSample(TestStats& stats)
{
    DevQNetwork single = makeZeroNetwork();
    DevQNetwork batch = makeZeroNetwork();

    DevQTrainingConfig config{};
    config.learning_rate = 0.1;
    config.huber_delta = 1.0;
    config.gradient_clip = 0.0;

    const DevAIObservation observation =
        makeObservation();

    (void)single.trainSingle(
        observation,
        DevAction::EXIT,
        0.25,
        config);

    (void)batch.trainBatch(
        std::vector<DevQTrainingSample>{
            {observation, DevAction::EXIT, 0.25}},
        config);

    const auto p1 = single.parameters();
    const auto p2 = batch.parameters();

    require(p1.weights1 == p2.weights1 &&
            p1.biases1 == p2.biases1 &&
            p1.weights2 == p2.weights2 &&
            p1.biases2 == p2.biases2 &&
            p1.weights3 == p2.weights3 &&
            p1.biases3 == p2.biases3,
            "One-item mini-batch exactly matches Phase 3.8B single-sample SGD",
            stats);
}

void testMiniBatchTrainerUsesBellmanTargets(TestStats& stats)
{
    DevQNetwork online = makeZeroNetwork();
    DevQNetwork target = makeZeroNetwork();

    DevMiniBatchTrainerConfig config{};
    config.bellman.discount_factor = 0.99;
    config.training.learning_rate = 0.1;
    config.training.huber_delta = 1.0;
    config.training.gradient_clip = 0.0;

    DevMiniBatchTrainer trainer(config);

    std::vector<DevTransition> transitions{
        makeTerminalTransition(DevAction::LONG, 0.4, 1000),
        makeTerminalTransition(DevAction::LONG, 0.6, 1060)
    };

    const auto target_before =
        target.parameters();

    const auto result =
        trainer.train(
            transitions,
            online,
            target);

    const auto target_after =
        target.parameters();

    require(result.batch_size == 2,
            "Mini-batch trainer processes every transition", stats);

    require(approximatelyEqual(result.mean_bellman_target, 0.5),
            "Terminal Bellman targets equal rewards and average correctly", stats);

    require(approximatelyEqual(result.mean_td_error, 0.5),
            "Trainer reports exact mean TD error", stats);

    require(approximatelyEqual(
                online.parameters().biases3[
                    static_cast<std::size_t>(DevAction::LONG)],
                0.05),
            "Trainer applies one averaged online-network update", stats);

    require(target_before.weights1 == target_after.weights1 &&
            target_before.biases1 == target_after.biases1 &&
            target_before.weights2 == target_after.weights2 &&
            target_before.biases2 == target_after.biases2 &&
            target_before.weights3 == target_after.weights3 &&
            target_before.biases3 == target_after.biases3,
            "Mini-batch trainer never mutates target network", stats);
}

void testNonTerminalBellmanBootstrap(TestStats& stats)
{
    DevQNetwork online = makeZeroNetwork();
    DevQNetwork target = makeZeroNetwork();

    auto target_parameters =
        target.parameters();

    // With a zero observation/network hidden state, output biases directly
    // define Q_target(next_state, action). Maximum is 0.20.
    target_parameters.biases3[0] = 0.10;
    target_parameters.biases3[1] = 0.20;
    target_parameters.biases3[2] = -0.10;
    target_parameters.biases3[3] = 0.05;
    target.setParameters(target_parameters);

    DevMiniBatchTrainerConfig config{};
    config.bellman.discount_factor = 0.5;
    config.training.learning_rate = 0.1;
    config.training.huber_delta = 1.0;
    config.training.gradient_clip = 0.0;

    DevMiniBatchTrainer trainer(config);

    DevTransition transition{};
    transition.observation = makeObservation();
    transition.next_observation = makeObservation();
    transition.action = DevAction::HOLD;
    transition.reward = 0.30;
    transition.terminal = false;
    transition.decision_time = 2000;
    transition.next_decision_time = 2060;

    const auto result =
        trainer.train(
            std::vector<DevTransition>{transition},
            online,
            target);

    // 0.30 + 0.5 * 0.20 = 0.40
    require(approximatelyEqual(result.mean_bellman_target, 0.40),
            "Non-terminal mini-batch uses frozen Bellman bootstrap", stats);
}

void testValidation(TestStats& stats)
{
    DevQNetwork network = makeZeroNetwork();

    expectThrow(
        [&]()
        {
            (void)network.trainBatch(
                std::vector<DevQTrainingSample>{});
        },
        "Empty Q-training batch is rejected",
        stats);

    expectThrow(
        [&]()
        {
            auto observation = makeObservation();

            (void)network.trainBatch(
                std::vector<DevQTrainingSample>{
                    {observation,
                     DevAction::HOLD,
                     std::numeric_limits<double>::quiet_NaN()}});
        },
        "NaN batch target is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevQNetwork online = makeZeroNetwork();
            DevQNetwork target = makeZeroNetwork();
            DevMiniBatchTrainer trainer;

            (void)trainer.train(
                std::vector<DevTransition>{},
                online,
                target);
        },
        "Empty transition mini-batch is rejected",
        stats);

    expectThrow(
        [&]()
        {
            DevMiniBatchTrainerConfig config{};
            config.training.learning_rate = 0.0;
            DevMiniBatchTrainer trainer(config);
            (void)trainer;
        },
        "Invalid mini-batch learning rate is rejected",
        stats);
}

void testArchitectureStillFrozen(TestStats& stats)
{
    DevQNetwork network;

    require(network.parameterCount() == 7652,
            "Phase 3.8C preserves frozen 7,652-parameter architecture",
            stats);

    require(DEV_Q_INPUT_SIZE == 84,
            "Phase 3.8C preserves frozen 84-value observation input",
            stats);

    require(DEV_Q_OUTPUT_SIZE == 4,
            "Phase 3.8C preserves four-action Q output",
            stats);
}

} // namespace

int main()
{
    TestStats stats{};

    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.8C — MINI-BATCH TRAINING TEST\n"
        << "======================================================================\n";

    try
    {
        testTrueAverageNotSequentialSGD(stats);
        testBatchGradientIsAveraged(stats);
        testSingleItemBatchMatchesSingleSample(stats);
        testMiniBatchTrainerUsesBellmanTargets(stats);
        testNonTerminalBellmanBootstrap(stats);
        testValidation(stats);
        testArchitectureStillFrozen(stats);
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
        << "PHASE 3.8C TEST SUMMARY\n"
        << "======================================================================\n"
        << "Checks   : " << stats.checks << '\n'
        << "Failures : " << stats.failures << '\n'
        << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.8C MINI-BATCH TRAINING: PASSED\n"
            << "======================================================================\n";
        return 0;
    }

    std::cerr
        << "DEVAI PHASE 3.8C MINI-BATCH TRAINING: FAILED\n"
        << "======================================================================\n";

    return 1;
}
