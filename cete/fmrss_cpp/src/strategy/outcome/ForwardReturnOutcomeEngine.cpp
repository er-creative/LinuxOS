#include "devai/strategy/outcome/ForwardReturnOutcomeEngine.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>


namespace devai::strategy::outcome
{

// ============================================================================
// finite
// ============================================================================

bool
ForwardReturnOutcomeEngine::finite(
    double value) noexcept
{
    return std::isfinite(value);
}


// ============================================================================
// validateHorizon
// ============================================================================

void
ForwardReturnOutcomeEngine::validateHorizon(
    std::size_t horizon_bars)
{
    if (horizon_bars == 0)
    {
        throw std::invalid_argument(
            "ForwardReturnOutcomeEngine horizon_bars must be greater than 0.");
    }
}


// ============================================================================
// validateBar
// ============================================================================

void
ForwardReturnOutcomeEngine::validateBar(
    const ForwardReturnBar& bar)
{
    if (bar.decision_time <= 0)
    {
        throw std::invalid_argument(
            "ForwardReturnOutcomeEngine decision_time must be positive.");
    }


    if (!finite(bar.close) ||
        bar.close <= 0.0)
    {
        throw std::invalid_argument(
            "ForwardReturnOutcomeEngine close must be positive and finite.");
    }
}


// ============================================================================
// validateSequence
// ============================================================================

void
ForwardReturnOutcomeEngine::validateSequence(
    const std::vector<ForwardReturnBar>& bars)
{
    if (bars.empty())
    {
        throw std::invalid_argument(
            "ForwardReturnOutcomeEngine historical sequence cannot be empty.");
    }


    validateBar(
        bars.front());


    for (std::size_t index = 1;
         index < bars.size();
         ++index)
    {
        validateBar(
            bars[index]);


        if (bars[index].decision_time <=
            bars[index - 1].decision_time)
        {
            throw std::invalid_argument(
                "ForwardReturnOutcomeEngine historical sequence must have "
                "strictly increasing decision times.");
        }
    }
}


// ============================================================================
// calculate
// ============================================================================

double
ForwardReturnOutcomeEngine::calculate(
    double origin_close,
    double outcome_close)
{
    if (!finite(origin_close) ||
        origin_close <= 0.0)
    {
        throw std::invalid_argument(
            "ForwardReturnOutcomeEngine origin_close "
            "must be positive and finite.");
    }


    if (!finite(outcome_close) ||
        outcome_close <= 0.0)
    {
        throw std::invalid_argument(
            "ForwardReturnOutcomeEngine outcome_close "
            "must be positive and finite.");
    }


    const double result =
        (outcome_close / origin_close) - 1.0;


    if (!finite(result))
    {
        throw std::runtime_error(
            "ForwardReturnOutcomeEngine generated "
            "a non-finite forward return.");
    }


    return result;
}


// ============================================================================
// evaluate
// ============================================================================

ForwardReturnOutcome
ForwardReturnOutcomeEngine::evaluate(
    const std::vector<ForwardReturnBar>& bars,
    std::size_t origin_index,
    std::size_t horizon_bars) const
{
    validateHorizon(
        horizon_bars);


    validateSequence(
        bars);


    if (origin_index >= bars.size())
    {
        throw std::out_of_range(
            "ForwardReturnOutcomeEngine origin_index "
            "is outside the historical sequence.");
    }


    const ForwardReturnBar& origin_bar =
        bars[origin_index];


    ForwardReturnOutcome result;

    result.origin_index =
        origin_index;

    result.horizon_bars =
        horizon_bars;

    result.origin_time =
        origin_bar.decision_time;

    result.origin_close =
        origin_bar.close;


    // ========================================================================
    // Determine whether the exact N-bar future outcome exists.
    //
    // Avoid:
    //
    //     origin_index + horizon_bars
    //
    // because an unchecked addition could overflow size_t.
    // ========================================================================

    const std::size_t remaining_bars =
        (bars.size() - 1U) - origin_index;


    if (horizon_bars > remaining_bars)
    {
        result.available =
            false;

        result.numerical_integrity =
            true;

        return result;
    }


    const std::size_t outcome_index =
        origin_index + horizon_bars;


    const ForwardReturnBar& outcome_bar =
        bars[outcome_index];


    const double forward_return =
        calculate(
            origin_bar.close,
            outcome_bar.close);


    result.outcome_index =
        outcome_index;

    result.outcome_time =
        outcome_bar.decision_time;

    result.outcome_close =
        outcome_bar.close;

    result.forward_return =
        forward_return;

    result.available =
        true;

    result.numerical_integrity =
        true;


    return result;
}


// ============================================================================
// evaluateAll
// ============================================================================

std::vector<ForwardReturnOutcome>
ForwardReturnOutcomeEngine::evaluateAll(
    const std::vector<ForwardReturnBar>& bars,
    std::size_t horizon_bars) const
{
    validateHorizon(
        horizon_bars);


    validateSequence(
        bars);


    std::vector<ForwardReturnOutcome> outcomes;

    outcomes.reserve(
        bars.size());


    for (std::size_t origin_index = 0;
         origin_index < bars.size();
         ++origin_index)
    {
        outcomes.push_back(
            evaluate(
                bars,
                origin_index,
                horizon_bars));
    }


    return outcomes;
}

} // namespace devai::strategy::outcome