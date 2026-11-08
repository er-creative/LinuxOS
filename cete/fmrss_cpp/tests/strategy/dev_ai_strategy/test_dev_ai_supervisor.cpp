#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "cete_5c_supervisor_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root);
        devai::strategy::DevSupervisorConfig config;
        config.initial_capital = 100000.0;
        devai::strategy::DevinderjeetSupervisor central(config);
        bool permit = false;
        devai::strategy::DevAISupervisor supervisor(registry, central,
            [&](const std::string&, const std::string&) { return permit; });
        const std::array<std::string, 5> agents{
            "DevManager_001_Agent_001", "DevManager_001_Agent_002",
            "DevManager_001_Agent_003", "DevManager_001_Agent_004",
            "DevManager_001_Agent_005"};
        bool denied = false;
        try { (void)supervisor.createManager({"RELIANCE"}, agents, 20000.0); }
        catch (const std::runtime_error&) { denied = true; }
        assert(denied && registry.list_managers().empty());
        permit = true;
        const auto manager = supervisor.createManager({"RELIANCE", "SBIN"}, agents, 20000.0);
        assert(manager.manager_id == "DevManager_001");
        assert(manager.active && manager.symbols.size() == 2);
        assert(supervisor.account(manager.manager_id).cash == 20000.0);
        supervisor.assignShares(manager.manager_id, {"NTPC", "HDFCBANK", "ICICIBANK"});
        assert(registry.load_manager(manager.manager_id).symbols.size() == 3);
        supervisor.setActive(manager.manager_id, false);
        assert(!registry.load_manager(manager.manager_id).active);
        // Central capital rejection leaves a reserved, inactive manager ID.
        const std::array<std::string, 5> agents2{"B1", "B2", "B3", "B4", "B5"};
        bool rejected = false;
        try { (void)supervisor.createManager({"ITC"}, agents2, 90000.0); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        assert(!registry.load_manager("DevManager_002").active);
        assert(central.audit().empty()); // administration never places trades
        fs::remove_all(root);
        std::cout << "DevAISupervisor 5C foundation PASS\n";
    } catch (...) { fs::remove_all(root); throw; }
}
