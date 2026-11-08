#pragma once

#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevTransition.hpp"

#include <cstdint>

namespace devai::strategy
{

// ============================================================================
// DEVAI PHASE 3.8A
// BELLMAN TARGET ENGINE
//
// Non-terminal transition:
//
//     target = reward
//            + gamma * max_a Q_target(next_observation, a)
//
// Terminal transition:
//
//     target = reward
//
// This engine calculates Bellman targets only.
//
// It does NOT:
//   - update neural-network parameters
//   - perform backpropagation
//   - sample the replay buffer
//   - choose actions
//   - calculate rewards
//   - calculate brokerage/slippage/P&L
//   - use ForwardReturn_N
// ============================================================================

struct DevBellmanTargetConfig
{
    double discount_factor{0.99};
};

struct DevBellmanTargetResult
{
    // Immediate causal reward R_t stored in DevTransition.
    double reward{0.0};

    // gamma.
    double discount_factor{0.0};

    // max_a Q_target(S_{t+1}, a).
    //
    // For terminal transitions there is no bootstrapping, therefore
    // this value remains zero.
    double next_max_q{0.0};

    // gamma * max_a Q_target(S_{t+1}, a).
    //
    // Zero for terminal transitions.
    double discounted_bootstrap{0.0};

    // Final Bellman target.
    double target{0.0};

    // Copied from the transition.
    bool terminal{false};

    // Useful diagnostics.
    std::int64_t decision_time{0};
    std::int64_t next_decision_time{0};

    bool numerical_integrity{false};
};

class DevBellmanTargetEngine
{
public:
    explicit DevBellmanTargetEngine(
        DevBellmanTargetConfig config = {});

    // ------------------------------------------------------------------------
    // Main transition-based API.
    //
    // Non-terminal:
    //     evaluates target_network(next_observation)
    //
    // Terminal:
    //     does NOT bootstrap from next_observation
    // ------------------------------------------------------------------------
    [[nodiscard]]
    DevBellmanTargetResult calculate(
        const DevTransition& transition,
        const DevQNetwork& target_network) const;

    // ------------------------------------------------------------------------
    // Scalar calculation API.
    //
    // Primarily useful for mathematical validation and later batch-training
    // infrastructure.
    //
    // If terminal == true:
    //     next_max_q is ignored mathematically and target == reward.
    // ------------------------------------------------------------------------
    [[nodiscard]]
    DevBellmanTargetResult calculate(
        double reward,
        double next_max_q,
        bool terminal,
        std::int64_t decision_time,
        std::int64_t next_decision_time) const;

    [[nodiscard]]
    const DevBellmanTargetConfig& config() const noexcept;

private:
    static void validateConfig(
        const DevBellmanTargetConfig& config);

    static void validateTransition(
        const DevTransition& transition);

    static void validateChronology(
        std::int64_t decision_time,
        std::int64_t next_decision_time);

    DevBellmanTargetConfig config_;
};

} // namespace devai::strategy