#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace devai::strategy::brokerage
{

namespace
{

void requireFiniteNonNegative(
    double value,
    const char* name)
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument(
            std::string(name) +
            " must be finite.");
    }

    if (value < 0.0)
    {
        throw std::invalid_argument(
            std::string(name) +
            " must be >= 0.");
    }
}

} // namespace


// ============================================================
// CONSTRUCTOR
// ============================================================

BrokerageChargesEngine::BrokerageChargesEngine(
    BrokerageChargesConfig config)
    :
    config_(config)
{
    requireFiniteNonNegative(
        config_.brokerage_rate,
        "brokerage_rate");

    requireFiniteNonNegative(
        config_.brokerage_cap_per_order,
        "brokerage_cap_per_order");

    requireFiniteNonNegative(
        config_.stt_sell_rate,
        "stt_sell_rate");

    requireFiniteNonNegative(
        config_.exchange_transaction_rate,
        "exchange_transaction_rate");

    requireFiniteNonNegative(
        config_.sebi_rate,
        "sebi_rate");

    requireFiniteNonNegative(
        config_.stamp_duty_buy_rate,
        "stamp_duty_buy_rate");

    requireFiniteNonNegative(
        config_.gst_rate,
        "gst_rate");
}


// ============================================================
// CALCULATE
// ============================================================

TradingCharges BrokerageChargesEngine::calculate(
    ExecutionSide side,
    double turnover) const
{
    requireFiniteNonNegative(
        turnover,
        "turnover");

    TradingCharges result;

    result.turnover =
        turnover;

    if (turnover == 0.0)
    {
        return result;
    }


    // --------------------------------------------------------
    // Brokerage
    // --------------------------------------------------------

    result.brokerage =
        std::min(
            turnover *
                config_.brokerage_rate,
            config_.brokerage_cap_per_order);


    // --------------------------------------------------------
    // STT
    // Intraday: sell side only
    // --------------------------------------------------------

    if (side == ExecutionSide::SELL)
    {
        result.stt =
            turnover *
            config_.stt_sell_rate;
    }


    // --------------------------------------------------------
    // Exchange transaction charge
    // --------------------------------------------------------

    result.exchange_transaction_charge =
        turnover *
        config_.exchange_transaction_rate;


    // --------------------------------------------------------
    // SEBI charge
    // --------------------------------------------------------

    result.sebi_charge =
        turnover *
        config_.sebi_rate;


    // --------------------------------------------------------
    // Stamp duty
    // Buy side only
    // --------------------------------------------------------

    if (side == ExecutionSide::BUY)
    {
        result.stamp_duty =
            turnover *
            config_.stamp_duty_buy_rate;
    }


    // --------------------------------------------------------
    // GST
    // --------------------------------------------------------

    const double gst_base =
        result.brokerage +
        result.exchange_transaction_charge +
        result.sebi_charge;

    result.gst =
        gst_base *
        config_.gst_rate;


    // --------------------------------------------------------
    // Total
    // --------------------------------------------------------

    result.total =
        result.brokerage +
        result.stt +
        result.exchange_transaction_charge +
        result.sebi_charge +
        result.stamp_duty +
        result.gst;


    if (!std::isfinite(result.total))
    {
        throw std::runtime_error(
            "BrokerageChargesEngine produced non-finite charges.");
    }

    return result;
}


// ============================================================
// CONFIG
// ============================================================

const BrokerageChargesConfig&
BrokerageChargesEngine::config() const noexcept
{
    return config_;
}

} // namespace devai::strategy::brokerage