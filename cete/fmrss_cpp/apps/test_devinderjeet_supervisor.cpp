// Isolated central-supervisor unit test. This is NOT a manager execution path.
// Production manager orders must use DevSecureExecutionPath.
#include "cete/supervision/DevinderjeetSupervisor.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace cete::supervision;

int main() {
    DevSupervisorConfig config;
    config.initial_capital = 100000;
    config.global_max_exposure_fraction = .75;
    config.max_open_positions = 2;
    DevinderjeetSupervisor supervisor(config);
    supervisor.registerManager({"DevManager_001", {"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"}, 50000});
    supervisor.registerManager({"DevManager_002", {"DevAgent_006","DevAgent_007","DevAgent_008","DevAgent_009","DevAgent_010"}, 50000});

    // Direct supervisor calls are intentional ONLY in this isolated unit test.
    auto a = supervisor.execute({"order-1","DevManager_001","DevAgent_001","RELIANCE",DevSupervisedAction::BUY,1000,25,1000});
    auto b = supervisor.execute({"order-2","DevManager_002","DevAgent_006","HDFCBANK",DevSupervisedAction::BUY,1000,30,1000});
    assert(a.status == DevOrderStatus::FILLED && b.status == DevOrderStatus::FILLED);
    assert(supervisor.snapshot().open_positions == 2);
    assert(supervisor.snapshot().gross_exposure >= 55000);
    assert(supervisor.audit().size() == 2);

    // Duplicate order IDs cannot execute twice.
    assert(supervisor.execute({"order-1","DevManager_001","DevAgent_001","RELIANCE",DevSupervisedAction::BUY,1000,25,1000}).status == DevOrderStatus::FILLED);
    assert(supervisor.audit().size() == 2);

    assert(supervisor.execute({"bad-agent","DevManager_001","DevAgent_010","ITC",DevSupervisedAction::BUY,1060,1,100}).status == DevOrderStatus::REJECTED);
    assert(supervisor.execute({"third","DevManager_001","DevAgent_002","ITC",DevSupervisedAction::BUY,1060,1,100}).status == DevOrderStatus::REJECTED);
    supervisor.setHalted(true);
    assert(supervisor.execute({"exit-1","DevManager_001","DevAgent_001","RELIANCE",DevSupervisedAction::EXIT,1120,0,1010}).status == DevOrderStatus::FILLED);
    assert(supervisor.snapshot().open_positions == 1);
    assert(supervisor.execute({"halted","DevManager_001","DevAgent_002","ITC",DevSupervisedAction::BUY,1180,1,100}).status == DevOrderStatus::REJECTED);
    assert(supervisor.execute({"exit-2","DevManager_002","DevAgent_006","HDFCBANK",DevSupervisedAction::SELL,1120,0,990}).status == DevOrderStatus::FILLED);
    assert(supervisor.snapshot().open_positions == 0);
    assert(supervisor.audit().size() == 7);

    bool threw = false;
    try { supervisor.registerManager({"DevManager_003", {"a","b","c","d","e"}, 100}); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    std::cout << "PASS DevinderjeetSupervisor unit test: two managers, global risk, exits, halt, idempotency\n";
}
