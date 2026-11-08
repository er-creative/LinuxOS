#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace devai::strategy;
int main() {
    const DevOOSPriceProvider price=[](const std::string&,std::int64_t){return 100.0;};
    constexpr std::int64_t day=1760000000;
    // Use UTC epoch aligned to 10:00 IST, with one-minute increments.
    const std::int64_t t=((day+19800)/86400)*86400-19800+10*3600;
    DevOOSLedgerConfig z{};
    z.cost_mode=DevOOSCostMode::ZERODHA_EQUITY_INTRADAY;
    DevRealHistoricalMultiStockOOS ledger(z);
    auto buy=ledger.execute(t,"TEST",price);
    assert(ledger.state().quantity>0);
    assert(buy.brokerage>0 && buy.other_costs>0);
    assert(ledger.state().quantity*100.0+buy.brokerage+buy.other_costs<=5250.0+1e-7);
    auto sell=ledger.execute(t+60,"",price);
    assert(ledger.state().quantity==0);
    assert(sell.other_costs>buy.other_costs); // STT is charged on SELL
    assert(ledger.state().equity<7000.0);
    DevOOSLedgerConfig free{};free.cost_mode=DevOOSCostMode::ZERO_COST;
    DevRealHistoricalMultiStockOOS zero(free);
    auto b=zero.execute(t,"TEST",price);
    auto e=zero.execute(t+60,"",price);
    assert(b.brokerage==0 && b.slippage==0 && b.other_costs==0);
    assert(e.brokerage==0 && e.slippage==0 && e.other_costs==0);
    assert(std::abs(zero.state().equity-7000.0)<1e-8);
    std::cout<<"[CETE][5E2K4] Ledger integration tests PASS\n";
}
