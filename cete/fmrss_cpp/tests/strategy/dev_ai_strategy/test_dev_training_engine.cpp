#include "devai/strategy/dev_ai_strategy/DevTrainingEngine.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;

struct Stats
{
    std::size_t checks{0};
    std::size_t failures{0};
};

void check(bool condition, const std::string& description, Stats& stats)
{
    ++stats.checks;
    if (condition)
        std::cout << "[PASS] " << description << '\n';
    else
    {
        ++stats.failures;
        std::cerr << "[FAIL] " << description << '\n';
    }
}

template <typename Function>
void expectThrow(Function&& function, const std::string& description, Stats& stats)
{
    bool threw = false;
    try { function(); }
    catch (const std::exception&) { threw = true; }
    check(threw, description, stats);
}

bool sameParameters(
    const DevQNetworkParameters& a,
    const DevQNetworkParameters& b)
{
    return a.weights1 == b.weights1 &&
           a.biases1 == b.biases1 &&
           a.weights2 == b.weights2 &&
           a.biases2 == b.biases2 &&
           a.weights3 == b.weights3 &&
           a.biases3 == b.biases3;
}

DevAIObservation makeObservation(double market0 = 0.0)
{
    DevAIObservation observation{};
    observation.market[0] = market0;
    observation.numerical_integrity = true;
    observation.ready = true;
    return observation;
}

DevTransition makeTransition(
    DevAction action,
    double reward,
    bool terminal,
    std::int64_t time,
    double current_market0 = 0.0,
    double next_market0 = 0.0)
{
    DevTransition transition{};
    transition.observation = makeObservation(current_market0);
    transition.action = action;
    transition.reward = reward;
    transition.next_observation = makeObservation(next_market0);
    transition.terminal = terminal;
    transition.decision_time = time;
    transition.next_decision_time = time + 60;
    return transition;
}

DevTrainingEngineConfig makeConfig(std::size_t sync_interval)
{
    DevTrainingEngineConfig config{};
    config.mini_batch.bellman.discount_factor = 0.99;
    config.mini_batch.training.learning_rate = 0.01;
    config.mini_batch.training.huber_delta = 1.0;
    config.mini_batch.training.gradient_clip = 10.0;
    config.target_synchronization.synchronization_interval = sync_interval;
    return config;
}

void testResetAndInitialization(Stats& stats)
{
    DevQNetwork online({.random_seed = 11U, .zero_initialize = false});
    DevQNetwork target({.random_seed = 22U, .zero_initialize = false});
    DevTrainingEngine engine(makeConfig(3));

    check(!engine.initialized(), "Engine starts uninitialized", stats);
    check(engine.completedTrainingUpdates() == 0,
          "Engine starts with zero training updates", stats);
    check(!sameParameters(online.parameters(), target.parameters()),
          "Different seeds initially produce different networks", stats);

    engine.reset(online, target);

    check(engine.initialized(), "Reset initializes training engine", stats);
    check(engine.completedTrainingUpdates() == 0,
          "Reset keeps training-update count at zero", stats);
    check(sameParameters(online.parameters(), target.parameters()),
          "Reset hard-synchronizes target from online", stats);
}

void testRequiresResetAndNonEmptyBatch(Stats& stats)
{
    DevQNetwork online({.random_seed = 1U, .zero_initialize = true});
    DevQNetwork target({.random_seed = 2U, .zero_initialize = true});
    DevTrainingEngine engine(makeConfig(2));

    std::vector<DevTransition> batch{
        makeTransition(DevAction::LONG, 0.01, true, 1000)};

    expectThrow(
        [&] { (void)engine.trainBatch(batch, online, target); },
        "Training before reset is rejected",
        stats);

    engine.reset(online, target);

    expectThrow(
        [&] {
            const std::vector<DevTransition> empty;
            (void)engine.trainBatch(empty, online, target);
        },
        "Empty training batch is rejected",
        stats);
}

void testOneTrainingUpdate(Stats& stats)
{
    DevQNetwork online({.random_seed = 3U, .zero_initialize = true});
    DevQNetwork target({.random_seed = 4U, .zero_initialize = true});
    DevTrainingEngine engine(makeConfig(3));
    engine.reset(online, target);

    const auto before = online.parameters();

    std::vector<DevTransition> batch{
        makeTransition(DevAction::LONG, 0.50, true, 2000),
        makeTransition(DevAction::SHORT, -0.25, true, 2060)};

    const auto result = engine.trainBatch(batch, online, target);

    check(result.numerical_integrity,
          "Training result has numerical integrity", stats);
    check(result.batch_size == 2,
          "Training result reports batch size", stats);
    check(result.completed_training_updates == 1,
          "First successful batch counts as one update", stats);
    check(engine.completedTrainingUpdates() == 1,
          "Engine update counter advances", stats);
    check(!sameParameters(before, online.parameters()),
          "Successful batch updates online network", stats);
    check(!result.target_synchronized,
          "Target does not synchronize before configured boundary", stats);
    check(result.target_updates_since_synchronization == 1,
          "Target synchronization counter advances", stats);
}

void testSynchronizationOrderingAndBoundary(Stats& stats)
{
    DevQNetwork online({.random_seed = 5U, .zero_initialize = true});
    DevQNetwork target({.random_seed = 6U, .zero_initialize = true});
    DevTrainingEngine engine(makeConfig(2));
    engine.reset(online, target);

    std::vector<DevTransition> first{
        makeTransition(DevAction::LONG, 1.0, true, 3000)};

    const auto first_result = engine.trainBatch(first, online, target);

    check(!first_result.target_synchronized,
          "Update one leaves target frozen", stats);
    check(!sameParameters(online.parameters(), target.parameters()),
          "Online diverges from frozen target after update one", stats);

    std::vector<DevTransition> second{
        makeTransition(DevAction::SHORT, -0.5, true, 3060)};

    const auto second_result = engine.trainBatch(second, online, target);

    check(second_result.target_synchronized,
          "Update two synchronizes at exact boundary", stats);
    check(second_result.target_updates_since_synchronization == 0,
          "Synchronization resets interval counter", stats);
    check(sameParameters(online.parameters(), target.parameters()),
          "Boundary synchronization copies updated online network", stats);
    check(engine.completedTrainingUpdates() == 2,
          "Two successful batches produce two updates", stats);
}

void testNonTerminalBellmanPath(Stats& stats)
{
    DevQNetwork online({.random_seed = 7U, .zero_initialize = true});
    DevQNetwork target({.random_seed = 8U, .zero_initialize = true});

    auto target_parameters = target.parameters();
    target_parameters.biases3[static_cast<std::size_t>(DevAction::LONG)] = 2.0;
    target.setParameters(target_parameters);

    DevTrainingEngine engine(makeConfig(10));

    // reset intentionally copies online to target, so install the controlled
    // target value after reset to verify the non-terminal bootstrap path.
    engine.reset(online, target);
    target_parameters = target.parameters();
    target_parameters.biases3[static_cast<std::size_t>(DevAction::LONG)] = 2.0;
    target.setParameters(target_parameters);

    std::vector<DevTransition> batch{
        makeTransition(DevAction::HOLD, 1.0, false, 4000)};

    const auto result = engine.trainBatch(batch, online, target);

    const double expected_target = 1.0 + 0.99 * 2.0;
    check(std::fabs(result.mean_bellman_target - expected_target) < 1.0e-12,
          "Non-terminal training uses target-network Bellman bootstrap", stats);
    check(!result.target_synchronized,
          "Non-terminal test keeps target frozen before interval", stats);
}

void testResetAfterTraining(Stats& stats)
{
    DevQNetwork online({.random_seed = 9U, .zero_initialize = true});
    DevQNetwork target({.random_seed = 10U, .zero_initialize = true});
    DevTrainingEngine engine(makeConfig(5));
    engine.reset(online, target);

    std::vector<DevTransition> batch{
        makeTransition(DevAction::LONG, 0.5, true, 5000)};
    (void)engine.trainBatch(batch, online, target);

    check(engine.completedTrainingUpdates() == 1,
          "Precondition records one training update", stats);

    engine.reset(online, target);

    check(engine.completedTrainingUpdates() == 0,
          "Reset clears training-update counter", stats);
    check(sameParameters(online.parameters(), target.parameters()),
          "Reset re-synchronizes current online parameters", stats);
}

void testFrozenArchitecture(Stats& stats)
{
    DevQNetwork online({.random_seed = 12U, .zero_initialize = false});
    DevQNetwork target({.random_seed = 13U, .zero_initialize = false});

    check(online.parameterCount() == 7652,
          "Online network remains 7,652 parameters", stats);
    check(target.parameterCount() == 7652,
          "Target network remains 7,652 parameters", stats);
    check(DEV_Q_INPUT_SIZE == 84,
          "Frozen 84-value input remains unchanged", stats);
    check(DEV_Q_OUTPUT_SIZE == 4,
          "Frozen four-action output remains unchanged", stats);
}

} // namespace

int main()
{
    Stats stats{};

    std::cout
        << "\n======================================================================\n"
        << "DEVAI PHASE 3.8F — DEV TRAINING ENGINE TEST\n"
        << "======================================================================\n";

    try
    {
        testResetAndInitialization(stats);
        testRequiresResetAndNonEmptyBatch(stats);
        testOneTrainingUpdate(stats);
        testSynchronizationOrderingAndBoundary(stats);
        testNonTerminalBellmanPath(stats);
        testResetAfterTraining(stats);
        testFrozenArchitecture(stats);
    }
    catch (const std::exception& exception)
    {
        ++stats.failures;
        std::cerr << "[FATAL] " << exception.what() << '\n';
    }

    std::cout
        << "\n======================================================================\n"
        << "PHASE 3.8F TEST SUMMARY\n"
        << "======================================================================\n"
        << "Checks   : " << stats.checks << '\n'
        << "Failures : " << stats.failures << '\n'
        << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.8F DEV TRAINING ENGINE: PASSED\n"
            << "======================================================================\n";
        return 0;
    }

    std::cerr
        << "DEVAI PHASE 3.8F DEV TRAINING ENGINE: FAILED\n"
        << "======================================================================\n";
    return 1;
}
