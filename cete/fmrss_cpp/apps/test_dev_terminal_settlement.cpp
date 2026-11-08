#include "devai/strategy/dev_ai_strategy/DevPortfolioEnvironment.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static bool near(double a,double b,double e=1e-8){return std::fabs(a-b)<e;}
int main(){
    DevPortfolioConfig cfg;
    DevPortfolioEnvironment env(cfg);
    const std::int64_t t=1790070240; // 2026-09-22 15:14 IST
    DevPortfolioFrame last{t,{{"RELIANCE",100.0,110.0}},false};
    DevPortfolioFrame cutoff{t+60,{{"RELIANCE",100.0,110.0}},true};
    bool rejected=false;
    try{env.settleSession(cutoff);}catch(const std::logic_error&){rejected=true;}
    assert(rejected);
    auto entry=env.step(last,{"DevAgent_001","RELIANCE",DevPortfolioAction::LONG});
    assert(entry.executed && env.state().position);
    const double before=env.state().equity;
    const auto pos=*env.state().position;
    const double exit_px=110.0*(1.0-cfg.slippage_bps/10000.0);
    const double exit_fee=pos.shares*exit_px*cfg.transaction_cost_bps/10000.0;
    auto settled=env.settleSession(cutoff);
    assert(settled.executed && settled.after.terminal && !settled.after.position);
    assert(settled.reason=="terminal_liquidation_at_last_close");
    assert(near(settled.after.equity,before+pos.shares*(exit_px-110.0)-exit_fee));
    assert(near(settled.reward,(settled.after.equity-before)/cfg.initial_cash));
    rejected=false;
    try{env.step({t+120,{{"RELIANCE",110.0,110.0}},false},{});}catch(const std::logic_error&){rejected=true;}
    assert(rejected);
    env.reset();
    env.step(last,{});
    auto flat=env.settleSession(cutoff);
    assert(!flat.executed && flat.after.terminal && near(flat.after.equity,7000.0) && near(flat.reward,0.0));
    std::cout<<"PASS terminal settlement: close liquidation, fees, slippage, reward, flat state, no second step\n";
}
