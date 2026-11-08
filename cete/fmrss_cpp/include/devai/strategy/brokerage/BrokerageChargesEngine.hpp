#pragma once

namespace devai::strategy::brokerage
{

// ============================================================
// EXECUTION SIDE
// ============================================================

enum class ExecutionSide
{
    BUY,
    SELL
};


// ============================================================
// BROKERAGE / STATUTORY CHARGE CONFIGURATION
// ============================================================
//
// This component is intentionally strategy-independent.
//
// DevAI, PCA and future strategies can all use the same
// brokerage implementation.
//
// Rates are decimal fractions.
//
// Example:
//
//     0.0003 = 0.03%
//
// Defaults represent NSE equity-intraday charges configured
// for the current broker model.
//
// Keep rates configurable because broker/exchange/regulatory
// charges can change.
// ============================================================

struct BrokerageChargesConfig
{
    // Brokerage:
    // lower of percentage brokerage or cap per executed order.
    double brokerage_rate{0.0003};
    double brokerage_cap_per_order{20.0};

    // Intraday STT - sell side.
    double stt_sell_rate{0.00025};

    // NSE transaction charge.
    double exchange_transaction_rate{0.0000307};

    // SEBI turnover charge.
    // Rs.10 per crore = 10 / 10,000,000.
    double sebi_rate{0.000001};

    // Intraday stamp duty - buy side.
    double stamp_duty_buy_rate{0.00003};

    // GST on:
    // brokerage + exchange transaction charge + SEBI charge.
    double gst_rate{0.18};
};


// ============================================================
// CHARGE BREAKDOWN
// ============================================================

struct TradingCharges
{
    double turnover{0.0};

    double brokerage{0.0};
    double stt{0.0};

    double exchange_transaction_charge{0.0};
    double sebi_charge{0.0};

    double stamp_duty{0.0};
    double gst{0.0};

    double total{0.0};
};


// ============================================================
// BROKERAGE CHARGES ENGINE
// ============================================================

class BrokerageChargesEngine
{
public:

    explicit BrokerageChargesEngine(
        BrokerageChargesConfig config = {});


    [[nodiscard]]
    TradingCharges calculate(
        ExecutionSide side,
        double turnover) const;


    [[nodiscard]]
    const BrokerageChargesConfig&
    config() const noexcept;


private:

    BrokerageChargesConfig config_;
};

} // namespace devai::strategy::brokerage