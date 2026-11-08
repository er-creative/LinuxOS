#include "devai/strategy/dev_ai_strategy/BaselineDevAgent.hpp"


namespace devai::strategy
{

// ============================================================================
// selectAction
// ============================================================================

DevAction BaselineDevAgent::selectAction(
    const DevAIObservation& observation,
    const std::int64_t decision_time)
{
    // ------------------------------------------------------------------------
    // Phase 3.4 does not interpret market features yet.
    //
    // The observation is nevertheless part of the policy contract because
    // later Dev policies will use it for action selection.
    // ------------------------------------------------------------------------

    (void)observation;


    // ------------------------------------------------------------------------
    // Validate causal decision metadata before modifying agent-local state.
    // ------------------------------------------------------------------------

    DevPolicyValidator::validateDecisionTime(
        decision_time);


    // ------------------------------------------------------------------------
    // Deterministic reference action.
    // ------------------------------------------------------------------------

    constexpr DevAction action =
        DevAction::HOLD;


    // ------------------------------------------------------------------------
    // Validate the policy output at the same boundary future policies use.
    // ------------------------------------------------------------------------

    DevPolicyValidator::validateAction(
        action);


    // ------------------------------------------------------------------------
    // Update diagnostic state only after successful validation.
    // ------------------------------------------------------------------------

    ++selection_count_;

    last_decision_time_ =
        decision_time;


    return action;
}


// ============================================================================
// name
// ============================================================================

std::string_view BaselineDevAgent::name() const noexcept
{
    return "BaselineDevAgent";
}


// ============================================================================
// reset
// ============================================================================

void BaselineDevAgent::reset() noexcept
{
    selection_count_ = 0;

    last_decision_time_ = 0;
}


// ============================================================================
// selectionCount
// ============================================================================

std::size_t BaselineDevAgent::selectionCount() const noexcept
{
    return selection_count_;
}


// ============================================================================
// lastDecisionTime
// ============================================================================

std::int64_t BaselineDevAgent::lastDecisionTime() const noexcept
{
    return last_decision_time_;
}

} // namespace devai::strategy