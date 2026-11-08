#pragma once

#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationNormalizer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace devai::strategy
{

inline constexpr std::size_t DEV_Q_INPUT_SIZE = DEV_OBSERVATION_SIZE;
inline constexpr std::size_t DEV_Q_HIDDEN1_SIZE = 64;
inline constexpr std::size_t DEV_Q_HIDDEN2_SIZE = 32;
inline constexpr std::size_t DEV_Q_OUTPUT_SIZE = 4;

static_assert(DEV_Q_INPUT_SIZE == DEV_OBSERVATION_SIZE);
static_assert(DEV_Q_OUTPUT_SIZE == 4);

struct DevQValues
{
    std::array<double, DEV_Q_OUTPUT_SIZE> values{};
    bool numerical_integrity{false};

    [[nodiscard]] double value(DevAction action) const;
    [[nodiscard]] DevAction greedyAction() const;
    [[nodiscard]] double maximumValue() const;
};

struct DevQNetworkParameters
{
    // Frozen Phase-3.7 row-major layout:
    // W1[hidden1][input]
    // W2[hidden2][hidden1]
    // W3[action][hidden2]
    std::vector<double> weights1;
    std::vector<double> biases1;
    std::vector<double> weights2;
    std::vector<double> biases2;
    std::vector<double> weights3;
    std::vector<double> biases3;
};

struct DevQNetworkConfig
{
    std::uint32_t random_seed{12345U};
    bool zero_initialize{false};
};

struct DevQTrainingConfig
{
    double learning_rate{0.001};
    double huber_delta{1.0};

    // Element-wise clipping. <= 0 disables clipping.
    double gradient_clip{10.0};
};

struct DevQTrainingResult
{
    DevAction action{DevAction::HOLD};
    double target_q{0.0};
    double predicted_q_before{0.0};
    double predicted_q_after{0.0};

    // Conventional TD error = target - prediction_before.
    double td_error{0.0};
    double loss{0.0};

    double gradient_l2_norm{0.0};
    double clipped_gradient_l2_norm{0.0};
    bool numerical_integrity{false};
};


// ============================================================================
// Phase 3.8C — Mini-Batch Training Contract
// ============================================================================

struct DevQTrainingSample
{
    DevAIObservation observation{};
    DevAction action{DevAction::HOLD};
    double target_q{0.0};
};

struct DevQBatchTrainingResult
{
    std::size_t batch_size{0};

    double mean_loss{0.0};
    double mean_td_error{0.0};
    double mean_absolute_td_error{0.0};

    // Norm of the averaged batch gradient before/after element-wise clipping.
    double gradient_l2_norm{0.0};
    double clipped_gradient_l2_norm{0.0};

    bool numerical_integrity{false};
};

class DevQNetwork
{
public:
    explicit DevQNetwork(DevQNetworkConfig config = {});

    [[nodiscard]]
    DevQValues predict(const DevAIObservation& observation) const;

    [[nodiscard]]
    DevQValues predict(
        const std::array<double, DEV_Q_INPUT_SIZE>& input) const;

    // Phase 3.8B: one causal supervised Q update for Q(S_t, A_t).
    // target_q is supplied by the already-frozen Bellman Target Engine.
    [[nodiscard]]
    DevQTrainingResult trainSingle(
        const DevAIObservation& observation,
        DevAction action,
        double target_q,
        const DevQTrainingConfig& training_config = {});


    // Phase 3.8C — true mini-batch update.
    //
    // Every sample gradient is evaluated from the SAME pre-update parameter
    // snapshot. Gradients are then averaged and exactly one SGD update is
    // applied. This is intentionally NOT repeated trainSingle() SGD.
    [[nodiscard]]
    DevQBatchTrainingResult trainBatch(
        const std::vector<DevQTrainingSample>& batch,
        const DevQTrainingConfig& training_config = {});

    [[nodiscard]]
    const DevQNetworkConfig& config() const noexcept;

    [[nodiscard]]
    std::size_t parameterCount() const noexcept;

    [[nodiscard]]
    DevQNetworkParameters parameters() const;

    void setParameters(const DevQNetworkParameters& parameters);
    void copyParametersFrom(const DevQNetwork& other);

private:
    [[nodiscard]]
    static double relu(double value) noexcept;

    static void validateInput(
        const std::array<double, DEV_Q_INPUT_SIZE>& input);

    static void validateParameters(
        const DevQNetworkParameters& parameters);

    static void validateTrainingConfig(
        const DevQTrainingConfig& config);

    [[nodiscard]]
    static std::size_t actionIndex(DevAction action);

    [[nodiscard]]
    static double huberLoss(
        double error,
        double delta) noexcept;

    [[nodiscard]]
    static double huberDerivative(
        double prediction_minus_target,
        double delta) noexcept;

    [[nodiscard]]
    static double clipGradient(
        double gradient,
        double clip) noexcept;

    [[nodiscard]]
    std::array<double, DEV_Q_INPUT_SIZE>
    observationToInput(const DevAIObservation& observation) const;

    DevQNetworkConfig config_;
    DevQNetworkParameters parameters_;
    ObservationNormalizer observation_normalizer_{};
};

} // namespace devai::strategy
