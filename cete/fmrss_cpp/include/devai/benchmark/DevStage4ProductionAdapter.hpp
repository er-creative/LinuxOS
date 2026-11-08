#pragma once
#include "devai/benchmark/DevRLModelInterface.hpp"
#include "devai/benchmark/DevRuntimeReplayBridge.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace devai::benchmark::h47 {

// Strict production bridge: accepts only a state produced by CETE's actual
// DevManagerStateEncoder, never an invented 148-element diagnostic vector.
inline h45::HistoricalManagerObservation makeProductionObservation(
    const devai::strategy::DevManagerEncodedState& manager_state,
    const devai::strategy::DevSharedNeuralNetwork& network,
    const std::array<std::string, 5>& top_five,
    std::int64_t decision_time_utc,
    bool terminal)
{
    if (!manager_state.numerical_integrity ||
        manager_state.decision_time != decision_time_utc)
        throw std::invalid_argument("Stage4: invalid manager state/time");
    h45::HistoricalManagerObservation o{};
    o.decision_time_utc = decision_time_utc;
    o.encoded_manager_features = network.encodeManager(manager_state).values;
    o.ranked_top_five = top_five;
    o.legal_actions = manager_state.valid_actions;
    o.completed_candles = true; // caller must supply verified completed-candle state
    o.causal_features = true;    // caller must supply causal CETE manager state
    o.terminal = terminal;
    return o;
}

// Never permit inference from random initialization when a checkpoint is required.
inline std::shared_ptr<devai::strategy::DevSharedNeuralNetwork>
loadTrainedNetwork(const std::filesystem::path& checkpoint)
{
    if (checkpoint.empty() || !std::filesystem::is_regular_file(checkpoint))
        throw std::runtime_error("Stage4: trained checkpoint not found");
    auto network = std::make_shared<devai::strategy::DevSharedNeuralNetwork>();
    network->load(checkpoint);
    return network;
}

} // namespace devai::benchmark::h47
