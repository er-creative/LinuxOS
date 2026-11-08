#pragma once
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace devai::strategy {
// Single parameter set for both roles. The role is explicitly encoded.
// Agent actions use output indices 0..3; manager actions use 0..6.
inline constexpr std::size_t DEV_SHARED_INPUT = DEV_Q_INPUT_SIZE + DEV_MANAGER_INPUT_SIZE + 2;
inline constexpr std::size_t DEV_SHARED_HIDDEN = 64;
inline constexpr std::size_t DEV_SHARED_OUTPUT = DEV_MANAGER_ACTION_COUNT;

struct DevSharedInput {
    std::array<double, DEV_SHARED_INPUT> values{};
};
struct DevSharedParameters {
    std::vector<double> w1, b1, w2, b2;
};
class DevSharedNeuralNetwork {
public:
    explicit DevSharedNeuralNetwork(std::uint32_t seed = 73117U);
    [[nodiscard]] DevSharedInput encodeAgent(const DevAIObservation& observation) const;
    [[nodiscard]] DevSharedInput encodeManager(const DevManagerEncodedState& state) const;
    [[nodiscard]] std::array<double, DEV_SHARED_OUTPUT> predict(const DevSharedInput& input) const;
    [[nodiscard]] DevQValues predictAgent(const DevAIObservation& observation) const;
    [[nodiscard]] DevManagerQValues predictManager(const DevManagerEncodedState& state) const;
    // One shared update path; only the chosen action output is trained.
    [[nodiscard]] double train(const DevSharedInput& input, std::size_t action,
                               double target, double learning_rate = 0.001);
    [[nodiscard]] const DevSharedParameters& parameters() const noexcept { return params_; }
    void setParameters(const DevSharedParameters& parameters);
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
private:
    DevSharedParameters params_;
};
}
