#include "devai/strategy/dev_ai_strategy/DevHistoricalExecutionRiskGate_H47_6C1G2B1.hpp"
#include <cassert>
#include <stdexcept>
#include <iostream>
using namespace devai::strategy;
int main() {
    DevHistoricalExecutionRiskGate_H47_6C1G2B1 gate({0.02, 0.05, 0.10});
    constexpr std::int64_t d = 1792122300LL; // 2026-10-16 09:15 IST
    assert(gate.observe(d, 7000.0));
    assert(gate.observe(d+60, 6900.0));
    assert(!gate.observe(d+120, 6850.0));
    assert(!gate.observe(d+180, 7000.0)); // daily loss latch
    assert(gate.observe(d+3*86400, 7000.0)); // next Monday, new week/day
    assert(!gate.observe(d+3*86400+60, 6200.0)); // drawdown latch
    assert(!gate.observe(d+4*86400, 7000.0));
    bool rejected=false;
    try { (void)gate.observe(d+4*86400,7000.0); }
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    std::cout << "6C1G2B1 risk gate PASS\n";
}
