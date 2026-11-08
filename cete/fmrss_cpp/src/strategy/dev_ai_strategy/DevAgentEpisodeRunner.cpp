#include "devai/strategy/dev_ai_strategy/DevAgentEpisodeRunner.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>


namespace devai::strategy
{

// ============================================================================
// Construction
// ============================================================================

DevAgentEpisodeRunner::DevAgentEpisodeRunner(
    std::string symbol,
    DevEpisode& episode,
    TradingEnvironment& environment,
    DevPolicy& policy,
    DevReplayBuffer* replay_buffer)
    :
    symbol_(std::move(symbol)),
    episode_(episode),
    environment_(environment),
    policy_(policy),
    replay_buffer_(replay_buffer)
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "DevAgentEpisodeRunner symbol must not be empty.");
    }
}


// ============================================================================
// reset
// ============================================================================

void DevAgentEpisodeRunner::reset(
    const std::int64_t environment_reset_time,
    const double initial_market_price)
{
    validateMarketPrice(
        initial_market_price);


    episode_.reset();


    if (!episode_.isInitialized())
    {
        throw std::logic_error(
            "DevAgentEpisodeRunner episode failed to initialize.");
    }


    if (episode_.totalSteps() == 0)
    {
        throw std::logic_error(
            "DevAgentEpisodeRunner episode contains no steps.");
    }


    if (environment_reset_time <= 0)
    {
        throw std::invalid_argument(
            "DevAgentEpisodeRunner reset time must be positive.");
    }


    if (environment_reset_time >=
        episode_.currentDecisionTime())
    {
        throw std::invalid_argument(
            "DevAgentEpisodeRunner environment reset time must "
            "strictly precede first episode decision time.");
    }


    policy_.reset();


    environment_.reset(
        symbol_,
        environment_reset_time,
        initial_market_price);


    transitions_produced_ = 0;

    reset_completed_ = true;
}


// ============================================================================
// step
// ============================================================================

DevAgentEpisodeRunnerResult
DevAgentEpisodeRunner::step(
    const double current_market_price)
{
    if (!reset_completed_)
    {
        throw std::logic_error(
            "DevAgentEpisodeRunner must be reset before step().");
    }


    if (!canStep())
    {
        throw std::logic_error(
            "DevAgentEpisodeRunner cannot create another complete transition.");
    }


    validateMarketPrice(
        current_market_price);


    // ========================================================================
    // 1. Capture S(t)
    // ========================================================================

    const std::size_t source_step_index =
        episode_.stepIndex();


    const std::int64_t decision_time =
        episode_.currentDecisionTime();


    DevPolicyValidator::validateDecisionTime(
        decision_time);


    const DevAIObservation observation =
        buildCurrentObservation();


    if (!observation.numerical_integrity)
    {
        throw std::runtime_error(
            "Current DevAI observation lacks numerical integrity.");
    }


    if (!observation.ready)
    {
        throw std::runtime_error(
            "Current DevAI observation is not ready.");
    }


    // ========================================================================
    // 2. Select A(t)
    //
    // IMPORTANT:
    //
    // episode_.advance() has NOT occurred.
    //
    // The policy therefore receives only S(t).
    // ========================================================================

    const DevAction action =
        policy_.selectAction(
            observation,
            decision_time);


    DevPolicyValidator::validateAction(
        action);


    // ========================================================================
    // 3. Execute through frozen TradingEnvironment
    //
    // TradingEnvironment remains authoritative for:
    //
    //   execution
    //   brokerage
    //   slippage
    //   P&L
    //   reward
    //   portfolio state
    // ========================================================================

    const TradingStepResult trading_step =
        environment_.step(
            action,
            decision_time,
            current_market_price);


    if (trading_step.decision_time !=
        decision_time)
    {
        throw std::logic_error(
            "TradingEnvironment decision time is not aligned with episode.");
    }


    if (trading_step.action != action)
    {
        throw std::logic_error(
            "TradingEnvironment action differs from selected Dev action.");
    }


    if (!std::isfinite(
            trading_step.reward.total))
    {
        throw std::runtime_error(
            "TradingEnvironment produced non-finite reward.");
    }


    // ========================================================================
    // 4. Advance historical episode
    //
    // Only now is S(t+1) exposed.
    // ========================================================================

    episode_.advance();


    const std::size_t destination_step_index =
        episode_.stepIndex();


    const std::int64_t next_decision_time =
        episode_.currentDecisionTime();


    if (next_decision_time <=
        decision_time)
    {
        throw std::logic_error(
            "DevAgentEpisodeRunner detected non-forward chronology.");
    }


    // ========================================================================
    // 5. Build S(t+1)
    //
    // Portfolio state is the POST-ACTION state returned by the environment.
    // ========================================================================

    const DevAIObservation next_observation =
        observation_builder_.build(
            episode_.currentSnapshot(),
            symbol_,
            trading_step.portfolio);


    if (!next_observation.numerical_integrity)
    {
        throw std::runtime_error(
            "Next DevAI observation lacks numerical integrity.");
    }


    if (!next_observation.ready)
    {
        throw std::runtime_error(
            "Next DevAI observation is not ready.");
    }


    // ========================================================================
    // 6. Construct transition
    //
    // terminal=true means S(t+1) is the final episode state and learning must
    // not bootstrap beyond it.
    // ========================================================================

    DevTransition transition;

    transition.observation =
        observation;

    transition.action =
        action;

    transition.reward =
        trading_step.reward.total;

    transition.next_observation =
        next_observation;

    transition.terminal =
        episode_.isTerminal();

    transition.decision_time =
        decision_time;

    transition.next_decision_time =
        next_decision_time;


    transition_validator_.validate(
        transition);


    // ========================================================================
    // 7. Store completed experience
    // ========================================================================

    bool stored_in_replay =
        false;


    if (replay_buffer_ != nullptr)
    {
        replay_buffer_->push(
            transition);

        stored_in_replay =
            true;
    }


    ++transitions_produced_;


    DevAgentEpisodeRunnerResult result;

    result.transition =
        transition;

    result.trading_step =
        trading_step;

    result.source_step_index =
        source_step_index;

    result.destination_step_index =
        destination_step_index;

    result.stored_in_replay =
        stored_in_replay;


    return result;
}


// ============================================================================
// canStep
// ============================================================================

bool DevAgentEpisodeRunner::canStep() const noexcept
{
    if (!reset_completed_)
    {
        return false;
    }


    if (!episode_.isInitialized())
    {
        return false;
    }


    if (episode_.isTerminal())
    {
        return false;
    }


    if (episode_.totalSteps() < 2)
    {
        return false;
    }


    return
        (episode_.stepIndex() + 1) <
        episode_.totalSteps();
}


// ============================================================================
// transitionsProduced
// ============================================================================

std::size_t
DevAgentEpisodeRunner::transitionsProduced() const noexcept
{
    return transitions_produced_;
}


// ============================================================================
// symbol
// ============================================================================

const std::string&
DevAgentEpisodeRunner::symbol() const noexcept
{
    return symbol_;
}


// ============================================================================
// buildCurrentObservation
// ============================================================================

DevAIObservation
DevAgentEpisodeRunner::buildCurrentObservation() const
{
    const DevPortfolioState portfolio =
        environment_.portfolioState();


    return
        observation_builder_.build(
            episode_.currentSnapshot(),
            symbol_,
            portfolio);
}


// ============================================================================
// validateMarketPrice
// ============================================================================

void DevAgentEpisodeRunner::validateMarketPrice(
    const double market_price)
{
    if (!std::isfinite(market_price) ||
        market_price <= 0.0)
    {
        throw std::invalid_argument(
            "DevAgentEpisodeRunner market price must be finite and positive.");
    }
}

} // namespace devai::strategy