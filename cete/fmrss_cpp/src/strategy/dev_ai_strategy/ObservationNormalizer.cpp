#include "devai/strategy/dev_ai_strategy/ObservationNormalizer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>


namespace devai::strategy
{

namespace
{

double clamp(
    double value,
    double lower,
    double upper)
{
    return
        std::clamp(
            value,
            lower,
            upper);
}


double squashReturn(
    double value)
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument(
            "Portfolio return must be finite.");
    }

    // Deterministic bounded transformation.
    //
    // No fitted mean/std is used, therefore no
    // training/test leakage is introduced.

    return std::tanh(
        10.0 * value);
}

} // anonymous namespace


ObservationNormalizer::ObservationNormalizer(
    double maximum_holding_bars)
    :
    maximum_holding_bars_(
        maximum_holding_bars)
{
    if (!std::isfinite(
            maximum_holding_bars_) ||
        maximum_holding_bars_ <= 0.0)
    {
        throw std::invalid_argument(
            "Maximum holding bars must be positive.");
    }
}


double
ObservationNormalizer::maximumHoldingBars() const noexcept
{
    return maximum_holding_bars_;
}


std::array<double, DEV_OBSERVATION_SIZE>
ObservationNormalizer::normalize(
    const DevAIObservation& observation) const
{
    if (!observation.numerical_integrity ||
        !observation.ready)
    {
        throw std::invalid_argument(
            "Cannot normalize an invalid "
            "Dev observation.");
    }


    std::array<double, DEV_OBSERVATION_SIZE>
        output{};


    std::size_t cursor =
        0;


    // ========================================================================
    // 39 CETE market features
    // ========================================================================

    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        const double value =
            observation.market[i];


        if (!std::isfinite(value))
        {
            throw std::runtime_error(
                "Dev market feature is non-finite.");
        }


        output[cursor++] =
            clamp(
                value,
                -1.0,
                1.0);
    }


    // ========================================================================
    // 39 availability masks
    // ========================================================================

    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        output[cursor++] =
            observation.availability[i] > 0.5
                ? 1.0
                : 0.0;
    }


    // ========================================================================
    // 6 portfolio features
    // ========================================================================

    const auto& p =
        observation.portfolio;


    output[cursor++] =
        clamp(
            p.position,
            -1.0,
            1.0);


    output[cursor++] =
        squashReturn(
            p.unrealized_return);


    output[cursor++] =
        squashReturn(
            p.realized_return);


    output[cursor++] =
        squashReturn(
            p.equity_return);


    if (!std::isfinite(p.drawdown))
    {
        throw std::invalid_argument(
            "Drawdown must be finite.");
    }


    output[cursor++] =
        clamp(
            p.drawdown,
            0.0,
            1.0);


    if (!std::isfinite(
            p.holding_period))
    {
        throw std::invalid_argument(
            "Holding period must be finite.");
    }


    output[cursor++] =
        clamp(
            p.holding_period /
                maximum_holding_bars_,
            0.0,
            1.0);


    if (cursor !=
        DEV_OBSERVATION_SIZE)
    {
        throw std::runtime_error(
            "Dev observation-size invariant failed.");
    }


    return output;
}

} // namespace devai::strategy