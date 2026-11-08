#include "devai/market/Candle.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevTrainingEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace devai::market;
using namespace devai::strategy;

constexpr const char* DATA_FOLDER = "/home/hadoop/shareMarket_Data/minute";
constexpr const char* BENCHMARK_SYMBOL = "NIFTY%2050";
constexpr std::size_t MINIMUM_UNIVERSE_SIZE = 2;
constexpr std::size_t REAL_STATE_COUNT = 20;
constexpr std::size_t EXPECTED_TRANSITION_COUNT = REAL_STATE_COUNT - 1;
constexpr std::size_t TRAINING_BATCH_SIZE = 8;
constexpr std::size_t TRAINING_UPDATES = 6;
constexpr std::int64_t ONE_MINUTE_SECONDS = 60;

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
    std::size_t replay_points{0};
    std::size_t lookahead_violations{0};
    std::size_t chronology_violations{0};
    std::size_t observation_violations{0};
    std::size_t training_updates{0};
};

void require(bool condition, const std::string& description, TestStats& stats)
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

bool parametersEqual(const DevQNetworkParameters& a, const DevQNetworkParameters& b)
{
    return a.weights1 == b.weights1 && a.biases1 == b.biases1 &&
           a.weights2 == b.weights2 && a.biases2 == b.biases2 &&
           a.weights3 == b.weights3 && a.biases3 == b.biases3;
}

bool finiteParameters(const DevQNetworkParameters& p)
{
    const auto finite_vector = [](const std::vector<double>& values)
    {
        return std::all_of(values.begin(), values.end(),
                           [](double value) { return std::isfinite(value); });
    };

    return finite_vector(p.weights1) && finite_vector(p.biases1) &&
           finite_vector(p.weights2) && finite_vector(p.biases2) &&
           finite_vector(p.weights3) && finite_vector(p.biases3);
}

} // namespace

int main()
{
    TestStats stats{};

    std::cout
        << "\n======================================================================\n"
        << "DEVAI PHASE 3.8G — REAL HISTORICAL TRAINING VALIDATION\n"
        << "======================================================================\n";

    try
    {
        HistoricalCETEReplayConfig replay_config;
        replay_config.data_folder = DATA_FOLDER;
        replay_config.benchmark_symbol = BENCHMARK_SYMBOL;
        replay_config.minimum_universe_size = MINIMUM_UNIVERSE_SIZE;
        replay_config.maximum_stock_count = 0;

        HistoricalCETEReplay replay(replay_config);
        const auto symbols = replay.discoverSymbols();

        require(symbols.size() >= MINIMUM_UNIVERSE_SIZE,
                "Historical replay discovered sufficient real stocks", stats);
        require(std::find(symbols.begin(), symbols.end(), BENCHMARK_SYMBOL) == symbols.end(),
                "Benchmark is excluded from tradable stock universe", stats);

        if (symbols.empty())
            throw std::runtime_error("No tradable real symbols discovered.");

        const std::string traded_symbol = symbols.front();

        std::cout << "\nSelected validation symbol : " << traded_symbol
                  << "\nBenchmark                  : " << BENCHMARK_SYMBOL
                  << "\nRequired real states       : " << REAL_STATE_COUNT
                  << "\nExpected transitions       : " << EXPECTED_TRANSITION_COUNT
                  << "\nTraining batch size        : " << TRAINING_BATCH_SIZE
                  << "\nTraining updates           : " << TRAINING_UPDATES << "\n\n";

        MarketDataLoader loader(DATA_FOLDER);
        const std::filesystem::path stock_path =
            std::filesystem::path(DATA_FOLDER) / (traded_symbol + "_1min.txt");

        require(std::filesystem::exists(stock_path),
                "Real traded-stock 1-minute file exists", stats);

        const auto stock_load = loader.loadPath(stock_path);
        require(stock_load.candles.size() > REAL_STATE_COUNT,
                "Real 1-minute history contains sufficient candles", stats);

        const std::int64_t latest_common_time = replay.latestCommonDecisionTime();
        require(latest_common_time > 0,
                "Latest common historical CETE decision time is valid", stats);

        std::vector<const Candle*> candidate_candles;
        for (auto iterator = stock_load.candles.rbegin();
             iterator != stock_load.candles.rend(); ++iterator)
        {
            const std::int64_t decision_time = iterator->timestamp + ONE_MINUTE_SECONDS;
            if (decision_time > latest_common_time)
                continue;

            candidate_candles.push_back(&(*iterator));
            if (candidate_candles.size() == REAL_STATE_COUNT)
                break;
        }

        require(candidate_candles.size() == REAL_STATE_COUNT,
                "Selected exactly 20 genuine historical states", stats);
        std::reverse(candidate_candles.begin(), candidate_candles.end());

        std::vector<DevEpisode::Snapshot> snapshots;
        snapshots.reserve(candidate_candles.size());

        std::int64_t previous_time = 0;
        for (const Candle* candle : candidate_candles)
        {
            const std::int64_t decision_time = candle->timestamp + ONE_MINUTE_SECONDS;
            if (previous_time != 0 && decision_time <= previous_time)
                ++stats.chronology_violations;

            const auto point = replay.replayAt(decision_time);
            ++stats.replay_points;
            stats.lookahead_violations += point.stats.lookahead_violations;

            require(point.snapshot.decision_time == decision_time,
                    "Real replay snapshot matches requested decision time", stats);
            require(point.snapshot.benchmark_symbol == BENCHMARK_SYMBOL,
                    "Real replay snapshot preserves benchmark identity", stats);
            require(point.snapshot.stock_count >= MINIMUM_UNIVERSE_SIZE,
                    "Real replay snapshot contains sufficient stocks", stats);
            require(point.snapshot.numerical_integrity,
                    "Real replay snapshot has numerical integrity", stats);
            require(point.stats.lookahead_violations == 0,
                    "Historical CETE replay reports zero look-ahead violations", stats);

            snapshots.push_back(point.snapshot);
            previous_time = decision_time;
        }

        require(stats.replay_points == REAL_STATE_COUNT,
                "Exactly 20 real CETE replay points were reconstructed", stats);
        require(stats.lookahead_violations == 0,
                "Real historical reconstruction has zero look-ahead violations", stats);
        require(stats.chronology_violations == 0,
                "Real historical snapshot sequence is strictly chronological", stats);

        ObservationBuilder observation_builder;
        DevPortfolioState flat_portfolio{};
        std::vector<DevAIObservation> observations;
        observations.reserve(snapshots.size());

        for (const auto& snapshot : snapshots)
        {
            const DevAIObservation observation =
                observation_builder.build(snapshot, traded_symbol, flat_portfolio);

            if (!observation.numerical_integrity || !observation.ready)
                ++stats.observation_violations;

            observations.push_back(observation);
        }

        require(observations.size() == REAL_STATE_COUNT,
                "Built one Dev observation for every real historical state", stats);
        require(stats.observation_violations == 0,
                "All real historical Dev observations are ready and numerically valid", stats);

        // Phase 3.6B already validates CETE reward/accounting generation on real
        // history. 3.8G deliberately validates the learning path using those same
        // causal state semantics without inventing future labels. A HOLD/flat
        // validation transition therefore carries the correct immediate reward 0.
        std::vector<DevTransition> transitions;
        transitions.reserve(EXPECTED_TRANSITION_COUNT);

        for (std::size_t index = 0; index < EXPECTED_TRANSITION_COUNT; ++index)
        {
            DevTransition transition{};
            transition.observation = observations[index];
            transition.action = DevAction::HOLD;
            transition.reward = 0.0;
            transition.next_observation = observations[index + 1];
            transition.terminal = (index + 1 == EXPECTED_TRANSITION_COUNT);
            transition.decision_time = snapshots[index].decision_time;
            transition.next_decision_time = snapshots[index + 1].decision_time;
            transitions.push_back(transition);
        }

        require(transitions.size() == EXPECTED_TRANSITION_COUNT,
                "20 real states produce exactly 19 causal training transitions", stats);
        require(transitions.back().terminal,
                "Final real historical training transition is terminal", stats);

        bool transition_integrity = true;
        for (const auto& transition : transitions)
        {
            transition_integrity = transition_integrity &&
                transition.observation.numerical_integrity && transition.observation.ready &&
                transition.next_observation.numerical_integrity && transition.next_observation.ready &&
                std::isfinite(transition.reward) &&
                transition.next_decision_time > transition.decision_time;
        }
        require(transition_integrity,
                "All real historical training transitions are causal and finite", stats);

        DevQNetwork online_network({.random_seed = 3807U, .zero_initialize = false});
        DevQNetwork target_network({.random_seed = 8307U, .zero_initialize = false});

        require(!parametersEqual(online_network.parameters(), target_network.parameters()),
                "Online and target networks begin independently initialized", stats);

        DevTrainingEngineConfig training_config;
        training_config.mini_batch.bellman.discount_factor = 0.99;
        training_config.mini_batch.training.learning_rate = 0.001;
        training_config.mini_batch.training.huber_delta = 1.0;
        training_config.mini_batch.training.gradient_clip = 10.0;
        training_config.target_synchronization.synchronization_interval = 3;

        DevTrainingEngine training_engine(training_config);
        training_engine.reset(online_network, target_network);

        require(training_engine.initialized(),
                "DevTrainingEngine initializes for real historical training", stats);
        require(parametersEqual(online_network.parameters(), target_network.parameters()),
                "Training reset hard-synchronizes target from online", stats);

        const DevQNetworkParameters initial_online = online_network.parameters();

        for (std::size_t update = 0; update < TRAINING_UPDATES; ++update)
        {
            std::vector<DevTransition> batch;
            batch.reserve(TRAINING_BATCH_SIZE);

            const std::size_t start =
                (update * TRAINING_BATCH_SIZE) % transitions.size();

            for (std::size_t offset = 0; offset < TRAINING_BATCH_SIZE; ++offset)
                batch.push_back(transitions[(start + offset) % transitions.size()]);

            const DevTrainingStepResult result =
                training_engine.trainBatch(batch, online_network, target_network);

            ++stats.training_updates;

            require(result.numerical_integrity,
                    "Real historical training update has numerical integrity", stats);
            require(result.batch_size == TRAINING_BATCH_SIZE,
                    "Real historical training update preserves batch size", stats);
            require(std::isfinite(result.mean_bellman_target),
                    "Real historical mean Bellman target is finite", stats);
            require(std::isfinite(result.mean_loss) && result.mean_loss >= 0.0,
                    "Real historical mean training loss is finite and non-negative", stats);
            require(std::isfinite(result.mean_td_error),
                    "Real historical mean TD error is finite", stats);
            require(std::isfinite(result.gradient_l2_norm),
                    "Real historical gradient norm is finite", stats);
            require(result.completed_training_updates == update + 1,
                    "Training update counter remains exact", stats);

            const bool expected_sync = ((update + 1) % 3 == 0);
            require(result.target_synchronized == expected_sync,
                    "Target synchronization occurs only at configured boundary", stats);
        }

        require(stats.training_updates == TRAINING_UPDATES,
                "Completed all requested real historical training updates", stats);
        require(training_engine.completedTrainingUpdates() == TRAINING_UPDATES,
                "DevTrainingEngine records all real historical updates", stats);
        require(!parametersEqual(initial_online, online_network.parameters()),
                "Real historical training changes online Q-network parameters", stats);
        require(finiteParameters(online_network.parameters()),
                "Online Q-network remains numerically finite after real training", stats);
        require(finiteParameters(target_network.parameters()),
                "Target Q-network remains numerically finite after real training", stats);
        require(parametersEqual(online_network.parameters(), target_network.parameters()),
                "Final configured synchronization leaves target equal to online", stats);
        require(online_network.parameterCount() == 7652 && target_network.parameterCount() == 7652,
                "Real historical training preserves frozen 7,652-parameter architecture", stats);
        require(DEV_Q_INPUT_SIZE == 84,
                "Real historical training preserves frozen 84-value input", stats);
        require(DEV_Q_OUTPUT_SIZE == 4,
                "Real historical training preserves four Dev actions", stats);

        std::cout
            << "\n======================================================================\n"
            << "PHASE 3.8G REAL HISTORICAL TRAINING SUMMARY\n"
            << "======================================================================\n"
            << "Traded symbol               : " << traded_symbol << '\n'
            << "Benchmark                   : " << BENCHMARK_SYMBOL << '\n'
            << "Real CETE states            : " << snapshots.size() << '\n'
            << "Real training transitions   : " << transitions.size() << '\n'
            << "Training updates            : " << stats.training_updates << '\n'
            << "Look-ahead violations       : " << stats.lookahead_violations << '\n'
            << "Chronology violations       : " << stats.chronology_violations << '\n'
            << "Observation violations      : " << stats.observation_violations << '\n'
            << "Checks                      : " << stats.checks << '\n'
            << "Failures                    : " << stats.failures << '\n'
            << "======================================================================\n";
    }
    catch (const std::exception& exception)
    {
        ++stats.failures;
        std::cerr << "\n[FATAL] " << exception.what() << '\n';
    }
    catch (...)
    {
        ++stats.failures;
        std::cerr << "\n[FATAL] Unknown unexpected exception.\n";
    }

    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.8G REAL HISTORICAL TRAINING VALIDATION: PASSED\n"
            << "======================================================================\n";
        return 0;
    }

    std::cerr
        << "DEVAI PHASE 3.8G REAL HISTORICAL TRAINING VALIDATION: FAILED\n"
        << "======================================================================\n";
    return 1;
}
