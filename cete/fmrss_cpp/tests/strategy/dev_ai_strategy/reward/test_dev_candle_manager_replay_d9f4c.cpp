#include "devai/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    using namespace devai::strategy;
    using namespace devai::strategy::d9f4c;
    DevOOSLedgerState s{};
    s.cash=4900.0; s.quantity=2;
    assert(std::fabs(DevCandleManagerReplay::markedEquity(s,1050.0)-7000.0)<1e-9);
    bool rejected=false;
    s.quantity=-1;
    try { (void)DevCandleManagerReplay::markedEquity(s,100.0); }
    catch(const std::runtime_error&) { rejected=true; }
    assert(rejected);
    assert(DevCandleManagerReplay::minuteIST(0)==330);
    std::cout << "D9F4C_CANDLE_REPLAY_UNIT_PASS mark=1 short_rejection=1 time=1 "
                 "real_candles_tested=0 model_trained=0\n";
}
