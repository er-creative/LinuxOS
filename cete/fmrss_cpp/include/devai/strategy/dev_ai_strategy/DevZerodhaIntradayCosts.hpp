#pragma once
#include <cstdint>

namespace devai::strategy {
enum class DevExchange { NSE, BSE };
enum class DevTradeSide { BUY, SELL };

struct DevZerodhaChargeRates {
    double brokerage_rate = 0.0003;
    double brokerage_cap_per_order = 20.0;
    double sell_stt_rate = 0.00025;
    double nse_transaction_rate = 0.0000307;
    double bse_transaction_rate = 0.0000375;
    double sebi_rate = 0.000001; // Rs 10 / crore
    double buy_stamp_rate = 0.00003;
    double gst_rate = 0.18;
    double slippage_rate = 0.0; // scenario assumption, not a Zerodha fee
    bool round_stt_per_order = false; // true only when matching order-level contract note rules
};

struct DevZerodhaChargeBreakdown {
    double turnover = 0.0;
    double brokerage = 0.0;
    double stt = 0.0;
    double exchange_transaction = 0.0;
    double sebi = 0.0;
    double stamp = 0.0;
    double gst = 0.0;
    double slippage = 0.0;
    double statutory_total() const noexcept;
    double total() const noexcept;
};

class DevZerodhaIntradayCosts {
public:
    explicit DevZerodhaIntradayCosts(DevZerodhaChargeRates rates = {});
    DevZerodhaChargeBreakdown calculate(DevTradeSide side, std::int64_t quantity,
                                        double price, DevExchange exchange = DevExchange::NSE) const;
private:
    DevZerodhaChargeRates rates_;
};
}
