#pragma once

#include <cstdint>
#include <string_view>

#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"


namespace devai::strategy
{

// ============================================================================
// DevPolicy
//
// Abstract action-selection contract for the Dev agent.
//
// A policy receives the current causal DevAI observation and returns exactly
// one legal DevAction.
//
// Implementations may later include:
//
//   - deterministic baseline policy
//   - random/exploration policy
//   - neural-network policy
//   - trained Q-policy
//
// The interface deliberately does NOT:
//
//   - calculate reward
//   - calculate P&L
//   - calculate brokerage/slippage
//   - calculate forward returns
//   - modify TradingEnvironment
//   - store replay experiences
//   - train a neural network
//   - inspect future market information
//
// Phase 3.3 defines only the action-selection boundary.
// ============================================================================
class DevPolicy
{
public:

    virtual ~DevPolicy() = default;


    // ========================================================================
    // selectAction
    //
    // Select one action using only information available in the supplied
    // current observation.
    //
    // decision_time is supplied as current causal metadata. It must not be
    // interpreted as permission to inspect future state.
    // ========================================================================

    [[nodiscard]]
    virtual DevAction selectAction(
        const DevAIObservation& observation,
        std::int64_t decision_time) = 0;


    // ========================================================================
    // name
    //
    // Stable human-readable policy name for diagnostics, tests, model
    // metadata and future episode reports.
    // ========================================================================

    [[nodiscard]]
    virtual std::string_view name() const noexcept = 0;


    // ========================================================================
    // reset
    //
    // Called at an episode boundary.
    //
    // Stateless policies may leave this as the default no-op.
    // Stateful policies can override it later.
    // ========================================================================

    virtual void reset() noexcept
    {
    }
};


// ============================================================================
// DevPolicyValidator
//
// Validation helper for values crossing the policy boundary.
//
// DevAction itself is an enum class, but malformed values can still be
// produced through explicit casts, corrupted model output, bad deserialization
// or future integration defects.
// ============================================================================
class DevPolicyValidator
{
public:

    [[nodiscard]]
    static bool isValidAction(
        DevAction action) noexcept;


    static void validateAction(
        DevAction action);


    static void validateDecisionTime(
        std::int64_t decision_time);
};

} // namespace devai::strategy