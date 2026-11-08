#include "devai/runtime/DevModeAwareExecutionRouter.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace devai;
int main(int argc,char**argv) {
    if(argc!=2) return 2;
    auto cfg=config::loadDualManagerActivation(argv[1]);
    const auto path=std::filesystem::temp_directory_path()/"cete_mode_router_registry";
    std::filesystem::remove_all(path);
    DevManagerRegistry registry(path);
    const std::vector<std::string> stocks={"RELIANCE","SBIN","ITC","NTPC","HDFCBANK","AXISBANK","ICICIBANK","BHARTIARTL","HCLTECH","ADANIPORTS"};
    registry.create_manager(stocks);
    registry.create_manager(stocks);
    int technical=0,portfolio=0;
    runtime::DevDualManagerRuntimeBridge bridge(cfg,registry,
        [&](const runtime::ManagerRuntimeStatus&){++technical;},
        [&](const runtime::ManagerRuntimeStatus&){++portfolio;});
    runtime::DevModeAwareExecutionRouter router(cfg,bridge);
    assert(router.selectedRoute("DevManager_001")==runtime::RouteKind::PAPER);
    assert(router.selectedRoute("DevManager_002")==runtime::RouteKind::BACKTEST);
    assert(router.dispatch("DevManager_001")==runtime::RouteResult::BACKEND_NOT_CONNECTED);
    assert(router.dispatch("DevManager_002")==runtime::RouteResult::MANAGER_INACTIVE);
    int paper=0, backtest=0, forward=0;
    router.connectTestingBackend(runtime::RouteKind::PAPER,[&](const runtime::ManagerCycleContext& c){
        assert(c.manager_id=="DevManager_001" && c.direction=="LONG");
        assert(c.top_k==5 && c.capital==7000 && c.shared_model_path==cfg.shared_model);
        ++paper;
    });
    router.connectTestingBackend(runtime::RouteKind::BACKTEST,[&](const runtime::ManagerCycleContext&){++backtest;});
    router.connectTestingBackend(runtime::RouteKind::FORWARD,[&](const runtime::ManagerCycleContext&){++forward;});
    assert(router.dispatch("DevManager_001")==runtime::RouteResult::DISPATCHED);
    assert(paper==1 && backtest==0 && forward==0);
    cfg.managers[0].testing_type=config::TestingType::FORWARD;
    assert(router.dispatch("DevManager_001")==runtime::RouteResult::DISPATCHED && forward==1);
    cfg.managers[0].testing_type=config::TestingType::BACKTEST;
    assert(router.dispatch("DevManager_001")==runtime::RouteResult::DISPATCHED && backtest==1);
    cfg.managers[0].mode=config::ExecutionMode::LIVE;
    assert(router.dispatch("DevManager_001")==runtime::RouteResult::LIVE_BLOCKED);
    bool rejected=false;
    try {router.connectTestingBackend(runtime::RouteKind::LIVE,[](const runtime::ManagerCycleContext&){});}
    catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    cfg.managers[0].mode=config::ExecutionMode::TESTING;
    cfg.managers[1].enabled=true;
    int short_calls=0;
    router.connectTestingBackend(runtime::RouteKind::BACKTEST,[&](const runtime::ManagerCycleContext& c){
        if (c.manager_id=="DevManager_002") {
            assert(c.direction=="SHORT"); ++short_calls;
        } else { ++backtest; }
    });
    assert(router.dispatch("DevManager_002")==runtime::RouteResult::DISPATCHED);
    assert(short_calls==1);
    cfg.managers[1].enabled=false;
    cfg.managers[0].enabled=false;
    assert(router.dispatch("DevManager_001")==runtime::RouteResult::MANAGER_INACTIVE);
    assert(paper==1 && backtest==1 && forward==1);
    std::filesystem::remove_all(path);
    std::cout<<"PASS mode-aware router: BACKTEST/FORWARD/PAPER dispatch, independent disable, missing backend, LIVE fail-closed\n";
}
