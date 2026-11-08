#include "devai/strategy/dev_ai_strategy/DevEventDrivenRotationReplay.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    const auto dir=std::filesystem::temp_directory_path()/"cete_4c3_fixture";
    std::filesystem::create_directories(dir);
    for(const std::string sym:{"RELIANCE","SBIN"}) {
        std::ofstream f(dir/(sym+"_1min.txt"));
        f<<"Symbol,Date,Time,Open,High,Low,Close,Volume\n";
        for(int i=15;i<=24;++i) {
            const double px=sym=="RELIANCE"?100.0:50.0;
            f<<sym<<",2026-10-09,09:"<<i<<":00,"<<px<<","<<px<<","<<px<<","<<px<<",100\n";
        }
    }
    DevRealMinuteMarket market;
    for(const std::string sym:{"RELIANCE","SBIN"}) market.load(sym,dir/(sym+"_1min.txt"));
    // IST 2026-10-09 09:15, derived using an epoch-independent bar lookup.
    // Parse once using a known fixed epoch: 2026-10-09 03:45 UTC.
    const std::int64_t start=1791517500;
    auto setup=[]() {
        DevSupervisorConfig cfg{};cfg.initial_capital=100000;cfg.global_max_exposure_fraction=1.0;
        cfg.max_open_positions=1;cfg.transaction_cost_bps=0;cfg.slippage_bps=0;
        return cfg;
    };
    auto run=[&](){
        DevinderjeetSupervisor supervisor(setup());
        supervisor.registerManager({"DevManager_001",{"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"},100000});
        DevEventDrivenRotationReplay engine;
        const auto policy=[&](std::int64_t t)->DevRotationIntent {
            if(t==start+60) return {DevRotationAction::BUY,"RELIANCE","DevAgent_001"};
            if(t==start+180) return {DevRotationAction::ROTATE,"SBIN","DevAgent_002"};
            return {DevRotationAction::HOLD,"",""};
        };
        const auto result=engine.run({start+60,start+120,start+180,start+240,start+300},
            "DevManager_001",policy,supervisor,market,0.50,true);
        assert(result.decision_count==5);
        assert(result.rotation_count==1);
        assert(result.replay.filled==4); // BUY, EXIT, BUY, terminal EXIT
        assert(result.replay.rejected==0);
        assert(result.replay.events.size()==4);
        assert(result.replay.events[0].time==start+60); // 09:16 next open
        assert(result.replay.events[1].time==start+180); // 09:18 exit
        assert(result.replay.events[2].time==start+180); // 09:18 entry
        assert(result.replay.events[3].action=="TERMINAL_EXIT");
        assert(supervisor.snapshot().open_positions==0);
        assert(result.replay.net_pnl==0);
        return result;
    };
    const auto a=run(),b=run();
    assert(a.replay.net_pnl==b.replay.net_pnl);
    assert(a.replay.events.size()==b.replay.events.size());
    std::filesystem::remove_all(dir);
    std::cout<<"PASS event-driven rotation: next-open execution, ordered exit-before-buy, one manager, terminal settlement, deterministic replay\n";
}
