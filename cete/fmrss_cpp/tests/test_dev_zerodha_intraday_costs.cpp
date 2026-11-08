#include "devai/strategy/dev_ai_strategy/DevZerodhaIntradayCosts.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static bool near(double a, double b) { return std::abs(a-b) < 1e-9; }
int main() {
    DevZerodhaIntradayCosts calc;
    auto buy=calc.calculate(DevTradeSide::BUY, 50, 100.0);
    auto sell=calc.calculate(DevTradeSide::SELL, 50, 100.0);
    assert(near(buy.turnover,5000.0));
    assert(near(buy.brokerage,1.5));
    assert(near(buy.stamp,0.15));
    assert(near(buy.stt,0.0));
    assert(near(sell.stt,1.25));
    assert(near(sell.stamp,0.0));
    assert(near(buy.exchange_transaction,0.1535));
    assert(near(buy.sebi,0.005));
    assert(near(buy.gst,0.18*(1.5+0.1535+0.005)));
    assert(near(calc.calculate(DevTradeSide::BUY, 1000, 100.0).brokerage,20.0));
    assert(near(calc.calculate(DevTradeSide::BUY, 0, 100.0).total(),0.0));
    DevZerodhaChargeRates r; r.slippage_rate=0.0005;
    assert(near(DevZerodhaIntradayCosts(r).calculate(DevTradeSide::BUY,50,100).slippage,2.5));
    r.round_stt_per_order=true;
    assert(near(DevZerodhaIntradayCosts(r).calculate(DevTradeSide::SELL,50,100).stt,1.0));
    bool rejected=false;
    try { (void)calc.calculate(DevTradeSide::BUY,-1,100); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    std::cout << "[CETE][5E2K4] Zerodha cost-model tests PASS\n";
}
