#include "devai/strategy/dev_ai_strategy/DevIntradayPolicyD9A.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static std::int64_t at(int hour, int minute) { return 20000LL*1440 + hour*60 + minute; }
int main() {
    DevIntradayPolicyD9A policy;
    DevD9AInput x;
    x.decision_key=at(10,30); x.long_profit_probability=0.8; x.short_profit_probability=0.9;
    x.expected_long_net_return=0.004; x.expected_short_net_return=0.009; x.stock_shortable=true;
    assert(policy.evaluate(x).action==DevD9AAction::OPEN_LONG);
    x.position=DevD9APosition::SHORT;
    assert(policy.evaluate(x).action==DevD9AAction::CLOSE_SHORT);
    x.position=DevD9APosition::LONG;
    assert(policy.evaluate(x).action==DevD9AAction::HOLD_LONG);
    x.decision_key=at(15,15);
    assert(policy.evaluate(x).action==DevD9AAction::FORCE_EXIT);
    x.position=DevD9APosition::FLAT;
    assert(policy.evaluate(x).action==DevD9AAction::HOLD_FLAT);
    DevD9ATradingConfig both; both.enable_short=true;
    DevIntradayPolicyD9A combined(both);
    x.decision_key=at(10,30);
    assert(combined.evaluate(x).action==DevD9AAction::OPEN_SHORT);
    x.stock_shortable=false;
    assert(combined.evaluate(x).action==DevD9AAction::OPEN_LONG);
    x.stock_shortable=true; x.decision_key=at(15,15);
    assert(combined.evaluate(x).action==DevD9AAction::HOLD_FLAT);
    x.position=DevD9APosition::SHORT;
    assert(combined.evaluate(x).action==DevD9AAction::FORCE_EXIT);
    bool threw=false;
    try { DevD9ATradingConfig bad; bad.allow_overnight=true; DevIntradayPolicyD9A invalid(bad); }
    catch (const std::invalid_argument&) { threw=true; }
    assert(threw);
    std::cout << "PASS 5C-3I-D9A long/short intraday permissions and forced-exit contract\n";
}
