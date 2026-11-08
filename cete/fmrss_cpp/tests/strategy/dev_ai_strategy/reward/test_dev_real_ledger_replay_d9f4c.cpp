#include "devai/strategy/dev_ai_strategy/reward/DevRealLedgerReplay_D9F4C.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;
using namespace devai::strategy::d9f4c;
namespace {
void require(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
template<class F> void rejects(F f) {
    bool threw=false;
    try { f(); } catch (const std::exception&) { threw=true; }
    require(threw,"expected rejection");
}
ReplayDecision event(std::int64_t t, std::size_t action,
                     std::string desired, bool terminal=false) {
    ReplayDecision d{};
    d.decision_time=t; d.manager_action=action;
    d.desired_symbol=std::move(desired); d.terminal=terminal;
    d.legal_actions={true,true,true,true,true,true,true};
    return d;
}
}
int main() {
    // 09:30, 09:31, 09:32, 15:15 IST on the same synthetic trading day.
    constexpr std::int64_t base=86400*20000-19800;
    const auto t0=base+9*3600+30*60;
    const auto t1=t0+60;
    const auto t2=t1+60;
    const auto close=base+15*3600+15*60;
    DevOOSLedgerConfig cfg{};
    cfg.initial_capital=7000;
    cfg.max_allocation_fraction=0.75;
    cfg.brokerage_rate=0.001;
    cfg.slippage_rate=0.001;
    cfg.other_cost_rate=0.0005;
    cfg.allow_intraday_short=false;
    const DevOOSPriceProvider prices=[&](const std::string& s, std::int64_t t) {
        if (s=="AAA") return t>=t2 ? 102.0 : 100.0;
        if (s=="BBB") return t>=close ? 202.0 : 200.0;
        throw std::invalid_argument("unknown symbol");
    };
    const std::vector<ReplayDecision> decisions={
        event(t0,2,"AAA"), event(t1,1,"AAA"),
        event(t2,3,"BBB"), event(close,0,"",true)
    };
    auto r=DevRealLedgerReplay::run(decisions,prices,cfg);
    require(r.steps.size()==4,"missing actual replay events");
    require(r.steps[0].execution.executed_legs==1,"entry not executed");
    require(r.steps[1].execution.executed_legs==0,"hold executed a leg");
    require(r.steps[2].execution.executed_legs==2,"switch not executed");
    require(r.steps[3].execution.executed_legs==1,"exit not executed");
    require(r.final_portfolio.quantity==0 && r.final_portfolio.held_symbol.empty(),"not flat");
    for (const auto& step:r.steps) {
        require(std::isfinite(step.gross.selected_reward),"gross reward invalid");
        require(std::isfinite(step.net.selected_reward),"net reward invalid");
        require(step.gross.selected_reward + 1e-10 >= step.net.selected_reward,
                "gross reward smaller than net reward");
    }
    auto bad=decisions; bad[0].desired_symbol="SHORT:AAA";
    rejects([&]{ (void)DevRealLedgerReplay::run(bad,prices,cfg); });
    bad=decisions; bad[0].legal_actions[2]=false;
    rejects([&]{ (void)DevRealLedgerReplay::run(bad,prices,cfg); });
    bad=decisions; bad[3].desired_symbol="BBB";
    rejects([&]{ (void)DevRealLedgerReplay::run(bad,prices,cfg); });
    std::cout << "D9F4C_REAL_LEDGER_REPLAY_PASS events=4 entry=1 hold=1 switch=1 exit=1 gross_net=1 terminal=1 model_trained=0\n";
}
