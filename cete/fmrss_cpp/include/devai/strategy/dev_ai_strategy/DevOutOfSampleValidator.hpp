#pragma once

#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace devai::strategy
{

// ============================================================================
// DEVAI PHASE 3.10 — INITIAL CONTROLLED OUT-OF-SAMPLE VALIDATION
//
// This component evaluates a FROZEN Dev Q-network on a strictly later,
// already-normalized observation sequence.
//
// It deliberately does NOT:
//   - train or mutate the network,
//   - generate historical CETE state,
//   - calculate trading P&L/reward,
//   - use ForwardReturn_N,
//   - infer counterfactual reward for actions not actually executed.
//
// Profitability evaluation belongs to a later causal trading/OOS episode.
// ============================================================================

struct DevOutOfSamplePoint
{
    std::int64_t decision_time{0};
    std::array<double, DEV_Q_INPUT_SIZE> normalized_observation{};
};

struct DevOutOfSamplePrediction
{
    std::int64_t decision_time{0};
    DevAction greedy_action{DevAction::HOLD};
    std::array<double, DEV_Q_OUTPUT_SIZE> q_values{};
    double maximum_q{0.0};
    bool numerical_integrity{false};
};

struct DevOutOfSampleValidationResult
{
    std::int64_t training_end_time{0};
    std::int64_t oos_start_time{0};
    std::int64_t oos_end_time{0};

    std::size_t evaluated_points{0};

    std::array<std::size_t, DEV_Q_OUTPUT_SIZE> greedy_action_counts{};

    double mean_maximum_q{0.0};
    double minimum_maximum_q{0.0};
    double maximum_maximum_q{0.0};

    bool strict_temporal_separation{false};
    bool network_unchanged{false};
    bool numerical_integrity{false};

    std::vector<DevOutOfSamplePrediction> predictions;
};

class DevOutOfSampleValidator
{
public:
    [[nodiscard]]
    DevOutOfSampleValidationResult evaluate(
        const DevQNetwork& frozen_network,
        std::int64_t training_end_time,
        const std::vector<DevOutOfSamplePoint>& oos_points) const;

private:
    [[nodiscard]]
    static std::size_t actionIndex(
        DevAction action);

    [[nodiscard]]
    static bool parametersEqual(
        const DevQNetworkParameters& left,
        const DevQNetworkParameters& right) noexcept;
};

} // namespace devai::strategy
