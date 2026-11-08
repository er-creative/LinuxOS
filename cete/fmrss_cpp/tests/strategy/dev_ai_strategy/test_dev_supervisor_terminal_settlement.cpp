#include "devai/strategy/dev_ai_strategy/DevSupervisorTerminalSettlement.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    DevSupervisorConfig cfg; cfg.initial_capital=100000; cfg.global_max_exposure_fraction=.75;
    cfg.max_open_positions=5;
    DevinderjeetSupervisor supervisor(cfg);
    supervisor.registerManager({"DevManager_001",{"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"},100000});
    auto buy=supervisor.execute({"buy-1","DevManager_001","DevAgent_001","RELIANCE",DevSupervisedAction::BUY,1000,10,100});
    assert(buy.status==DevOrderStatus::FILLED);
    supervisor.setHalted(true); // Risk halt must still allow exits.
    bool stale_rejected=false;
    try { (void)DevSupervisorTerminalSettlement::settle(supervisor,{"DevManager_001"},1200,{{"RELIANCE",{1080,101}}}); }
    catch (const std::invalid_argument&) { stale_rejected=true; }
    assert(stale_rejected);
    assert(supervisor.snapshot().open_positions==1);
    auto result=DevSupervisorTerminalSettlement::settle(supervisor,{"DevManager_001"},1200,{{"RELIANCE",{1140,101}}});
    assert(result.reports.size()==1);
    assert(result.reports[0].status==DevOrderStatus::FILLED);
    assert(result.after.open_positions==0);
    assert(std::isfinite(result.after.equity));
    auto retry=DevSupervisorTerminalSettlement::settle(supervisor,{"DevManager_001"},1200,{{"RELIANCE",{1140,101}}});
    assert(retry.reports.empty());
    std::cout << "PASS supervisor terminal settlement: completed-close, stale rejection, halted exits, flat, repeat-safe\n";
}
