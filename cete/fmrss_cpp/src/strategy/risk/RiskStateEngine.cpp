#include "devai/strategy/risk/RiskStateEngine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy::risk
{

namespace
{

constexpr double ABSOLUTE_TOLERANCE =
    1.0e-9;

constexpr double RELATIVE_TOLERANCE =
    1.0e-10;

} // namespace


// ============================================================================
// Numeric helpers
// ============================================================================

bool
RiskStateEngine::finite(
    double value) noexcept
{
    return std::isfinite(value);
}


bool
RiskStateEngine::finiteNonNegative(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value >= 0.0;
}


bool
RiskStateEngine::approximatelyEqual(
    double left,
    double right) noexcept
{
    if (!finite(left) ||
        !finite(right))
    {
        return false;
    }

    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            });

    return
        std::fabs(left - right) <=
        ABSOLUTE_TOLERANCE +
        RELATIVE_TOLERANCE * scale;
}


// ============================================================================
// Position validation
// ============================================================================

void
RiskStateEngine::validatePosition(
    double position)
{
    if (!finite(position))
    {
        throw std::invalid_argument(
            "RiskStateEngine position must be finite.");
    }

    if (!approximatelyEqual(position, -1.0) &&
        !approximatelyEqual(position, 0.0) &&
        !approximatelyEqual(position, 1.0))
    {
        throw std::invalid_argument(
            "RiskStateEngine position must be -1, 0, or +1.");
    }
}


// ============================================================================
// Input validation
// ============================================================================

void
RiskStateEngine::validateInput(
    const RiskStateInput& input)
{
    if (input.decision_time <= 0)
    {
        throw std::invalid_argument(
            "RiskStateEngine decision time must be positive.");
    }

    if (!finite(input.initial_equity) ||
        input.initial_equity <= 0.0)
    {
        throw std::invalid_argument(
            "RiskStateEngine initial equity must be "
            "positive and finite.");
    }

    if (!finite(input.current_equity) ||
        input.current_equity <= 0.0)
    {
        throw std::invalid_argument(
            "RiskStateEngine current equity must be "
            "positive and finite.");
    }

    validatePosition(
        input.position);

    if (!finiteNonNegative(
            input.quantity))
    {
        throw std::invalid_argument(
            "RiskStateEngine quantity must be "
            "finite and non-negative.");
    }

    if (!finite(
            input.current_price) ||
        input.current_price <= 0.0)
    {
        throw std::invalid_argument(
            "RiskStateEngine current price must be "
            "positive and finite.");
    }

    if (approximatelyEqual(
            input.position,
            0.0))
    {
        if (!approximatelyEqual(
                input.quantity,
                0.0))
        {
            throw std::invalid_argument(
                "FLAT risk state must have zero quantity.");
        }
    }
    else
    {
        if (input.quantity <= 0.0)
        {
            throw std::invalid_argument(
                "Open position risk state must have "
                "positive quantity.");
        }
    }
}


// ============================================================================
// Gross exposure
// ============================================================================

double
RiskStateEngine::calculateGrossExposure(
    double position,
    double quantity,
    double current_price)
{
    const double exposure =
        std::fabs(position) *
        quantity *
        current_price;

    if (!finiteNonNegative(exposure))
    {
        throw std::runtime_error(
            "RiskStateEngine generated invalid gross exposure.");
    }

    return exposure;
}


// ============================================================================
// Exposure ratio
// ============================================================================

double
RiskStateEngine::calculateExposureRatio(
    double gross_exposure,
    double current_equity)
{
    if (!finiteNonNegative(gross_exposure) ||
        !finite(current_equity) ||
        current_equity <= 0.0)
    {
        throw std::runtime_error(
            "RiskStateEngine cannot calculate exposure ratio.");
    }

    const double ratio =
        gross_exposure /
        current_equity;

    if (!finiteNonNegative(ratio))
    {
        throw std::runtime_error(
            "RiskStateEngine generated invalid exposure ratio.");
    }

    return ratio;
}


// ============================================================================
// Drawdown
// ============================================================================

double
RiskStateEngine::calculateDrawdown(
    double peak_equity,
    double current_equity)
{
    if (!finite(peak_equity) ||
        peak_equity <= 0.0 ||
        !finite(current_equity) ||
        current_equity <= 0.0)
    {
        throw std::runtime_error(
            "RiskStateEngine cannot calculate drawdown.");
    }

    if (current_equity >= peak_equity)
    {
        return 0.0;
    }

    const double drawdown =
        (peak_equity - current_equity) /
        peak_equity;

    if (!finiteNonNegative(drawdown))
    {
        throw std::runtime_error(
            "RiskStateEngine generated invalid drawdown.");
    }

    return drawdown;
}


// ============================================================================
// reset
// ============================================================================

void
RiskStateEngine::reset(
    const RiskStateInput& input)
{
    validateInput(
        input);

    state_ = {};

    state_.decision_time =
        input.decision_time;

    state_.initial_equity =
        input.initial_equity;

    state_.current_equity =
        input.current_equity;

    state_.peak_equity =
        input.current_equity;

    state_.current_drawdown =
        0.0;

    state_.maximum_drawdown =
        0.0;

    state_.position =
        input.position;

    state_.quantity =
        input.quantity;

    state_.current_price =
        input.current_price;

    state_.gross_exposure =
        calculateGrossExposure(
            input.position,
            input.quantity,
            input.current_price);

    state_.exposure_ratio =
        calculateExposureRatio(
            state_.gross_exposure,
            input.current_equity);

    state_.initialized =
        true;

    state_.numerical_integrity =
        true;

    validate(
        state_);
}


// ============================================================================
// update
// ============================================================================

const RiskState&
RiskStateEngine::update(
    const RiskStateInput& input)
{
    if (!state_.initialized)
    {
        throw std::runtime_error(
            "RiskStateEngine must be reset before update.");
    }

    validateInput(
        input);

    if (input.decision_time <=
        state_.decision_time)
    {
        throw std::invalid_argument(
            "RiskStateEngine decision time must advance strictly.");
    }

    if (!approximatelyEqual(
            input.initial_equity,
            state_.initial_equity))
    {
        throw std::invalid_argument(
            "RiskStateEngine initial equity cannot change "
            "within an episode.");
    }

    state_.decision_time =
        input.decision_time;

    state_.current_equity =
        input.current_equity;

    state_.peak_equity =
        std::max(
            state_.peak_equity,
            input.current_equity);

    state_.current_drawdown =
        calculateDrawdown(
            state_.peak_equity,
            input.current_equity);

    state_.maximum_drawdown =
        std::max(
            state_.maximum_drawdown,
            state_.current_drawdown);

    state_.position =
        input.position;

    state_.quantity =
        input.quantity;

    state_.current_price =
        input.current_price;

    state_.gross_exposure =
        calculateGrossExposure(
            input.position,
            input.quantity,
            input.current_price);

    state_.exposure_ratio =
        calculateExposureRatio(
            state_.gross_exposure,
            input.current_equity);

    state_.numerical_integrity =
        true;

    validate(
        state_);

    return state_;
}


// ============================================================================
// state
// ============================================================================

const RiskState&
RiskStateEngine::state() const noexcept
{
    return state_;
}


// ============================================================================
// initialized
// ============================================================================

bool
RiskStateEngine::initialized() const noexcept
{
    return state_.initialized;
}


// ============================================================================
// validate
// ============================================================================

void
RiskStateEngine::validate(
    const RiskState& state) const
{
    if (!state.initialized)
    {
        throw std::runtime_error(
            "RiskStateEngine state is not initialized.");
    }

    if (state.decision_time <= 0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid decision time.");
    }

    if (!finite(state.initial_equity) ||
        state.initial_equity <= 0.0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid initial equity.");
    }

    if (!finite(state.current_equity) ||
        state.current_equity <= 0.0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid current equity.");
    }

    if (!finite(state.peak_equity) ||
        state.peak_equity <= 0.0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid peak equity.");
    }

    if (state.peak_equity +
            ABSOLUTE_TOLERANCE <
        state.current_equity)
    {
        throw std::runtime_error(
            "RiskStateEngine peak equity is below current equity.");
    }

    validatePosition(
        state.position);

    if (!finiteNonNegative(
            state.quantity))
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid quantity.");
    }

    if (!finite(state.current_price) ||
        state.current_price <= 0.0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid current price.");
    }

    if (!finiteNonNegative(
            state.current_drawdown) ||
        state.current_drawdown >= 1.0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid current drawdown.");
    }

    if (!finiteNonNegative(
            state.maximum_drawdown) ||
        state.maximum_drawdown >= 1.0)
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid maximum drawdown.");
    }

    if (state.maximum_drawdown +
            ABSOLUTE_TOLERANCE <
        state.current_drawdown)
    {
        throw std::runtime_error(
            "RiskStateEngine maximum drawdown is below "
            "current drawdown.");
    }

    if (!finiteNonNegative(
            state.gross_exposure))
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid gross exposure.");
    }

    if (!finiteNonNegative(
            state.exposure_ratio))
    {
        throw std::runtime_error(
            "RiskStateEngine state contains invalid exposure ratio.");
    }

    const double expected_drawdown =
        calculateDrawdown(
            state.peak_equity,
            state.current_equity);

    if (!approximatelyEqual(
            state.current_drawdown,
            expected_drawdown))
    {
        throw std::runtime_error(
            "RiskStateEngine drawdown reconciliation failed.");
    }

    const double expected_exposure =
        calculateGrossExposure(
            state.position,
            state.quantity,
            state.current_price);

    if (!approximatelyEqual(
            state.gross_exposure,
            expected_exposure))
    {
        throw std::runtime_error(
            "RiskStateEngine gross-exposure reconciliation failed.");
    }

    const double expected_exposure_ratio =
        calculateExposureRatio(
            state.gross_exposure,
            state.current_equity);

    if (!approximatelyEqual(
            state.exposure_ratio,
            expected_exposure_ratio))
    {
        throw std::runtime_error(
            "RiskStateEngine exposure-ratio reconciliation failed.");
    }

    if (!state.numerical_integrity)
    {
        throw std::runtime_error(
            "RiskStateEngine numerical integrity is false.");
    }
}

} // namespace devai::strategy::risk