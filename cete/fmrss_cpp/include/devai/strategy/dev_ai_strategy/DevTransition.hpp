#pragma once

#include <cstdint>

#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"


namespace devai::strategy
{

// ============================================================================
// DevTransition
//
// Represents one complete causal experience observed by the Dev agent:
//
//     S(t)
//       |
//       v
//     A(t)
//       |
//       v
//     market / CETE advances
//       |
//       v
//     R(t)
//       |
//       v
//     S(t+1)
//
// Reinforcement-learning representation:
//
//     (S_t, A_t, R_t, S_{t+1}, Done_t)
//
// IMPORTANT:
//
// This structure stores an already-observed experience.
//
// It deliberately does NOT:
//
//   - calculate reward
//   - calculate P&L
//   - calculate brokerage
//   - calculate slippage
//   - calculate forward returns
//   - inspect future market data
//   - select an action
//   - perform learning
//   - contain DQN-specific state
//
// ForwardReturn_N, future prices, MFE, MAE, and any other future-derived
// information must never be inserted into observation or next_observation.
//
// A terminal transition may still contain next_observation when that state
// genuinely exists. The terminal flag determines whether learning algorithms
// are permitted to bootstrap beyond the transition.
// ============================================================================
struct DevTransition
{
    DevAIObservation observation{};

    DevAction action{
        DevAction::HOLD
    };

    double reward{0.0};

    DevAIObservation next_observation{};

    bool terminal{false};

    std::int64_t decision_time{0};

    std::int64_t next_decision_time{0};
};


// ============================================================================
// DevTransitionValidator
//
// Validates the structural and causal integrity of a DevTransition.
//
// This validator deliberately performs no trading or learning logic.
// ============================================================================
class DevTransitionValidator
{
public:

    void validate(
        const DevTransition& transition) const;


    [[nodiscard]]
    bool hasNumericalIntegrity(
        const DevTransition& transition) const noexcept;


    [[nodiscard]]
    bool hasChronologicalIntegrity(
        const DevTransition& transition) const noexcept;


    [[nodiscard]]
    static bool isValidAction(
        DevAction action) noexcept;


private:

    [[nodiscard]]
    static bool finite(
        double value) noexcept;
};

} // namespace devai::strategy