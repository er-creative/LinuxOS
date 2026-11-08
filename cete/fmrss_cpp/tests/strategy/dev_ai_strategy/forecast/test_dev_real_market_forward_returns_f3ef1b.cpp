#include "devai/strategy/dev_ai_strategy/forecast/DevRealMarketForwardReturns_F3EF1B.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
int main() {
    using namespace devai::strategy;
    // 2025-01-15 09:15 IST, start-of-minute Unix seconds.
    const std::int64_t start=1736912700;
    assert(f3ef1b::ist_minute_of_day(start)==555);
    std::vector<f3ef1::MinuteClose> bars;
    for(int i=0;i<20;++i) bars.push_back({start+60*i,100.0+i,0});
    auto rows=f3ef1b::generate(bars);
    assert(rows.size()==20);
    assert(rows[0].session_id==20250115);
    assert(rows[0].horizons[3].valid);
    assert(std::abs(rows[0].horizons[3].simple_return-0.10)<1e-12);
    assert(!rows[10].horizons[4].valid);
    bars.erase(bars.begin()+4);
    rows=f3ef1b::generate(bars);
    assert(!rows[0].horizons[3].valid);
    std::cout<<"F3E-F1B REAL MARKET ADAPTER: PASS (IST, 10m, missing candle, horizon)\n";
}
