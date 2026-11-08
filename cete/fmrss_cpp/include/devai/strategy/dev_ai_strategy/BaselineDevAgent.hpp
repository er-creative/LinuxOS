#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "devai/strategy/dev_ai_strategy/DevPolicy.hpp"


namespace devai::strategy
{

// ============================================================================
// BaselineDevAgent
//
// Deterministic, non-learning reference implementation of DevPolicy.
//
// Phase 3.4 deliberately uses a HOLD-only policy.
//
// Purpose:
//
//   - prove that a real Dev agent can implement DevPolicy
//   - provide a deterministic reference/control policy
//   - validate observation -> policy -> action flow
//   - provide a stable baseline for later episode integration
//   - provide a zero-trading comparison for learned policies
//
// This baseline deliberately does NOT:
//
//   - train
//   - use a neural network
//   - use replay memory
//   - calculate reward
//   - calculate P&L
//   - calculate transaction costs
//   - calculate slippage
//   - calculate forward returns
//   - inspect future market information
//   - modify the supplied observation
//
// A more intelligent policy will be introduced later without changing the
// frozen DevPolicy interface.
// ============================================================================
class BaselineDevAgent final : public DevPolicy
{
public:

    BaselineDevAgent() = default;


    // ========================================================================
    // selectAction
    //
    // Phase 3.4 baseline action:
    //
    //     HOLD
    //
    // The observation is intentionally accepted but not interpreted.
    // decision_time is validated as causal metadata.
    // ========================================================================

    [[nodiscard]]
    DevAction selectAction(
        const DevAIObservation& observation,
        std::int64_t decision_time) override;


    // ========================================================================
    // name
    // ========================================================================

    [[nodiscard]]
    std::string_view name() const noexcept override;


    // ========================================================================
    // reset
    //
    // Reset episode-local diagnostic state.
    // ========================================================================

    void reset() noexcept override;


    // ========================================================================
    // Diagnostics
    //
    // These values do not influence action selection.
    // ========================================================================

    [[nodiscard]]
    std::size_t selectionCount() const noexcept;


    [[nodiscard]]
    std::int64_t lastDecisionTime() const noexcept;


private:

    std::size_t selection_count_{0};

    std::int64_t last_decision_time_{0};
};

} // namespace devai::strategy