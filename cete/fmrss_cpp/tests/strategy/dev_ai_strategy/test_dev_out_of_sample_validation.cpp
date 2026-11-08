#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"
#include "devai/strategy/dev_ai_strategy/DevOutOfSampleValidator.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
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
        description,
        stats);
}

bool parametersEqual(
    const DevQNetworkParameters& left,
    const DevQNetworkParameters& right)
{
    return
        left.weights1 == right.weights1 &&
        left.biases1 == right.biases1 &&
        left.weights2 == right.weights2 &&
        left.biases2 == right.biases2 &&
        left.weights3 == right.weights3 &&
        left.biases3 == right.biases3;
}

DevOutOfSamplePoint makePoint(
    const std::int64_t decision_time,
    const double offset)
{
    DevOutOfSamplePoint point;
    point.decision_time = decision_time;

    for (std::size_t i = 0;
         i < point.normalized_observation.size();
         ++i)
    {
        point.normalized_observation[i] =
            std::tanh(
                offset +
                static_cast<double>(
                    static_cast<int>(i % 13U) - 6) /
                10.0);
    }

    return point;
}

std::vector<DevQTrainingSample>
makeControlledTrainingBatch()
{
    std::vector<DevQTrainingSample> batch;
    batch.reserve(8U);

    for (std::size_t sample_index = 0;
         sample_index < 8U;
         ++sample_index)
    {
        DevQTrainingSample sample;

        // DevQNetwork::trainBatch consumes DevAIObservation, while this
        // controlled 3.10 test is intentionally independent of expensive
        // HistoricalCETEReplay.  We therefore do not use trainBatch here.
        // The function exists only to make the training/evaluation boundary
        // explicit in this test source.
        (void) sample;
    }

    return batch;
}

} // namespace

int main()
{
    TestStats stats;

    std::cout
        << "\n======================================================================\n"
        << "DEVAI PHASE 3.10 — INITIAL CONTROLLED OUT-OF-SAMPLE VALIDATION\n"
        << "======================================================================\n";

    require(
        DEV_Q_INPUT_SIZE == 84U,
        "Frozen OOS model input remains 84 values",
        stats);

    require(
        DEV_Q_OUTPUT_SIZE == 4U,
        "Frozen OOS model output remains four Dev actions",
        stats);

    // ------------------------------------------------------------------------
    // Controlled chronology.
    //
    // This subsection validates the OOS CONTRACT, not profitability:
    //
    // training period ends at 1,000
    // OOS begins strictly later at 1,060
    //
    // No HistoricalCETEReplay is invoked here.
    // ------------------------------------------------------------------------

    constexpr std::int64_t TRAINING_END_TIME =
        1000;

    std::vector<DevOutOfSamplePoint> oos_points{
        makePoint(1060, -0.30),
        makePoint(1120, -0.20),
        makePoint(1180, -0.10),
        makePoint(1240,  0.00),
        makePoint(1300,  0.10),
        makePoint(1360,  0.20),
        makePoint(1420,  0.30),
        makePoint(1480,  0.40)
    };

    require(
        oos_points.front().decision_time >
            TRAINING_END_TIME,
        "OOS sequence begins strictly after training boundary",
        stats);

    bool chronological = true;

    for (std::size_t i = 1;
         i < oos_points.size();
         ++i)
    {
        chronological =
            chronological &&
            oos_points[i].decision_time >
                oos_points[i - 1U].decision_time;
    }

    require(
        chronological,
        "Controlled OOS sequence is strictly chronological",
        stats);

    // ------------------------------------------------------------------------
    // Represent a trained model by deterministic non-default parameters.
    // 3.8 already validated the actual training engine.  3.10 must not spend
    // hours regenerating historical CETE state merely to retest training.
    // ------------------------------------------------------------------------

    DevQNetworkConfig trained_config;
    trained_config.random_seed = 3102026U;

    DevQNetwork trained_network(
        trained_config);

    auto trained_parameters =
        trained_network.parameters();

    trained_parameters.biases3[0] += 0.10;
    trained_parameters.biases3[1] += 0.20;
    trained_parameters.biases3[2] -= 0.15;
    trained_parameters.biases3[3] += 0.05;

    trained_network.setParameters(
        trained_parameters);

    const auto frozen_before =
        trained_network.parameters();

    require(
        trained_network.parameterCount() ==
            7652U,
        "Controlled trained model preserves 7,652 parameters",
        stats);

    // ------------------------------------------------------------------------
    // Phase 3.9 integration:
    // persist the model, load it into a fresh network, then OOS-evaluate only
    // the loaded frozen model.
    // ------------------------------------------------------------------------

    const std::filesystem::path model_path =
        std::filesystem::temp_directory_path() /
        "devai_phase_3_10_controlled_oos.devmodel";

    std::error_code error;
    std::filesystem::remove(
        model_path,
        error);

    DevModelPersistence persistence;

    const auto save_metadata =
        persistence.save(
            trained_network,
            model_path);

    require(
        save_metadata.numerical_integrity,
        "Controlled trained model saves with numerical integrity",
        stats);

    DevQNetwork loaded_network(
        DevQNetworkConfig{
            .random_seed = 1U,
            .zero_initialize = true
        });

    const auto load_metadata =
        persistence.load(
            loaded_network,
            model_path);

    require(
        load_metadata.numerical_integrity,
        "Persisted model reloads with numerical integrity",
        stats);

    require(
        parametersEqual(
            frozen_before,
            loaded_network.parameters()),
        "OOS model exactly matches persisted trained parameters",
        stats);

    const auto loaded_before_oos =
        loaded_network.parameters();

    DevOutOfSampleValidator validator;

    const DevOutOfSampleValidationResult result =
        validator.evaluate(
            loaded_network,
            TRAINING_END_TIME,
            oos_points);

    require(
        result.strict_temporal_separation,
        "Validator confirms strict train/OOS temporal separation",
        stats);

    require(
        result.training_end_time ==
            TRAINING_END_TIME,
        "Validator preserves exact training boundary",
        stats);

    require(
        result.oos_start_time ==
            oos_points.front().decision_time,
        "Validator records exact OOS start time",
        stats);

    require(
        result.oos_end_time ==
            oos_points.back().decision_time,
        "Validator records exact OOS end time",
        stats);

    require(
        result.evaluated_points ==
            oos_points.size(),
        "Every controlled OOS point is evaluated exactly once",
        stats);

    require(
        result.predictions.size() ==
            oos_points.size(),
        "Validator returns one prediction per OOS point",
        stats);

    require(
        result.network_unchanged,
        "OOS validation does not mutate frozen model",
        stats);

    require(
        parametersEqual(
            loaded_before_oos,
            loaded_network.parameters()),
        "Model parameters are bit-exact before and after OOS evaluation",
        stats);

    require(
        result.numerical_integrity,
        "Controlled OOS result has numerical integrity",
        stats);

    bool prediction_times_exact = true;
    bool predictions_finite = true;
    bool legal_actions = true;

    for (std::size_t i = 0;
         i < result.predictions.size();
         ++i)
    {
        const auto& prediction =
            result.predictions[i];

        prediction_times_exact =
            prediction_times_exact &&
            prediction.decision_time ==
                oos_points[i].decision_time;

        predictions_finite =
            predictions_finite &&
            prediction.numerical_integrity &&
            std::isfinite(
                prediction.maximum_q);

        for (const double q :
             prediction.q_values)
        {
            predictions_finite =
                predictions_finite &&
                std::isfinite(q);
        }

        legal_actions =
            legal_actions &&
            (prediction.greedy_action ==
                 DevAction::HOLD ||
             prediction.greedy_action ==
                 DevAction::LONG ||
             prediction.greedy_action ==
                 DevAction::SHORT ||
             prediction.greedy_action ==
                 DevAction::EXIT);
    }

    require(
        prediction_times_exact,
        "OOS predictions preserve exact chronological decision times",
        stats);

    require(
        predictions_finite,
        "All OOS Q-values are finite",
        stats);

    require(
        legal_actions,
        "All OOS greedy outputs map to legal Dev actions",
        stats);

    const std::size_t action_count =
        result.greedy_action_counts[0] +
        result.greedy_action_counts[1] +
        result.greedy_action_counts[2] +
        result.greedy_action_counts[3];

    require(
        action_count ==
            oos_points.size(),
        "OOS action distribution accounts for every evaluated point",
        stats);

    require(
        std::isfinite(
            result.mean_maximum_q) &&
        std::isfinite(
            result.minimum_maximum_q) &&
        std::isfinite(
            result.maximum_maximum_q),
        "OOS aggregate Q statistics are finite",
        stats);

    require(
        result.minimum_maximum_q <=
            result.mean_maximum_q &&
        result.mean_maximum_q <=
            result.maximum_maximum_q,
        "OOS aggregate Q statistics have valid ordering",
        stats);

    // ------------------------------------------------------------------------
    // Leakage / malformed-OOS rejection.
    // ------------------------------------------------------------------------

    auto overlapping =
        oos_points;

    overlapping.front().decision_time =
        TRAINING_END_TIME;

    expectThrow(
        [&]()
        {
            (void) validator.evaluate(
                loaded_network,
                TRAINING_END_TIME,
                overlapping);
        },
        "OOS point at training boundary is rejected",
        stats);

    auto backward =
        oos_points;

    backward[3].decision_time =
        backward[2].decision_time;

    expectThrow(
        [&]()
        {
            (void) validator.evaluate(
                loaded_network,
                TRAINING_END_TIME,
                backward);
        },
        "Non-increasing OOS chronology is rejected",
        stats);

    auto non_finite =
        oos_points;

    non_finite[0]
        .normalized_observation[0] =
        std::numeric_limits<double>::quiet_NaN();

    expectThrow(
        [&]()
        {
            (void) validator.evaluate(
                loaded_network,
                TRAINING_END_TIME,
                non_finite);
        },
        "Non-finite OOS observation is rejected",
        stats);

    expectThrow(
        [&]()
        {
            (void) validator.evaluate(
                loaded_network,
                TRAINING_END_TIME,
                {});
        },
        "Empty OOS sequence is rejected",
        stats);

    expectThrow(
        [&]()
        {
            (void) validator.evaluate(
                loaded_network,
                0,
                oos_points);
        },
        "Invalid training boundary is rejected",
        stats);

    // Persistence/evaluation must never mutate the original trained network.
    require(
        parametersEqual(
            frozen_before,
            trained_network.parameters()),
        "Original trained network remains unchanged throughout OOS validation",
        stats);

    std::filesystem::remove(
        model_path,
        error);

    std::cout
        << "\n----------------------------------------------------------------------\n"
        << "CONTROLLED OOS SUMMARY\n"
        << "----------------------------------------------------------------------\n"
        << "Training end time     : "
        << result.training_end_time
        << '\n'
        << "OOS start time        : "
        << result.oos_start_time
        << '\n'
        << "OOS end time          : "
        << result.oos_end_time
        << '\n'
        << "OOS points            : "
        << result.evaluated_points
        << '\n'
        << "Greedy HOLD           : "
        << result.greedy_action_counts[0]
        << '\n'
        << "Greedy LONG           : "
        << result.greedy_action_counts[1]
        << '\n'
        << "Greedy SHORT          : "
        << result.greedy_action_counts[2]
        << '\n'
        << "Greedy EXIT           : "
        << result.greedy_action_counts[3]
        << '\n'
        << "Mean maximum Q        : "
        << result.mean_maximum_q
        << '\n'
        << "Network unchanged     : "
        << (result.network_unchanged ? "YES" : "NO")
        << '\n';

    std::cout
        << "\n======================================================================\n"
        << "PHASE 3.10 INITIAL CONTROLLED OOS VALIDATION SUMMARY\n"
        << "======================================================================\n"
        << "Checks   : "
        << stats.checks
        << '\n'
        << "Failures : "
        << stats.failures
        << '\n'
        << "======================================================================\n";

    if (stats.failures == 0U)
    {
        std::cout
            << "DEVAI PHASE 3.10 INITIAL CONTROLLED OOS VALIDATION: PASSED\n";

        return 0;
    }

    std::cout
        << "DEVAI PHASE 3.10 INITIAL CONTROLLED OOS VALIDATION: FAILED\n";

    return 1;
}
