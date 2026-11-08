#include "devai/strategy/dev_ai_strategy/DevZerodhaIntradayCosts.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy {
namespace {
void valid(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
void rate(double x) { valid(std::isfinite(x) && x >= 0.0 && x < 1.0, "invalid Zerodha cost rate"); }
}
double DevZerodhaChargeBreakdown::statutory_total() const noexcept {
    return stt + exchange_transaction + sebi + stamp + gst;
}
double DevZerodhaChargeBreakdown::total() const noexcept {
    return brokerage + statutory_total() + slippage;
}
DevZerodhaIntradayCosts::DevZerodhaIntradayCosts(DevZerodhaChargeRates r) : rates_(r) {
    rate(r.brokerage_rate); rate(r.sell_stt_rate); rate(r.nse_transaction_rate);
    rate(r.bse_transaction_rate); rate(r.sebi_rate); rate(r.buy_stamp_rate);
    rate(r.gst_rate); rate(r.slippage_rate);
    valid(std::isfinite(r.brokerage_cap_per_order) && r.brokerage_cap_per_order >= 0,
          "invalid brokerage cap");
}
DevZerodhaChargeBreakdown DevZerodhaIntradayCosts::calculate(
    DevTradeSide side, std::int64_t quantity, double price, DevExchange exchange) const {
    valid(quantity >= 0, "negative order quantity");
    valid(std::isfinite(price) && price > 0.0, "invalid order price");
    DevZerodhaChargeBreakdown x;
    x.turnover = static_cast<double>(quantity) * price;
    valid(std::isfinite(x.turnover), "non-finite turnover");
    if (!quantity) return x;
    x.brokerage = std::min(x.turnover * rates_.brokerage_rate, rates_.brokerage_cap_per_order);
    x.stt = side == DevTradeSide::SELL ? x.turnover * rates_.sell_stt_rate : 0.0;
    if (rates_.round_stt_per_order) x.stt = std::floor(x.stt + 0.5);
    x.exchange_transaction = x.turnover * (exchange == DevExchange::NSE ?
        rates_.nse_transaction_rate : rates_.bse_transaction_rate);
    x.sebi = x.turnover * rates_.sebi_rate;
    x.stamp = side == DevTradeSide::BUY ? x.turnover * rates_.buy_stamp_rate : 0.0;
    x.gst = rates_.gst_rate * (x.brokerage + x.exchange_transaction + x.sebi);
    x.slippage = x.turnover * rates_.slippage_rate;
    valid(std::isfinite(x.total()), "non-finite total charges");
    return x;
}
}
