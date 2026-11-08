#pragma once

#include <cstddef>

#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

namespace devai::strategy
{

// ============================================================================
// SlippageSummary
//
// Phase 2.5 episode-step view of execution-price slippage.
//
// IMPORTANT:
//
// TradingEnvironment remains the authoritative source of:
//
//   - market price
//   - execution price
//   - execution side
//   - quantity
//   - slippage cost
//
// This structure only aggregates and exposes those already-calculated values.
// ============================================================================

struct SlippageSummary
{
    std::size_t execution_count{0};

    double total_turnover{0.0};

    double total_slippage_cost{0.0};

    double buy_slippage_cost{0.0};

    double sell_slippage_cost{0.0};

    std::size_t buy_execution_count{0};

    std::size_t sell_execution_count{0};

    bool has_execution{false};

    bool numerical_integrity{true};
};


// ============================================================================
// SlippageModel
//
// Phase 2.5 integration / aggregation / validation layer.
//
// It does NOT:
//
//   - determine execution prices
//   - apply slippage to equity
//   - calculate brokerage
//   - calculate P&L
//   - modify TradingEnvironment
//
// Those responsibilities remain with the frozen execution environment.
// ============================================================================

class SlippageModel
{
public:

    SlippageSummary summarize(
        const TradingStepResult& step) const;


    void validate(
        const TradingStepResult& step) const;


private:

    static bool finiteNonNegative(
        double value) noexcept;


    static bool approximatelyEqual(
        double left,
        double right) noexcept;
};

} // namespace devai::strategy