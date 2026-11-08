#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>
#include <stdexcept>

namespace devai::strategy
{
namespace
{

std::size_t index2D(
    const std::size_t row,
    const std::size_t column,
    const std::size_t column_count) noexcept
{
    return row * column_count + column;
}

std::size_t checkedActionIndex(const DevAction action)
{
    switch (action)
    {
        case DevAction::HOLD:  return 0;
        case DevAction::LONG:  return 1;
        case DevAction::SHORT: return 2;
        case DevAction::EXIT:  return 3;
    }

    throw std::invalid_argument("DevQNetwork: invalid DevAction.");
}

bool allFinite(const std::vector<double>& values) noexcept
{
    return std::all_of(
        values.begin(),
        values.end(),
        [](const double value)
        {
            return std::isfinite(value);
        });
}

} // namespace

double DevQValues::value(const DevAction action) const
{
    if (!numerical_integrity)
    {
        throw std::runtime_error(
            "DevQValues: numerical integrity is false.");
    }

    return values.at(checkedActionIndex(action));
}

DevAction DevQValues::greedyAction() const
{
    if (!numerical_integrity)
    {
        throw std::runtime_error(
            "DevQValues: numerical integrity is false.");
    }

    const auto iterator =
        std::max_element(values.begin(), values.end());

    const std::size_t index =
        static_cast<std::size_t>(
            std::distance(values.begin(), iterator));

    switch (index)
    {
        case 0: return DevAction::HOLD;
        case 1: return DevAction::LONG;
        case 2: return DevAction::SHORT;
        case 3: return DevAction::EXIT;
        default: break;
    }

    throw std::runtime_error(
        "DevQValues: invalid greedy-action index.");
}

double DevQValues::maximumValue() const
{
    if (!numerical_integrity)
    {
        throw std::runtime_error(
            "DevQValues: numerical integrity is false.");
    }

    return *std::max_element(values.begin(), values.end());
}

DevQNetwork::DevQNetwork(DevQNetworkConfig config)
    : config_(config)
{
    // Preserve the frozen Phase-3.7 storage layout exactly.
    parameters_.weights1.resize(
        DEV_Q_HIDDEN1_SIZE * DEV_Q_INPUT_SIZE);
    parameters_.biases1.resize(DEV_Q_HIDDEN1_SIZE);

    parameters_.weights2.resize(
        DEV_Q_HIDDEN2_SIZE * DEV_Q_HIDDEN1_SIZE);
    parameters_.biases2.resize(DEV_Q_HIDDEN2_SIZE);

    parameters_.weights3.resize(
        DEV_Q_OUTPUT_SIZE * DEV_Q_HIDDEN2_SIZE);
    parameters_.biases3.resize(DEV_Q_OUTPUT_SIZE);

    if (config_.zero_initialize)
    {
        return;
    }

    std::mt19937 generator(config_.random_seed);

    const double standard_deviation1 =
        std::sqrt(
            2.0 /
            static_cast<double>(DEV_Q_INPUT_SIZE));

    std::normal_distribution<double>
        distribution1(0.0, standard_deviation1);

    for (double& weight : parameters_.weights1)
    {
        weight = distribution1(generator);
    }

    const double standard_deviation2 =
        std::sqrt(
            2.0 /
            static_cast<double>(DEV_Q_HIDDEN1_SIZE));

    std::normal_distribution<double>
        distribution2(0.0, standard_deviation2);

    for (double& weight : parameters_.weights2)
    {
        weight = distribution2(generator);
    }

    const double standard_deviation3 =
        std::sqrt(
            1.0 /
            static_cast<double>(DEV_Q_HIDDEN2_SIZE));

    std::normal_distribution<double>
        distribution3(0.0, standard_deviation3);

    for (double& weight : parameters_.weights3)
    {
        weight = distribution3(generator);
    }
}

DevQValues DevQNetwork::predict(
    const DevAIObservation& observation) const
{
    return predict(observationToInput(observation));
}

DevQValues DevQNetwork::predict(
    const std::array<double, DEV_Q_INPUT_SIZE>& input) const
{
    validateInput(input);
    validateParameters(parameters_);

    std::array<double, DEV_Q_HIDDEN1_SIZE> hidden1{};

    for (std::size_t row = 0;
         row < DEV_Q_HIDDEN1_SIZE;
         ++row)
    {
        double activation = parameters_.biases1[row];

        for (std::size_t column = 0;
             column < DEV_Q_INPUT_SIZE;
             ++column)
        {
            activation +=
                parameters_.weights1[
                    index2D(
                        row,
                        column,
                        DEV_Q_INPUT_SIZE)] *
                input[column];
        }

        if (!std::isfinite(activation))
        {
            throw std::runtime_error(
                "DevQNetwork: non-finite hidden-layer-1 activation.");
        }

        hidden1[row] = relu(activation);
    }

    std::array<double, DEV_Q_HIDDEN2_SIZE> hidden2{};

    for (std::size_t row = 0;
         row < DEV_Q_HIDDEN2_SIZE;
         ++row)
    {
        double activation = parameters_.biases2[row];

        for (std::size_t column = 0;
             column < DEV_Q_HIDDEN1_SIZE;
             ++column)
        {
            activation +=
                parameters_.weights2[
                    index2D(
                        row,
                        column,
                        DEV_Q_HIDDEN1_SIZE)] *
                hidden1[column];
        }

        if (!std::isfinite(activation))
        {
            throw std::runtime_error(
                "DevQNetwork: non-finite hidden-layer-2 activation.");
        }

        hidden2[row] = relu(activation);
    }

    DevQValues result{};

    for (std::size_t row = 0;
         row < DEV_Q_OUTPUT_SIZE;
         ++row)
    {
        double q_value = parameters_.biases3[row];

        for (std::size_t column = 0;
             column < DEV_Q_HIDDEN2_SIZE;
             ++column)
        {
            q_value +=
                parameters_.weights3[
                    index2D(
                        row,
                        column,
                        DEV_Q_HIDDEN2_SIZE)] *
                hidden2[column];
        }

        if (!std::isfinite(q_value))
        {
            throw std::runtime_error(
                "DevQNetwork: non-finite Q-value.");
        }

        result.values[row] = q_value;
    }

    result.numerical_integrity = true;
    return result;
}

DevQTrainingResult DevQNetwork::trainSingle(
    const DevAIObservation& observation,
    const DevAction action,
    const double target_q,
    const DevQTrainingConfig& training_config)
{
    validateTrainingConfig(training_config);

    if (!std::isfinite(target_q))
    {
        throw std::invalid_argument(
            "DevQNetwork::trainSingle: target Q-value must be finite.");
    }

    const std::size_t selected_action =
        actionIndex(action);

    const auto input =
        observationToInput(observation);

    validateParameters(parameters_);

    // Forward pass with cached pre/post activations.
    std::array<double, DEV_Q_HIDDEN1_SIZE> z1{};
    std::array<double, DEV_Q_HIDDEN1_SIZE> a1{};

    for (std::size_t row = 0;
         row < DEV_Q_HIDDEN1_SIZE;
         ++row)
    {
        double activation = parameters_.biases1[row];

        for (std::size_t column = 0;
             column < DEV_Q_INPUT_SIZE;
             ++column)
        {
            activation +=
                parameters_.weights1[
                    index2D(
                        row,
                        column,
                        DEV_Q_INPUT_SIZE)] *
                input[column];
        }

        if (!std::isfinite(activation))
        {
            throw std::runtime_error(
                "DevQNetwork::trainSingle: non-finite layer-1 activation.");
        }

        z1[row] = activation;
        a1[row] = relu(activation);
    }

    std::array<double, DEV_Q_HIDDEN2_SIZE> z2{};
    std::array<double, DEV_Q_HIDDEN2_SIZE> a2{};

    for (std::size_t row = 0;
         row < DEV_Q_HIDDEN2_SIZE;
         ++row)
    {
        double activation = parameters_.biases2[row];

        for (std::size_t column = 0;
             column < DEV_Q_HIDDEN1_SIZE;
             ++column)
        {
            activation +=
                parameters_.weights2[
                    index2D(
                        row,
                        column,
                        DEV_Q_HIDDEN1_SIZE)] *
                a1[column];
        }

        if (!std::isfinite(activation))
        {
            throw std::runtime_error(
                "DevQNetwork::trainSingle: non-finite layer-2 activation.");
        }

        z2[row] = activation;
        a2[row] = relu(activation);
    }

    std::array<double, DEV_Q_OUTPUT_SIZE> output{};

    for (std::size_t row = 0;
         row < DEV_Q_OUTPUT_SIZE;
         ++row)
    {
        double q_value = parameters_.biases3[row];

        for (std::size_t column = 0;
             column < DEV_Q_HIDDEN2_SIZE;
             ++column)
        {
            q_value +=
                parameters_.weights3[
                    index2D(
                        row,
                        column,
                        DEV_Q_HIDDEN2_SIZE)] *
                a2[column];
        }

        if (!std::isfinite(q_value))
        {
            throw std::runtime_error(
                "DevQNetwork::trainSingle: non-finite Q-value.");
        }

        output[row] = q_value;
    }

    const double predicted_before =
        output[selected_action];

    // TD error is reported as target - prediction.
    const double td_error =
        target_q - predicted_before;

    if (!std::isfinite(td_error))
    {
        throw std::runtime_error(
            "DevQNetwork::trainSingle: non-finite TD error.");
    }

    const double loss =
        huberLoss(
            td_error,
            training_config.huber_delta);

    // dL/dPrediction uses prediction-target.
    const double output_gradient =
        huberDerivative(
            predicted_before - target_q,
            training_config.huber_delta);

    if (!std::isfinite(loss) ||
        !std::isfinite(output_gradient))
    {
        throw std::runtime_error(
            "DevQNetwork::trainSingle: non-finite loss/gradient.");
    }

    DevQNetworkParameters gradients{};
    gradients.weights1.assign(parameters_.weights1.size(), 0.0);
    gradients.biases1.assign(parameters_.biases1.size(), 0.0);
    gradients.weights2.assign(parameters_.weights2.size(), 0.0);
    gradients.biases2.assign(parameters_.biases2.size(), 0.0);
    gradients.weights3.assign(parameters_.weights3.size(), 0.0);
    gradients.biases3.assign(parameters_.biases3.size(), 0.0);

    // Output layer: only selected Q(S,A) receives direct loss.
    gradients.biases3[selected_action] =
        output_gradient;

    for (std::size_t hidden2 = 0;
         hidden2 < DEV_Q_HIDDEN2_SIZE;
         ++hidden2)
    {
        gradients.weights3[
            index2D(
                selected_action,
                hidden2,
                DEV_Q_HIDDEN2_SIZE)] =
            a2[hidden2] * output_gradient;
    }

    // Hidden layer 2.
    std::array<double, DEV_Q_HIDDEN2_SIZE> delta2{};

    for (std::size_t hidden2 = 0;
         hidden2 < DEV_Q_HIDDEN2_SIZE;
         ++hidden2)
    {
        const double propagated =
            parameters_.weights3[
                index2D(
                    selected_action,
                    hidden2,
                    DEV_Q_HIDDEN2_SIZE)] *
            output_gradient;

        delta2[hidden2] =
            propagated *
            (z2[hidden2] > 0.0 ? 1.0 : 0.0);

        gradients.biases2[hidden2] =
            delta2[hidden2];

        for (std::size_t hidden1 = 0;
             hidden1 < DEV_Q_HIDDEN1_SIZE;
             ++hidden1)
        {
            gradients.weights2[
                index2D(
                    hidden2,
                    hidden1,
                    DEV_Q_HIDDEN1_SIZE)] =
                a1[hidden1] * delta2[hidden2];
        }
    }

    // Hidden layer 1.
    std::array<double, DEV_Q_HIDDEN1_SIZE> delta1{};

    for (std::size_t hidden1 = 0;
         hidden1 < DEV_Q_HIDDEN1_SIZE;
         ++hidden1)
    {
        double propagated = 0.0;

        for (std::size_t hidden2 = 0;
             hidden2 < DEV_Q_HIDDEN2_SIZE;
             ++hidden2)
        {
            propagated +=
                parameters_.weights2[
                    index2D(
                        hidden2,
                        hidden1,
                        DEV_Q_HIDDEN1_SIZE)] *
                delta2[hidden2];
        }

        delta1[hidden1] =
            propagated *
            (z1[hidden1] > 0.0 ? 1.0 : 0.0);

        gradients.biases1[hidden1] =
            delta1[hidden1];

        for (std::size_t input_index = 0;
             input_index < DEV_Q_INPUT_SIZE;
             ++input_index)
        {
            gradients.weights1[
                index2D(
                    hidden1,
                    input_index,
                    DEV_Q_INPUT_SIZE)] =
                input[input_index] * delta1[hidden1];
        }
    }

    double gradient_squared_sum = 0.0;
    double clipped_gradient_squared_sum = 0.0;

    auto validate_and_clip =
        [&](std::vector<double>& values)
        {
            for (double& gradient : values)
            {
                if (!std::isfinite(gradient))
                {
                    throw std::runtime_error(
                        "DevQNetwork::trainSingle: non-finite gradient.");
                }

                gradient_squared_sum +=
                    gradient * gradient;

                gradient =
                    clipGradient(
                        gradient,
                        training_config.gradient_clip);

                clipped_gradient_squared_sum +=
                    gradient * gradient;
            }
        };

    validate_and_clip(gradients.weights1);
    validate_and_clip(gradients.biases1);
    validate_and_clip(gradients.weights2);
    validate_and_clip(gradients.biases2);
    validate_and_clip(gradients.weights3);
    validate_and_clip(gradients.biases3);

    const double gradient_l2_norm =
        std::sqrt(gradient_squared_sum);

    const double clipped_gradient_l2_norm =
        std::sqrt(clipped_gradient_squared_sum);

    if (!std::isfinite(gradient_l2_norm) ||
        !std::isfinite(clipped_gradient_l2_norm))
    {
        throw std::runtime_error(
            "DevQNetwork::trainSingle: non-finite gradient norm.");
    }

    // All gradients were calculated before any parameter mutation.
    auto apply_update =
        [&](std::vector<double>& parameter_values,
            const std::vector<double>& gradient_values)
        {
            if (parameter_values.size() != gradient_values.size())
            {
                throw std::runtime_error(
                    "DevQNetwork::trainSingle: parameter/gradient mismatch.");
            }

            for (std::size_t index = 0;
                 index < parameter_values.size();
                 ++index)
            {
                const double updated =
                    parameter_values[index] -
                    training_config.learning_rate *
                    gradient_values[index];

                if (!std::isfinite(updated))
                {
                    throw std::runtime_error(
                        "DevQNetwork::trainSingle: non-finite parameter update.");
                }

                parameter_values[index] = updated;
            }
        };

    apply_update(parameters_.weights1, gradients.weights1);
    apply_update(parameters_.biases1, gradients.biases1);
    apply_update(parameters_.weights2, gradients.weights2);
    apply_update(parameters_.biases2, gradients.biases2);
    apply_update(parameters_.weights3, gradients.weights3);
    apply_update(parameters_.biases3, gradients.biases3);

    const DevQValues after_values =
        predict(observation);

    const double predicted_after =
        after_values.value(action);

    DevQTrainingResult result{};
    result.action = action;
    result.target_q = target_q;
    result.predicted_q_before = predicted_before;
    result.predicted_q_after = predicted_after;
    result.td_error = td_error;
    result.loss = loss;
    result.gradient_l2_norm = gradient_l2_norm;
    result.clipped_gradient_l2_norm =
        clipped_gradient_l2_norm;

    result.numerical_integrity =
        std::isfinite(result.target_q) &&
        std::isfinite(result.predicted_q_before) &&
        std::isfinite(result.predicted_q_after) &&
        std::isfinite(result.td_error) &&
        std::isfinite(result.loss) &&
        std::isfinite(result.gradient_l2_norm) &&
        std::isfinite(result.clipped_gradient_l2_norm);

    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "DevQNetwork::trainSingle: invalid training result.");
    }

    return result;
}


// ============================================================================
// Phase 3.8C — True Mini-Batch Training
// ============================================================================

DevQBatchTrainingResult DevQNetwork::trainBatch(
    const std::vector<DevQTrainingSample>& batch,
    const DevQTrainingConfig& training_config)
{
    validateTrainingConfig(training_config);

    if (batch.empty())
    {
        throw std::invalid_argument(
            "DevQNetwork::trainBatch: batch must not be empty.");
    }

    const DevQNetworkParameters original =
        parameters();

    DevQNetworkParameters accumulated{};
    accumulated.weights1.assign(original.weights1.size(), 0.0);
    accumulated.biases1.assign(original.biases1.size(), 0.0);
    accumulated.weights2.assign(original.weights2.size(), 0.0);
    accumulated.biases2.assign(original.biases2.size(), 0.0);
    accumulated.weights3.assign(original.weights3.size(), 0.0);
    accumulated.biases3.assign(original.biases3.size(), 0.0);

    double loss_sum = 0.0;
    double td_error_sum = 0.0;
    double absolute_td_error_sum = 0.0;

    // We use the already-validated 3.8B single-sample backpropagation as the
    // gradient oracle. Each temporary network starts from exactly the same
    // original parameter snapshot. With learning_rate=1 and clipping disabled:
    //
    //     p_after = p_before - gradient
    //
    // therefore:
    //
    //     gradient = p_before - p_after
    //
    // This preserves one authoritative backpropagation implementation while
    // guaranteeing true batch averaging rather than sequential SGD.
    DevQTrainingConfig gradient_config = training_config;
    gradient_config.learning_rate = 1.0;
    gradient_config.gradient_clip = 0.0;

    auto accumulate_difference =
        [](std::vector<double>& destination,
           const std::vector<double>& before,
           const std::vector<double>& after)
        {
            if (destination.size() != before.size() ||
                before.size() != after.size())
            {
                throw std::runtime_error(
                    "DevQNetwork::trainBatch: parameter dimension mismatch.");
            }

            for (std::size_t index = 0;
                 index < destination.size();
                 ++index)
            {
                const double gradient =
                    before[index] - after[index];

                if (!std::isfinite(gradient))
                {
                    throw std::runtime_error(
                        "DevQNetwork::trainBatch: non-finite sample gradient.");
                }

                destination[index] += gradient;

                if (!std::isfinite(destination[index]))
                {
                    throw std::runtime_error(
                        "DevQNetwork::trainBatch: non-finite accumulated gradient.");
                }
            }
        };

    for (const DevQTrainingSample& sample : batch)
    {
        if (!std::isfinite(sample.target_q))
        {
            throw std::invalid_argument(
                "DevQNetwork::trainBatch: target Q-value must be finite.");
        }

        DevQNetwork temporary(
            DevQNetworkConfig{
                .random_seed = config_.random_seed,
                .zero_initialize = true});

        temporary.setParameters(original);

        const DevQTrainingResult sample_result =
            temporary.trainSingle(
                sample.observation,
                sample.action,
                sample.target_q,
                gradient_config);

        if (!sample_result.numerical_integrity)
        {
            throw std::runtime_error(
                "DevQNetwork::trainBatch: sample training lost numerical integrity.");
        }

        loss_sum += sample_result.loss;
        td_error_sum += sample_result.td_error;
        absolute_td_error_sum +=
            std::abs(sample_result.td_error);

        if (!std::isfinite(loss_sum) ||
            !std::isfinite(td_error_sum) ||
            !std::isfinite(absolute_td_error_sum))
        {
            throw std::runtime_error(
                "DevQNetwork::trainBatch: non-finite batch metric.");
        }

        const DevQNetworkParameters after =
            temporary.parameters();

        accumulate_difference(
            accumulated.weights1,
            original.weights1,
            after.weights1);

        accumulate_difference(
            accumulated.biases1,
            original.biases1,
            after.biases1);

        accumulate_difference(
            accumulated.weights2,
            original.weights2,
            after.weights2);

        accumulate_difference(
            accumulated.biases2,
            original.biases2,
            after.biases2);

        accumulate_difference(
            accumulated.weights3,
            original.weights3,
            after.weights3);

        accumulate_difference(
            accumulated.biases3,
            original.biases3,
            after.biases3);
    }

    const double inverse_batch_size =
        1.0 / static_cast<double>(batch.size());

    double gradient_squared_sum = 0.0;
    double clipped_gradient_squared_sum = 0.0;

    auto average_validate_and_clip =
        [&](std::vector<double>& values)
        {
            for (double& gradient : values)
            {
                gradient *= inverse_batch_size;

                if (!std::isfinite(gradient))
                {
                    throw std::runtime_error(
                        "DevQNetwork::trainBatch: non-finite averaged gradient.");
                }

                gradient_squared_sum +=
                    gradient * gradient;

                gradient =
                    clipGradient(
                        gradient,
                        training_config.gradient_clip);

                clipped_gradient_squared_sum +=
                    gradient * gradient;
            }
        };

    average_validate_and_clip(accumulated.weights1);
    average_validate_and_clip(accumulated.biases1);
    average_validate_and_clip(accumulated.weights2);
    average_validate_and_clip(accumulated.biases2);
    average_validate_and_clip(accumulated.weights3);
    average_validate_and_clip(accumulated.biases3);

    const double gradient_l2_norm =
        std::sqrt(gradient_squared_sum);

    const double clipped_gradient_l2_norm =
        std::sqrt(clipped_gradient_squared_sum);

    if (!std::isfinite(gradient_l2_norm) ||
        !std::isfinite(clipped_gradient_l2_norm))
    {
        throw std::runtime_error(
            "DevQNetwork::trainBatch: non-finite gradient norm.");
    }

    DevQNetworkParameters updated =
        original;

    auto apply_update =
        [&](std::vector<double>& parameter_values,
            const std::vector<double>& gradients)
        {
            if (parameter_values.size() != gradients.size())
            {
                throw std::runtime_error(
                    "DevQNetwork::trainBatch: update dimension mismatch.");
            }

            for (std::size_t index = 0;
                 index < parameter_values.size();
                 ++index)
            {
                parameter_values[index] -=
                    training_config.learning_rate *
                    gradients[index];

                if (!std::isfinite(parameter_values[index]))
                {
                    throw std::runtime_error(
                        "DevQNetwork::trainBatch: non-finite parameter update.");
                }
            }
        };

    apply_update(updated.weights1, accumulated.weights1);
    apply_update(updated.biases1, accumulated.biases1);
    apply_update(updated.weights2, accumulated.weights2);
    apply_update(updated.biases2, accumulated.biases2);
    apply_update(updated.weights3, accumulated.weights3);
    apply_update(updated.biases3, accumulated.biases3);

    // Exactly one mutation of the online network for the complete batch.
    setParameters(updated);

    DevQBatchTrainingResult result{};
    result.batch_size = batch.size();
    result.mean_loss =
        loss_sum * inverse_batch_size;
    result.mean_td_error =
        td_error_sum * inverse_batch_size;
    result.mean_absolute_td_error =
        absolute_td_error_sum * inverse_batch_size;
    result.gradient_l2_norm =
        gradient_l2_norm;
    result.clipped_gradient_l2_norm =
        clipped_gradient_l2_norm;

    result.numerical_integrity =
        result.batch_size > 0 &&
        std::isfinite(result.mean_loss) &&
        std::isfinite(result.mean_td_error) &&
        std::isfinite(result.mean_absolute_td_error) &&
        std::isfinite(result.gradient_l2_norm) &&
        std::isfinite(result.clipped_gradient_l2_norm);

    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "DevQNetwork::trainBatch: result failed numerical-integrity validation.");
    }

    return result;
}


const DevQNetworkConfig&
DevQNetwork::config() const noexcept
{
    return config_;
}

std::size_t
DevQNetwork::parameterCount() const noexcept
{
    return
        DEV_Q_HIDDEN1_SIZE * DEV_Q_INPUT_SIZE +
        DEV_Q_HIDDEN1_SIZE +
        DEV_Q_HIDDEN2_SIZE * DEV_Q_HIDDEN1_SIZE +
        DEV_Q_HIDDEN2_SIZE +
        DEV_Q_OUTPUT_SIZE * DEV_Q_HIDDEN2_SIZE +
        DEV_Q_OUTPUT_SIZE;
}

DevQNetworkParameters
DevQNetwork::parameters() const
{
    return parameters_;
}

void DevQNetwork::setParameters(
    const DevQNetworkParameters& parameters)
{
    validateParameters(parameters);
    parameters_ = parameters;
}

void DevQNetwork::copyParametersFrom(
    const DevQNetwork& other)
{
    setParameters(other.parameters_);
}

double DevQNetwork::relu(
    const double value) noexcept
{
    return value > 0.0 ? value : 0.0;
}

void DevQNetwork::validateInput(
    const std::array<double, DEV_Q_INPUT_SIZE>& input)
{
    for (const double value : input)
    {
        if (!std::isfinite(value))
        {
            throw std::invalid_argument(
                "DevQNetwork: input contains non-finite value.");
        }
    }
}

void DevQNetwork::validateParameters(
    const DevQNetworkParameters& parameters)
{
    if (parameters.weights1.size() !=
        DEV_Q_HIDDEN1_SIZE * DEV_Q_INPUT_SIZE)
    {
        throw std::invalid_argument(
            "DevQNetwork: invalid weights1 size.");
    }

    if (parameters.biases1.size() !=
        DEV_Q_HIDDEN1_SIZE)
    {
        throw std::invalid_argument(
            "DevQNetwork: invalid biases1 size.");
    }

    if (parameters.weights2.size() !=
        DEV_Q_HIDDEN2_SIZE * DEV_Q_HIDDEN1_SIZE)
    {
        throw std::invalid_argument(
            "DevQNetwork: invalid weights2 size.");
    }

    if (parameters.biases2.size() !=
        DEV_Q_HIDDEN2_SIZE)
    {
        throw std::invalid_argument(
            "DevQNetwork: invalid biases2 size.");
    }

    if (parameters.weights3.size() !=
        DEV_Q_OUTPUT_SIZE * DEV_Q_HIDDEN2_SIZE)
    {
        throw std::invalid_argument(
            "DevQNetwork: invalid weights3 size.");
    }

    if (parameters.biases3.size() !=
        DEV_Q_OUTPUT_SIZE)
    {
        throw std::invalid_argument(
            "DevQNetwork: invalid biases3 size.");
    }

    if (!allFinite(parameters.weights1) ||
        !allFinite(parameters.biases1) ||
        !allFinite(parameters.weights2) ||
        !allFinite(parameters.biases2) ||
        !allFinite(parameters.weights3) ||
        !allFinite(parameters.biases3))
    {
        throw std::invalid_argument(
            "DevQNetwork: parameters contain non-finite values.");
    }
}

void DevQNetwork::validateTrainingConfig(
    const DevQTrainingConfig& config)
{
    if (!std::isfinite(config.learning_rate) ||
        config.learning_rate <= 0.0)
    {
        throw std::invalid_argument(
            "DevQNetwork::trainSingle: learning rate must be finite and > 0.");
    }

    if (!std::isfinite(config.huber_delta) ||
        config.huber_delta <= 0.0)
    {
        throw std::invalid_argument(
            "DevQNetwork::trainSingle: Huber delta must be finite and > 0.");
    }

    if (!std::isfinite(config.gradient_clip))
    {
        throw std::invalid_argument(
            "DevQNetwork::trainSingle: gradient clip must be finite.");
    }
}

std::size_t DevQNetwork::actionIndex(
    const DevAction action)
{
    return checkedActionIndex(action);
}

double DevQNetwork::huberLoss(
    const double error,
    const double delta) noexcept
{
    const double absolute_error =
        std::abs(error);

    if (absolute_error <= delta)
    {
        return 0.5 * error * error;
    }

    return
        delta *
        (absolute_error - 0.5 * delta);
}

double DevQNetwork::huberDerivative(
    const double prediction_minus_target,
    const double delta) noexcept
{
    if (prediction_minus_target > delta)
    {
        return delta;
    }

    if (prediction_minus_target < -delta)
    {
        return -delta;
    }

    return prediction_minus_target;
}

double DevQNetwork::clipGradient(
    const double gradient,
    const double clip) noexcept
{
    if (clip <= 0.0)
    {
        return gradient;
    }

    return std::clamp(
        gradient,
        -clip,
        clip);
}

std::array<double, DEV_Q_INPUT_SIZE>
DevQNetwork::observationToInput(
    const DevAIObservation& observation) const
{
    if (!observation.numerical_integrity)
    {
        throw std::invalid_argument(
            "DevQNetwork: observation numerical integrity is false.");
    }

    if (!observation.ready)
    {
        throw std::invalid_argument(
            "DevQNetwork: observation is not ready.");
    }

    static_assert(
        DEV_Q_INPUT_SIZE ==
        DEV_OBSERVATION_SIZE);

    return observation_normalizer_.normalize(observation);
}

} // namespace devai::strategy
