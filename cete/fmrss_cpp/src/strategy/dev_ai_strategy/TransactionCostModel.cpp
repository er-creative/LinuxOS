#include "devai/strategy/dev_ai_strategy/TransactionCostModel.hpp"

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
TransactionCostModel::finiteNonNegative(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value >= 0.0;
}


bool
TransactionCostModel::approximatelyEqual(
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

TransactionCostSummary
TransactionCostModel::summarize(
    const TradingStepResult& step) const
{
    TransactionCostSummary summary;

    summary.execution_count =
        step.executions.size();

    summary.has_execution =
        !step.executions.empty();


    for (const auto& execution :
         step.executions)
    {
        // --------------------------------------------------------------------
        // Validate execution-level values
        // --------------------------------------------------------------------

        if (!finiteNonNegative(
                execution.turnover))
        {
            throw std::runtime_error(
                "TransactionCostModel encountered "
                "invalid execution turnover.");
        }


        if (!finiteNonNegative(
                execution.charges.turnover) ||
            !finiteNonNegative(
                execution.charges.brokerage) ||
            !finiteNonNegative(
                execution.charges.stt) ||
            !finiteNonNegative(
                execution.charges.
                    exchange_transaction_charge) ||
            !finiteNonNegative(
                execution.charges.sebi_charge) ||
            !finiteNonNegative(
                execution.charges.stamp_duty) ||
            !finiteNonNegative(
                execution.charges.gst) ||
            !finiteNonNegative(
                execution.charges.total))
        {
            throw std::runtime_error(
                "TransactionCostModel encountered "
                "invalid brokerage charge component.");
        }


        // --------------------------------------------------------------------
        // TradingCharges turnover must agree with ExecutionRecord turnover
        // --------------------------------------------------------------------

        if (!approximatelyEqual(
                execution.turnover,
                execution.charges.turnover))
        {
            throw std::runtime_error(
                "Execution turnover does not match "
                "TradingCharges turnover.");
        }


        // --------------------------------------------------------------------
        // Verify charge component sum
        //
        // Slippage is deliberately excluded.
        //
        // Phase 2.4 = transaction charges.
        // Phase 2.5 = slippage.
        // --------------------------------------------------------------------

        const double expected_total =
            execution.charges.brokerage +
            execution.charges.stt +
            execution.charges.
                exchange_transaction_charge +
            execution.charges.sebi_charge +
            execution.charges.stamp_duty +
            execution.charges.gst;


        if (!approximatelyEqual(
                expected_total,
                execution.charges.total))
        {
            throw std::runtime_error(
                "TradingCharges component sum does not "
                "match total transaction charges.");
        }


        // --------------------------------------------------------------------
        // Aggregate
        // --------------------------------------------------------------------

        summary.turnover +=
            execution.turnover;

        summary.brokerage +=
            execution.charges.brokerage;

        summary.stt +=
            execution.charges.stt;

        summary.exchange_transaction_charge +=
            execution.charges.
                exchange_transaction_charge;

        summary.sebi_charge +=
            execution.charges.sebi_charge;

        summary.stamp_duty +=
            execution.charges.stamp_duty;

        summary.gst +=
            execution.charges.gst;

        summary.total_charges +=
            execution.charges.total;
    }


    // ------------------------------------------------------------------------
    // Final aggregate integrity
    // ------------------------------------------------------------------------

    const double aggregate_component_sum =
        summary.brokerage +
        summary.stt +
        summary.exchange_transaction_charge +
        summary.sebi_charge +
        summary.stamp_duty +
        summary.gst;


    if (!approximatelyEqual(
            aggregate_component_sum,
            summary.total_charges))
    {
        throw std::runtime_error(
            "Aggregated transaction-cost component sum "
            "does not match total charges.");
    }


    // ------------------------------------------------------------------------
    // TradingEnvironment step-level agreement
    // ------------------------------------------------------------------------

    if (!finiteNonNegative(
            step.brokerage_charges))
    {
        throw std::runtime_error(
            "TradingStepResult contains invalid "
            "brokerage charges.");
    }


    if (!approximatelyEqual(
            summary.total_charges,
            step.brokerage_charges))
    {
        throw std::runtime_error(
            "TransactionCostModel total does not match "
            "TradingEnvironment brokerage charges.");
    }


    summary.numerical_integrity =
        true;


    return summary;
}


// ============================================================================
// Validate
// ============================================================================

void
TransactionCostModel::validate(
    const TradingStepResult& step) const
{
    (void)summarize(step);
}

} // namespace devai::strategy