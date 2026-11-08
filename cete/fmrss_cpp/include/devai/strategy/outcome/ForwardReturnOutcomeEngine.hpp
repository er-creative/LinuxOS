#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>


namespace devai::strategy::outcome
{

// ============================================================================
// ForwardReturnBar
//
// Minimal historical information required to calculate a forward return.
//
// IMPORTANT:
//
// The engine deliberately accepts only:
//
//     decision_time
//     close
//
// It does not require:
//     features
//     observations
//     actions
//     rewards
//     positions
//     P&L
//
// The supplied vector must represent one chronological episode / historical
// sequence. Episode/session boundary management remains the responsibility
// of the caller.
// ============================================================================
struct ForwardReturnBar
{
    std::int64_t decision_time{0};

    double close{0.0};
};


// ============================================================================
// ForwardReturnOutcome
//
// Represents the realized outcome for:
//
//     origin_index -> origin_index + horizon_bars
//
// Formula:
//
//     forward_return = outcome_close / origin_close - 1
//
// available == false means that the requested future horizon has not
// matured inside the supplied historical sequence.
//
// When available == false:
//
//     outcome_index       = 0
//     outcome_time        = 0
//     outcome_close       = 0
//     forward_return      = 0
//
// These zeros are placeholders only. The caller MUST inspect available.
// ============================================================================
struct ForwardReturnOutcome
{
    std::size_t origin_index{0};

    std::size_t horizon_bars{0};

    std::size_t outcome_index{0};

    std::int64_t origin_time{0};

    std::int64_t outcome_time{0};

    double origin_close{0.0};

    double outcome_close{0.0};

    double forward_return{0.0};

    bool available{false};

    bool numerical_integrity{false};
};


// ============================================================================
// ForwardReturnOutcomeEngine
//
// CETE SHARED OFFLINE OUTCOME COMPONENT.
//
// Calculates realized N-bar forward returns from a chronological historical
// sequence.
//
// This engine is NOT:
//
//     - a forecast engine
//     - an MFE engine
//     - an MAE engine
//     - a signal engine
//     - a reward engine
//     - a trading engine
//     - an observation feature engine
//
// CRITICAL ANTI-LEAKAGE CONTRACT:
//
// ForwardReturn_N(t) is only an OUTCOME.
//
// It must NEVER be inserted into Dev's observation at time t.
//
// The outcome becomes available only when the historical sequence contains
// the exact bar:
//
//     t + N
//
// No partial horizon is accepted.
// No substitute future bar is accepted.
// No extrapolation is performed.
// ============================================================================
class ForwardReturnOutcomeEngine
{
public:

    ForwardReturnOutcomeEngine() = default;


    // ========================================================================
    // evaluate
    //
    // Evaluate one origin bar and one forward horizon.
    //
    // If origin_index + horizon_bars exists:
    //
    //     available = true
    //
    // otherwise:
    //
    //     available = false
    //
    // horizon_bars must be > 0.
    // ========================================================================

    [[nodiscard]]
    ForwardReturnOutcome evaluate(
        const std::vector<ForwardReturnBar>& bars,
        std::size_t origin_index,
        std::size_t horizon_bars) const;


    // ========================================================================
    // evaluateAll
    //
    // Generates one outcome record for every origin bar.
    //
    // The final horizon_bars records will naturally be unavailable because
    // their complete future horizon does not exist inside the sequence.
    // ========================================================================

    [[nodiscard]]
    std::vector<ForwardReturnOutcome> evaluateAll(
        const std::vector<ForwardReturnBar>& bars,
        std::size_t horizon_bars) const;


    // ========================================================================
    // calculate
    //
    // Pure mathematical calculation.
    //
    // Exposed primarily for deterministic unit validation.
    // ========================================================================

    [[nodiscard]]
    static double calculate(
        double origin_close,
        double outcome_close);


private:

    static bool finite(
        double value) noexcept;


    static void validateHorizon(
        std::size_t horizon_bars);


    static void validateBar(
        const ForwardReturnBar& bar);


    static void validateSequence(
        const std::vector<ForwardReturnBar>& bars);
};

} // namespace devai::strategy::outcome