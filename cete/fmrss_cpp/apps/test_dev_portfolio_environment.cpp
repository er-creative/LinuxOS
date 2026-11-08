#include "devai/strategy/dev_ai_strategy/DevPortfolioEnvironment.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static DevPortfolioFrame frame(std::int64_t t,double o,double c,bool end=false) {
    return {t,{{"RELIANCE",o,c},{"SBIN",600,600}},end};
}
static bool near(double a,double b,double eps=1e-7){return std::fabs(a-b)<eps;}
int main(){
    DevPortfolioEnvironment env;
    auto a=env.step(frame(1000,100,102),{"DevAgent_001","RELIANCE",DevPortfolioAction::LONG});
    assert(a.executed && a.after.position && a.after.position->shares==52);
    assert(a.after.position->side==1);
    assert(a.after.equity>7000);
    auto b=env.step(frame(1060,103,104),{"DevAgent_002","SBIN",DevPortfolioAction::LONG});
    assert(!b.executed && b.reason=="one_position_limit");
    assert(b.after.position->symbol=="RELIANCE");
    auto c=env.step(frame(1120,105,105),{"DevAgent_001","RELIANCE",DevPortfolioAction::EXIT});
    assert(c.executed && !c.after.position && c.after.cash==c.after.equity);
    auto d=env.step(frame(1180,100,99,true),{"DevAgent_003","RELIANCE",DevPortfolioAction::SHORT});
    assert(d.executed && d.after.terminal && !d.after.position);
    assert(std::isfinite(d.reward) && std::isfinite(d.after.realized_pnl));
    bool threw=false;try {env.step(frame(1240,100,100),{});}catch(const std::logic_error&){threw=true;}assert(threw);
    env.reset();
    assert(near(env.state().equity,7000) && !env.state().position);
    env.step(frame(1000,100,100),{});
    threw=false;try {env.step(frame(1120,100,100),{});}catch(const std::invalid_argument&){threw=true;}assert(threw);
    assert(env.state().time==1000);
    DevPortfolioConfig cfg;cfg.allow_short=false;
    DevPortfolioEnvironment no_short(cfg);
    auto s=no_short.step(frame(1000,100,100),{"DevAgent_001","RELIANCE",DevPortfolioAction::SHORT});
    assert(!s.executed && s.reason=="short_not_allowed");
    std::cout<<"3.19D-3B-4B STATEFUL PORTFOLIO TEST PASSED\n";
}
