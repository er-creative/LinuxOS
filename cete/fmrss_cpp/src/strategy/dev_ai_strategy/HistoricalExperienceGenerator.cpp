#include "devai/strategy/dev_ai_strategy/HistoricalExperienceGenerator.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>


namespace devai::strategy
{

// ============================================================================
// Constructor
// ============================================================================

HistoricalExperienceGenerator::
HistoricalExperienceGenerator(
    std::string symbol,
    DevEpisode& episode,
    TradingEnvironment& environment,
    DevPolicy& policy,
    DevReplayBuffer& replay_buffer)
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
            "HistoricalExperienceGenerator: symbol is empty.");
    }
}


// ============================================================================
// validateResetState
// ============================================================================

void
HistoricalExperienceGenerator::
validateResetState(
    const std::int64_t environment_reset_time,
    const double environment_reset_price)
{
    if (environment_reset_time <= 0)
    {
        throw std::invalid_argument(
            "HistoricalExperienceGenerator: "
            "environment reset time must be positive.");
    }


    if (!std::isfinite(
            environment_reset_price) ||
        environment_reset_price <= 0.0)
    {
        throw std::invalid_argument(
            "HistoricalExperienceGenerator: "
            "environment reset price must be finite and positive.");
    }
}


// ============================================================================
// validateMarketPrices
// ============================================================================

void
HistoricalExperienceGenerator::
validateMarketPrices(
    const std::vector<HistoricalMarketPrice>& market_prices)
{
    if (market_prices.empty())
    {
        throw std::invalid_argument(
            "HistoricalExperienceGenerator: "
            "historical market-price sequence is empty.");
    }


    std::int64_t previous_decision_time =
        0;


    for (const auto& point :
         market_prices)
    {
        if (point.decision_time <= 0)
        {
            throw std::invalid_argument(
                "HistoricalExperienceGenerator: "
                "market-price decision time must be positive.");
        }


        if (!std::isfinite(
                point.market_price) ||
            point.market_price <= 0.0)
        {
            throw std::invalid_argument(
                "HistoricalExperienceGenerator: "
                "historical market price must be finite and positive.");
        }


        if (previous_decision_time != 0 &&
            point.decision_time <=
                previous_decision_time)
        {
            throw std::invalid_argument(
                "HistoricalExperienceGenerator: "
                "historical market prices are not strictly chronological.");
        }


        previous_decision_time =
            point.decision_time;
    }
}


// ============================================================================
// generate
// ============================================================================

HistoricalExperienceGenerationResult
HistoricalExperienceGenerator::
generate(
    const std::int64_t environment_reset_time,
    const double environment_reset_price,
    const std::vector<HistoricalMarketPrice>& market_prices)
{
    validateResetState(
        environment_reset_time,
        environment_reset_price);


    validateMarketPrices(
        market_prices);


    // ------------------------------------------------------------------------
    // Phase 3.5 runner remains the sole owner of:
    //
    //     observation
    //     policy action
    //     TradingEnvironment step
    //     reward extraction
    //     episode advancement
    //     next observation
    //     transition validation
    //     replay insertion
    //
    // Phase 3.6 orchestrates repeated historical generation only.
    // ------------------------------------------------------------------------

    DevAgentEpisodeRunner runner(
        symbol_,
        episode_,
        environment_,
        policy_,
        &replay_buffer_);


    runner.reset(
        environment_reset_time,
        environment_reset_price);


    if (!episode_.isInitialized())
    {
        throw std::runtime_error(
            "HistoricalExperienceGenerator: "
            "episode failed to initialize.");
    }


    if (episode_.totalSteps() < 2)
    {
        throw std::runtime_error(
            "HistoricalExperienceGenerator: "
            "at least two historical states are required.");
    }


    // N historical states produce N - 1 complete transitions.
    const std::size_t required_price_count =
        episode_.totalSteps() - 1;


    if (market_prices.size() !=
        required_price_count)
    {
        throw std::invalid_argument(
            "HistoricalExperienceGenerator: "
            "market-price count must equal episode state count minus one.");
    }


    if (environment_reset_time >=
        episode_.startDecisionTime())
    {
        throw std::invalid_argument(
            "HistoricalExperienceGenerator: "
            "environment reset time must precede episode start time.");
    }


    HistoricalExperienceGenerationResult result;

    result.symbol =
        symbol_;

    result.episode_start_time =
        episode_.startDecisionTime();

    result.episode_end_time =
        episode_.endDecisionTime();

    result.episode_state_count =
        episode_.totalSteps();

    result.replay_size_before =
        replay_buffer_.size();


    std::int64_t previous_transition_time =
        0;


    for (std::size_t index = 0;
         index < market_prices.size();
         ++index)
    {
        if (!runner.canStep())
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "runner became unavailable before all "
                "historical transitions were generated.");
        }


        const auto& price_point =
            market_prices[index];


        // --------------------------------------------------------------------
        // Critical causal alignment check.
        //
        // The price supplied to runner.step() MUST belong to the exact current
        // episode decision time.
        //
        // This prevents accidentally feeding:
        //
        //     price(t+1) into state(t)
        //
        // which would create future leakage.
        // --------------------------------------------------------------------

        if (price_point.decision_time !=
            episode_.currentDecisionTime())
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "market price is not aligned to current episode decision time.");
        }


        const auto step_result =
            runner.step(
                price_point.market_price);


        // --------------------------------------------------------------------
        // Validate generated transition chronology.
        // --------------------------------------------------------------------

        const auto& transition =
            step_result.transition;


        if (transition.decision_time !=
            price_point.decision_time)
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "generated transition decision time is misaligned.");
        }


        if (transition.next_decision_time <=
            transition.decision_time)
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "generated transition chronology is invalid.");
        }


        if (previous_transition_time != 0 &&
            transition.decision_time <=
                previous_transition_time)
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "generated transition sequence is not chronological.");
        }


        if (!std::isfinite(
                transition.reward))
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "generated transition reward is not finite.");
        }


        if (!step_result.stored_in_replay)
        {
            throw std::runtime_error(
                "HistoricalExperienceGenerator: "
                "generated transition was not stored in replay buffer.");
        }


        previous_transition_time =
            transition.decision_time;


        ++result.transitions_generated;
    }


    result.replay_size_after =
        replay_buffer_.size();


    // ------------------------------------------------------------------------
    // Number physically retained in replay memory.
    //
    // The replay buffer is bounded. If it was already close to capacity,
    // old transitions may be replaced while generation proceeds.
    //
    // Therefore replay_size_after - replay_size_before is NOT always equal
    // to transitions_generated.
    //
    // transitions_stored means transitions successfully sent through the
    // validated runner into replay memory during this generation.
    // ------------------------------------------------------------------------

    result.transitions_stored =
        result.transitions_generated;


    result.terminal_reached =
        episode_.isTerminal();


    result.numerical_integrity =
        result.transitions_generated ==
            required_price_count &&
        runner.transitionsProduced() ==
            required_price_count &&
        result.transitions_stored ==
            required_price_count &&
        result.terminal_reached;


    if (!result.numerical_integrity)
    {
        throw std::runtime_error(
            "HistoricalExperienceGenerator: "
            "historical experience generation failed integrity validation.");
    }


    return result;
}


// ============================================================================
// symbol
// ============================================================================

const std::string&
HistoricalExperienceGenerator::
symbol() const noexcept
{
    return symbol_;
}


} // namespace devai::strategy