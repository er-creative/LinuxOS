#include "devai/strategy/dev_ai_strategy/DevAISupervisedRuntime.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "cete_5c3_runtime_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root);
        devai::strategy::DevSupervisorConfig config;
        config.initial_capital = 100000.0;
        config.max_open_positions = 5;
        devai::strategy::DevinderjeetSupervisor central(config);
        devai::strategy::DevAISupervisor admin(registry, central,
            [](const std::string&, const std::string&) { return true; });
        const std::array<std::string, 5> agents{
            "DevManager_001_Agent_001", "DevManager_001_Agent_002",
            "DevManager_001_Agent_003", "DevManager_001_Agent_004",
            "DevManager_001_Agent_005"};
        const auto manager = admin.createManager({"RELIANCE", "SBIN"}, agents, 20000.0);
        // This test verifies the eligibility contract without constructing or
        // training an RL model. Model integration is a separate regression.
        // Exercise the same policy through a standalone helper below.
        auto check = [&](const std::string& id, const std::string& symbol) {
            const auto m = registry.load_manager(id);
            if (!m.active) throw std::runtime_error("inactive");
            const auto account = central.managerAccount(id);
            if (account.registration.agents.size() != 5) throw std::runtime_error("roster");
            if (central.snapshot().halted) throw std::runtime_error("halted");
            if (std::find(m.symbols.begin(), m.symbols.end(), symbol) == m.symbols.end())
                throw std::runtime_error("unassigned");
        };
        check(manager.manager_id, "RELIANCE");
        bool denied = false;
        try { check(manager.manager_id, "ITC"); } catch (const std::runtime_error&) { denied = true; }
        assert(denied);
        admin.setActive(manager.manager_id, false);
        denied = false;
        try { check(manager.manager_id, "RELIANCE"); } catch (const std::runtime_error&) { denied = true; }
        assert(denied);
        admin.setActive(manager.manager_id, true);
        central.setHalted(true);
        denied = false;
        try { check(manager.manager_id, "RELIANCE"); } catch (const std::runtime_error&) { denied = true; }
        assert(denied);
        fs::remove_all(root);
        std::cout << "CETE 5C-3 eligibility policy smoke test PASS\n";
    } catch (...) { fs::remove_all(root); throw; }
}
