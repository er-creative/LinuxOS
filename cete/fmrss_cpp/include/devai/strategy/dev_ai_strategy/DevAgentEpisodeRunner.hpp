#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/DevPolicy.hpp"
#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"


namespace devai::strategy
{

// ============================================================================
// DevAgentEpisodeRunnerResult
//
// Result of one successfully completed causal agent transition:
//
//     S(t)
//       -> A(t)
//       -> TradingEnvironment step at t
//       -> episode advances
//       -> S(t+1)
//       -> DevTransition
//       -> optional ReplayBuffer insertion
//
// The runner does not calculate reward itself. The authoritative scalar reward
// is supplied by TradingEnvironment through:
//
//     TradingStepResult::reward.total
// ============================================================================
struct DevAgentEpisodeRunnerResult
{
    DevTransition transition{};

    TradingStepResult trading_step{};

    std::size_t source_step_index{0};

    std::size_t destination_step_index{0};

    bool stored_in_replay{false};
};


// ============================================================================
// DevAgentEpisodeRunner
//
// Phase 3.5 orchestration component.
//
// Responsibilities:
//
//   - build current Dev observation
//   - call DevPolicy
//   - validate selected action
//   - execute action through TradingEnvironment
//   - advance DevEpisode exactly once
//   - build next causal observation
//   - construct and validate DevTransition
//   - optionally insert transition into DevReplayBuffer
//
// It deliberately does NOT:
//
//   - calculate CETE features
//   - calculate reward
//   - calculate P&L
//   - calculate brokerage
//   - calculate slippage
//   - calculate forward returns
//   - inspect future market state before action selection
//   - train a model
//   - perform neural-network inference itself
//
// Raw market-price lookup also remains outside this class. The caller supplies
// the exact current observable market price corresponding to the episode's
// current decision time.
// ============================================================================
class DevAgentEpisodeRunner
{
public:

    DevAgentEpisodeRunner(
        std::string symbol,
        DevEpisode& episode,
        TradingEnvironment& environment,
        DevPolicy& policy,
        DevReplayBuffer* replay_buffer = nullptr);


    // ========================================================================
    // reset
    //
    // Resets episode and policy state.
    //
    // TradingEnvironment requires a real observable market price at reset, so
    // its reset remains explicit here rather than hidden from the caller.
    //
    // environment_reset_time must strictly precede the first episode decision
    // time, matching the already-validated TradingEnvironment chronology.
    // ========================================================================

    void reset(
        std::int64_t environment_reset_time,
        double initial_market_price);


    // ========================================================================
    // step
    //
    // Runs exactly one agent decision from the CURRENT episode snapshot.
    //
    // A transition requires S(t+1). Therefore step() is legal only while a
    // later episode snapshot exists.
    //
    // current_market_price must correspond exactly to currentDecisionTime().
    //
    // The action is selected BEFORE episode advancement. Therefore the policy
    // cannot see S(t+1) during action selection.
    // ========================================================================

    [[nodiscard]]
    DevAgentEpisodeRunnerResult step(
        double current_market_price);


    // ========================================================================
    // State
    // ========================================================================

    [[nodiscard]]
    bool canStep() const noexcept;


    [[nodiscard]]
    std::size_t transitionsProduced() const noexcept;


    [[nodiscard]]
    const std::string& symbol() const noexcept;


private:

    [[nodiscard]]
    DevAIObservation buildCurrentObservation() const;


    static void validateMarketPrice(
        double market_price);


private:

    std::string symbol_;

    DevEpisode& episode_;

    TradingEnvironment& environment_;

    DevPolicy& policy_;

    DevReplayBuffer* replay_buffer_{nullptr};

    ObservationBuilder observation_builder_{};

    DevTransitionValidator transition_validator_{};

    std::size_t transitions_produced_{0};

    bool reset_completed_{false};
};

} // namespace devai::strategy