#pragma once

#include <cstddef>

#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

namespace devai::strategy
{

// ============================================================================
// TransactionCostSummary
//
// Phase 2.4 episode-level transaction-cost view.
//
// IMPORTANT:
//
// This structure does NOT calculate brokerage rates.
//
// The authoritative economic calculations are performed by the shared
// CETE BrokerageChargesEngine through TradingEnvironment.
//
// Phase 2.4 only aggregates and validates the execution-level charges
// already produced by TradingEnvironment.
// ============================================================================

struct TransactionCostSummary
{
    std::size_t execution_count{0};

    double turnover{0.0};

    double brokerage{0.0};

    double stt{0.0};

    double exchange_transaction_charge{0.0};

    double sebi_charge{0.0};

    double stamp_duty{0.0};

    double gst{0.0};

    double total_charges{0.0};

    bool has_execution{false};

    bool numerical_integrity{true};
};


// ============================================================================
// TransactionCostModel
//
// Phase 2.4 integration/audit layer.
//
// Responsibilities:
//
//   - aggregate TradingEnvironment execution charges
//   - validate execution-level transaction costs
//   - verify component-sum integrity
//   - verify agreement with TradingStepResult
//
// It deliberately does NOT:
//
//   - define brokerage rates
//   - calculate STT rules
//   - calculate GST rules
//   - calculate stamp-duty rules
//   - calculate slippage
//   - modify equity
//
// Those responsibilities remain with the already validated CETE components.
// ============================================================================

class TransactionCostModel
{
public:

    TransactionCostSummary summarize(
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
