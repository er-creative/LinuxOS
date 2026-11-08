#include "devai/strategy/dev_ai_strategy/PositionExposureState.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy
{

// ============================================================================
// Constructor
// ============================================================================

PositionExposureState::PositionExposureState(
    TradingEnvironmentConfig config)
    :
    config_(std::move(config)),
    environment_(config_)
{
}


// ============================================================================
// Reset
// ============================================================================

void
PositionExposureState::reset(
    const std::string& symbol,
    std::int64_t decision_time,
    double market_price)
{
    if (symbol.empty())
    {
        throw std::invalid_argument(
            "PositionExposureState symbol cannot be empty.");
    }


    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "PositionExposureState decision time must be positive.");
    }


    if (!std::isfinite(market_price) ||
        market_price <= 0.0)
    {
        throw std::invalid_argument(
            "PositionExposureState market price must be "
            "positive and finite.");
    }


    environment_.reset(
        symbol,
        decision_time,
        market_price);


    initialized_ = true;


    validateState();


    // Episode reset MUST begin flat.
    const auto& state =
        environment_.state();


    if (state.position != 0.0 ||
        state.quantity != 0.0 ||
        state.entry_price != 0.0 ||
        state.holding_bars != 0)
    {
        throw std::logic_error(
            "PositionExposureState reset did not produce "
            "a clean flat position.");
    }
}


// ============================================================================
// Step
// ============================================================================

TradingStepResult
PositionExposureState::step(
    DevAction action,
    std::int64_t decision_time,
    double market_price)
{
    if (!initialized_)
    {
        throw std::logic_error(
            "PositionExposureState must be reset before step().");
    }


    if (!isValidAction(action))
    {
        throw std::invalid_argument(
            "PositionExposureState received invalid DevAction.");
    }


    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "PositionExposureState decision time must be positive.");
    }


    if (!std::isfinite(market_price) ||
        market_price <= 0.0)
    {
        throw std::invalid_argument(
            "PositionExposureState market price must be "
            "positive and finite.");
    }


    const auto& before =
        environment_.state();


    if (decision_time <=
        before.decision_time)
    {
        throw std::invalid_argument(
            "PositionExposureState decision time must advance "
            "strictly forward.");
    }


    const double expected_target =
        targetPosition(
            action,
            before.position);


    TradingStepResult result =
        environment_.step(
            action,
            decision_time,
            market_price);


    validateState();


    const auto& after =
        environment_.state();


    if (after.position !=
        expected_target)
    {
        throw std::logic_error(
            "TradingEnvironment position does not match "
            "DevAction target position.");
    }


    if (result.target_position !=
        expected_target)
    {
        throw std::logic_error(
            "TradingStepResult target position mismatch.");
    }


    if (after.decision_time !=
        decision_time)
    {
        throw std::logic_error(
            "PositionExposureState decision-time mismatch.");
    }


    return result;
}


// ============================================================================
// Validate authoritative TradingEnvironment state
// ============================================================================

void
PositionExposureState::validateState() const
{
    const auto& state =
        environment_.state();


    if (!state.initialized)
    {
        throw std::logic_error(
            "TradingEnvironment state is not initialized.");
    }


    if (!isValidDevPosition(
            state.position))
    {
        throw std::logic_error(
            "TradingEnvironment contains invalid position.");
    }


    if (!std::isfinite(state.quantity) ||
        state.quantity < 0.0)
    {
        throw std::logic_error(
            "TradingEnvironment contains invalid quantity.");
    }


    if (!std::isfinite(state.entry_price) ||
        state.entry_price < 0.0)
    {
        throw std::logic_error(
            "TradingEnvironment contains invalid entry price.");
    }


    if (state.position == 0.0)
    {
        if (state.quantity != 0.0 ||
            state.entry_price != 0.0)
        {
            throw std::logic_error(
                "Flat position contains residual exposure.");
        }
    }
    else
    {
        if (state.quantity <= 0.0 ||
            state.entry_price <= 0.0)
        {
            throw std::logic_error(
                "Open position does not contain valid exposure.");
        }
    }
}


// ============================================================================
// Snapshot
// ============================================================================

PositionExposureSnapshot
PositionExposureState::snapshot() const
{
    if (!initialized_)
    {
        throw std::logic_error(
            "PositionExposureState is not initialized.");
    }


    validateState();


    const auto& state =
        environment_.state();


    PositionExposureSnapshot output;

    output.symbol =
        state.symbol;

    output.decision_time =
        state.decision_time;

    output.position =
        state.position;

    output.quantity =
        state.quantity;

    output.entry_price =
        state.entry_price;

    output.exposure_fraction =
        config_.exposure_fraction;

    output.holding_bars =
        state.holding_bars;

    output.flat =
        state.position == 0.0;

    output.long_position =
        state.position == 1.0;

    output.short_position =
        state.position == -1.0;

    output.initialized =
        state.initialized;


    return output;
}


// ============================================================================
// Environment state
// ============================================================================

const TradingEnvironmentState&
PositionExposureState::environmentState() const noexcept
{
    return environment_.state();
}


// ============================================================================
// Portfolio state
// ============================================================================

DevPortfolioState
PositionExposureState::portfolioState() const
{
    if (!initialized_)
    {
        throw std::logic_error(
            "PositionExposureState is not initialized.");
    }


    return environment_.portfolioState();
}


// ============================================================================
// Convenience accessors
// ============================================================================

bool
PositionExposureState::initialized() const noexcept
{
    return initialized_;
}


bool
PositionExposureState::isFlat() const noexcept
{
    return initialized_ &&
           environment_.state().position == 0.0;
}


bool
PositionExposureState::isLong() const noexcept
{
    return initialized_ &&
           environment_.state().position == 1.0;
}


bool
PositionExposureState::isShort() const noexcept
{
    return initialized_ &&
           environment_.state().position == -1.0;
}


double
PositionExposureState::position() const noexcept
{
    if (!initialized_)
    {
        return 0.0;
    }

    return environment_.state().position;
}


double
PositionExposureState::quantity() const noexcept
{
    if (!initialized_)
    {
        return 0.0;
    }

    return environment_.state().quantity;
}


double
PositionExposureState::entryPrice() const noexcept
{
    if (!initialized_)
    {
        return 0.0;
    }

    return environment_.state().entry_price;
}


std::size_t
PositionExposureState::holdingBars() const noexcept
{
    if (!initialized_)
    {
        return 0;
    }

    return environment_.state().holding_bars;
}


const std::string&
PositionExposureState::symbol() const noexcept
{
    static const std::string empty;

    if (!initialized_)
    {
        return empty;
    }

    return environment_.state().symbol;
}

} // namespace devai::strategy