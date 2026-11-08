#include "cete/supervision/DevinderjeetSupervisor.hpp"
#include "devai/strategy/dev_ai_strategy/DevinderjeetSupervisor.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>

int main() {
    using namespace cete::supervision;
    static_assert(std::is_same_v<DevinderjeetSupervisor,
        devai::strategy::DevinderjeetSupervisor>);
    DevSupervisorConfig cfg;
    cfg.initial_capital = 10000.0;
    cfg.global_max_exposure_fraction = 0.75;
    cfg.max_open_positions = 5;
    cfg.transaction_cost_bps = 0.0;
    cfg.slippage_bps = 0.0;
    DevinderjeetSupervisor supervisor(cfg);
    DevManagerRegistration reg;
    reg.manager_id = "DevManager_001";
    reg.agents = {"DevManager_001_Agent_001", "DevManager_001_Agent_002",
                  "DevManager_001_Agent_003", "DevManager_001_Agent_004",
                  "DevManager_001_Agent_005"};
    reg.allocated_capital = 5000.0;
    supervisor.registerManager(reg);
    assert(supervisor.managerAccount("DevManager_001").cash == 5000.0);
    assert(supervisor.snapshot().open_positions == 0);
    supervisor.setHalted(true);
    assert(supervisor.snapshot().halted);
    std::cout << "CETE 5C-2 Core Supervisor Migration PASS\n";
}
