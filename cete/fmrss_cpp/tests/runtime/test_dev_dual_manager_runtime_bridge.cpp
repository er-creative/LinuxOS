#include "devai/runtime/DevDualManagerRuntimeBridge.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace devai;
int main(int argc,char**argv) {
    if(argc!=2) throw std::runtime_error("Usage: test_dev_dual_manager_runtime_bridge config/devmanagers.ini");
    const auto cfg=config::loadDualManagerActivation(argv[1]);
    const auto path=std::filesystem::temp_directory_path()/"cete_dual_manager_bridge_test";
    std::filesystem::remove_all(path);
    DevManagerRegistry registry(path);
    const std::vector<std::string> stocks={"RELIANCE","SBIN","ITC","NTPC","HDFCBANK","AXISBANK","ICICIBANK","BHARTIARTL","HCLTECH","ADANIPORTS"};
    auto one=registry.create_manager(stocks);
    auto two=registry.create_manager(stocks);
    assert(one.manager_id=="DevManager_001" && two.manager_id=="DevManager_002");
    int technical=0,portfolio=0, executed=0;
    runtime::DevDualManagerRuntimeBridge bridge(cfg,registry,
        [&](const runtime::ManagerRuntimeStatus&){++technical;},
        [&](const runtime::ManagerRuntimeStatus&){++portfolio;});
    assert(bridge.mayScheduleStrategy("DevManager_001"));
    assert(!bridge.mayScheduleStrategy("DevManager_002"));
    assert(bridge.scheduleIfActive("DevManager_001",[&]{++executed;}));
    assert(!bridge.scheduleIfActive("DevManager_002",[&]{++executed;}));
    assert(executed==1);
    auto statuses=bridge.reportAll();
    assert(statuses.size()==2 && technical==2 && portfolio==2);
    assert(!statuses[0].new_entry_authorized && !statuses[1].new_entry_authorized);
    assert(statuses[0].mode=="TESTING" && statuses[0].testing_type=="PAPER");
    assert(statuses[1].mode=="TESTING" && statuses[1].testing_type=="BACKTEST");
    assert(!statuses[0].real_broker_order_route && !statuses[1].real_broker_order_route);
    assert(!statuses[0].simulated_order_route && !statuses[1].simulated_order_route);
    registry.set_active("DevManager_001",false);
    assert(!bridge.mayScheduleStrategy("DevManager_001"));
    std::filesystem::remove_all(path);
    std::cout<<"PASS CETE dual-manager runtime bridge: registry gating, scheduling, dual reporting, fail-closed orders\n";
}
