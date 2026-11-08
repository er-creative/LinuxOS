#include "devai/config/DevAIConfigurationBootstrap.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "cete_5c3c_rectified_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root);
        devai::strategy::DevSupervisorConfig core_config;
        core_config.initial_capital = 100000.0;
        devai::strategy::DevinderjeetSupervisor core(core_config);
        devai::strategy::DevAISupervisor supervisor(registry, core,
            [](const std::string&, const std::string&) { return true; }); // TEST ONLY
        int callbacks = 0;
        bool approved = true;
        std::string last_manager, last_directory;
        std::size_t last_universe = 0;
        devai::config::DevAIConfigurationBootstrap bootstrap(supervisor, registry,
            [&](const std::string& manager, const std::vector<std::string>& symbols,
                const std::string& directory) {
                ++callbacks;
                last_manager = manager;
                last_directory = directory;
                last_universe = symbols.size();
                return approved; // TEST STUB, NOT REAL TRAINING
            });
        std::vector<std::string> shares{
            "RELIANCE", "HDFCBANK", "ICICIBANK", "SBIN", "BHARTIARTL",
            "AXISBANK", "ITC", "INFY", "TCS", "LT"};
        devai::config::DevAIRuntimeConfig config;
        assert(config.managers[0].enabled && !config.managers[1].enabled);
        bootstrap.apply(config, shares, 100000.0);
        assert(registry.list_managers().size() == 1);
        assert(registry.load_manager("DevManager_001").active);
        assert(last_manager == "DevManager_001" && last_universe == 10);
        assert(last_directory == "models/DevManager_001");
        assert(core.managerAccount("DevManager_001").registration.agents.size() == 5);
        bootstrap.apply(config, shares, 100000.0);
        assert(callbacks == 2); // re-verifies model on restart

        // Universe can expand without a configured hard limit.
        shares.push_back("WIPRO");
        bootstrap.apply(config, shares, 100000.0);
        assert(last_universe == 11);
        assert(registry.load_manager("DevManager_001").symbols.size() == 11);

        // A failed training/approval attempt must leave the manager inactive.
        shares.push_back("HCLTECH");
        approved = false;
        bool failed = false;
        try { bootstrap.apply(config, shares, 100000.0); }
        catch (const std::runtime_error&) { failed = true; }
        assert(failed && !registry.load_manager("DevManager_001").active);
        approved = true;
        bootstrap.apply(config, shares, 100000.0);
        assert(registry.load_manager("DevManager_001").active);
        assert(last_universe == 12);

        // A second manager is separately configured and has its own model path.
        // Existing capital cannot be changed by this bootstrap; create fresh state.
        const auto second_root = root / "second";
        devai::DevManagerRegistry registry2(second_root);
        devai::strategy::DevinderjeetSupervisor core2(core_config);
        devai::strategy::DevAISupervisor supervisor2(registry2, core2,
            [](const std::string&, const std::string&) { return true; }); // TEST ONLY
        std::vector<std::string> observed_directories;
        devai::config::DevAIConfigurationBootstrap bootstrap2(supervisor2, registry2,
            [&](const std::string&, const std::vector<std::string>&,
                const std::string& directory) {
                observed_directories.push_back(directory);
                return true; // TEST STUB
            });
        auto both = config;
        both.managers[0].capital_allocation_fraction = 0.6;
        both.managers[1].enabled = true;
        both.managers[1].capital_allocation_fraction = 0.4;
        both.managers[1].assigned_symbols = {"TCS", "INFY"};
        bootstrap2.apply(both, shares, 100000.0);
        assert(registry2.list_managers().size() == 2);
        assert(registry2.load_manager("DevManager_002").symbols.size() == 2);
        assert(observed_directories.size() == 2);
        assert(observed_directories[0] != observed_directories[1]);

        auto invalid = both;
        invalid.managers[1].model_directory = invalid.managers[0].model_directory;
        bool rejected = false;
        try { bootstrap2.apply(invalid, shares, 100000.0); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        fs::remove_all(root);
        std::cout << "CETE 5C-3C manager-private model configuration PASS\n";
    } catch (...) { fs::remove_all(root); throw; }
}
