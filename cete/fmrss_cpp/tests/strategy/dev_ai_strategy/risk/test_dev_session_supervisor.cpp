#include "devai/strategy/dev_ai_strategy/risk/DevSessionSupervisor.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy::session;
// 2026-09-22 09:30 IST, from actual replay log.
static constexpr std::int64_t start = 1790049600;
static std::int64_t at(int h, int m) { return start + ((h * 60 + m) - (9 * 60 + 30)) * 60; }
int main() {
    assert(entryAllowed(at(14,44)));
    assert(!entryAllowed(at(14,45)));
    assert(targetAllowed("BHARTIARTL", "BHARTIARTL", at(14,46)));
    assert(targetAllowed("BHARTIARTL", "", at(14,46)));
    assert(!targetAllowed("BHARTIARTL", "RELIANCE", at(14,46)));
    assert(!targetAllowed("", "RELIANCE", at(14,46)));
    assert(!targetAllowed("BHARTIARTL", "BHARTIARTL", at(15,15)));
    assert(targetAllowed("BHARTIARTL", "", at(15,15)));
    PositionSnapshot p;
    p.invested=true; p.symbol="BHARTIARTL"; p.entry_time_utc=at(14,35);
    p.decision_time_utc=at(14,45); p.entry_price=300;
    p.last_price=300.05; p.highest_price_since_entry=300.2;
    p.lowest_price_since_entry=299.85; p.stop_loss_price=297;
    auto a=assess(p);
    assert(a.stagnant && a.age_minutes==10 && a.priority==Priority::REVIEW);
    p.forecast_available=true; p.expected_hold_net_advantage_fraction=0;
    a=assess(p); assert(a.priority==Priority::ELEVATED);
    p.last_price=296.95; a=assess(p);
    assert(a.stop_triggered && a.recommendation==Recommendation::EXIT_REQUIRED);
    p.last_price=300.05; p.decision_time_utc=at(15,11);
    a=assess(p); assert(a.priority==Priority::URGENT);
    p.decision_time_utc=at(15,15); a=assess(p);
    assert(a.priority==Priority::MANDATORY);
    std::cout << "D9F4C_SESSION_SUPERVISOR_PASS\n";
}
