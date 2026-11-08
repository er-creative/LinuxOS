#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static std::int64_t at(int h,int m) {
    using namespace std::chrono;
    return duration_cast<seconds>((sys_days{year{2026}/10/16}).time_since_epoch()
        +hours{h}+minutes{m}-minutes{330}).count();
}
int main() {
    DevOOSLedgerConfig cfg{};
    cfg.allow_intraday_short=false;
    cfg.cost_mode=DevOOSCostMode::LEGACY_PERCENT;
    cfg.brokerage_rate=.0003;
    DevRealHistoricalMultiStockOOS ledger(cfg);
    auto price=[](const std::string& s,std::int64_t) {return s=="AAA"?100.0:50.0;};
    auto opened=ledger.execute(at(14,44),"AAA",price);
    assert(opened.executed_legs==1 && ledger.state().quantity>0);
    const auto q=ledger.state().quantity;
    auto no_switch=ledger.execute(at(14,45),"BBB",price);
    assert(no_switch.executed_legs==1 && ledger.state().quantity==0);
    assert(ledger.state().held_symbol.empty());
    auto no_entry=ledger.execute(at(14,46),"AAA",price);
    assert(no_entry.executed_legs==0 && ledger.state().quantity==0);
    bool short_rejected=false;
    try {(void)ledger.execute(at(14,47),"SHORT:AAA",price);}
    catch(const std::invalid_argument&){short_rejected=true;}
    assert(short_rejected);
    auto terminal=ledger.execute(at(15,15),"AAA",price);
    assert(terminal.executed_legs==0 && ledger.state().quantity==0);
    assert(std::fabs(ledger.state().equity - (ledger.state().cash+ledger.state().quantity*100.0))<1e-6);
    (void)q;
    std::cout<<"6C1G2B1 ledger safety PASS\n";
}
