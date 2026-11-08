#include "devai/strategy/dev_ai_strategy/SlippageModel.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy
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
SlippageModel::finiteNonNegative(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value >= 0.0;
}


bool
SlippageModel::approximatelyEqual(
    double left,
    double right) noexcept
{
    if (!std::isfinite(left) ||
        !std::isfinite(right))
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
// Summarize one TradingEnvironment step
// ============================================================================

SlippageSummary
SlippageModel::summarize(
    const TradingStepResult& step) const
{
    SlippageSummary summary;

    summary.execution_count =
        step.executions.size();

    summary.has_execution =
        !step.executions.empty();


    for (const auto& execution :
         step.executions)
    {
        // --------------------------------------------------------------------
        // Basic execution integrity
        // --------------------------------------------------------------------

        if (!finiteNonNegative(
                execution.market_price) ||
            execution.market_price <= 0.0)
        {
            throw std::runtime_error(
                "SlippageModel encountered invalid "
                "market price.");
        }


        if (!finiteNonNegative(
                execution.execution_price) ||
            execution.execution_price <= 0.0)
        {
            throw std::runtime_error(
                "SlippageModel encountered invalid "
                "execution price.");
        }


        if (!finiteNonNegative(
                execution.quantity))
        {
            throw std::runtime_error(
                "SlippageModel encountered invalid "
                "execution quantity.");
        }


        if (!finiteNonNegative(
                execution.turnover))
        {
            throw std::runtime_error(
                "SlippageModel encountered invalid "
                "execution turnover.");
        }


        if (!finiteNonNegative(
                execution.slippage_cost))
        {
            throw std::runtime_error(
                "SlippageModel encountered invalid "
                "slippage cost.");
        }


        // --------------------------------------------------------------------
        // Execution direction integrity
        //
        // BUY must never receive a price below market from a positive/adverse
        // slippage model.
        //
        // SELL must never receive a price above market.
        //
        // Equality remains legal when slippage_rate == 0.
        // --------------------------------------------------------------------

        if (execution.side ==
            brokerage::ExecutionSide::BUY)
        {
            if (execution.execution_price +
                    ABSOLUTE_TOLERANCE <
                execution.market_price)
            {
                throw std::runtime_error(
                    "BUY execution received favorable "
                    "slippage direction.");
            }


            ++summary.buy_execution_count;

            summary.buy_slippage_cost +=
                execution.slippage_cost;
        }
        else if (execution.side ==
                 brokerage::ExecutionSide::SELL)
        {
            if (execution.execution_price >
                execution.market_price +
                    ABSOLUTE_TOLERANCE)
            {
                throw std::runtime_error(
                    "SELL execution received favorable "
                    "slippage direction.");
            }


            ++summary.sell_execution_count;

            summary.sell_slippage_cost +=
                execution.slippage_cost;
        }
        else
        {
            throw std::runtime_error(
                "SlippageModel encountered unknown "
                "execution side.");
        }


        // --------------------------------------------------------------------
        // Slippage-cost identity
        //
        // Absolute execution-price displacement × quantity.
        // --------------------------------------------------------------------

        const double expected_slippage_cost =
            std::fabs(
                execution.execution_price -
                execution.market_price) *
            execution.quantity;


        if (!approximatelyEqual(
                expected_slippage_cost,
                execution.slippage_cost))
        {
            throw std::runtime_error(
                "Execution slippage cost does not match "
                "execution-price displacement.");
        }


        // --------------------------------------------------------------------
        // Aggregate
        // --------------------------------------------------------------------

        summary.total_turnover +=
            execution.turnover;

        summary.total_slippage_cost +=
            execution.slippage_cost;
    }


    // ------------------------------------------------------------------------
    // Aggregate side reconciliation
    // ------------------------------------------------------------------------

    if (summary.buy_execution_count +
            summary.sell_execution_count !=
        summary.execution_count)
    {
        throw std::runtime_error(
            "Slippage execution-side count mismatch.");
    }


    if (!approximatelyEqual(
            summary.buy_slippage_cost +
                summary.sell_slippage_cost,
            summary.total_slippage_cost))
    {
        throw std::runtime_error(
            "BUY/SELL slippage aggregation mismatch.");
    }


    // ------------------------------------------------------------------------
    // TradingStepResult reconciliation
    // ------------------------------------------------------------------------

    if (!finiteNonNegative(
            step.slippage_cost))
    {
        throw std::runtime_error(
            "TradingStepResult contains invalid "
            "slippage cost.");
    }


    if (!approximatelyEqual(
            summary.total_slippage_cost,
            step.slippage_cost))
    {
        throw std::runtime_error(
            "SlippageModel total does not match "
            "TradingEnvironment step slippage.");
    }


    summary.numerical_integrity =
        true;


    return summary;
}


// ============================================================================
// Validate
// ============================================================================

void
SlippageModel::validate(
    const TradingStepResult& step) const
{
    (void)summarize(step);
}

} // namespace devai::strategy