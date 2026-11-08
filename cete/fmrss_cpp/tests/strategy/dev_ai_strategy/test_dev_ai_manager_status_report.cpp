#include "devai/config/DevAIConfigurationBootstrap.hpp"
#include "devai/report/DevAIManagerStatusReport.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <sstream>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "cete_5c3c_manager_status_report_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root);
        devai::strategy::DevSupervisorConfig core_cfg;
        core_cfg.initial_capital = 100000.0;
        devai::strategy::DevinderjeetSupervisor core(core_cfg);
        devai::strategy::DevAISupervisor supervisor(
            registry, core, [](const std::string&, const std::string&) { return true; });
        devai::config::DevAIConfigurationBootstrap bootstrap(
            supervisor, registry,
            [](const std::string&, const std::vector<std::string>&, const std::string&) {
                return true; // Test stub only, not actual RL training/approval.
            });
        devai::config::DevAIRuntimeConfig cfg;
        std::vector<std::string> shares{"RELIANCE", "TCS", "INFY"};
        bootstrap.apply(cfg, shares, 100000.0);
        auto s = devai::report::DevAIManagerStatusReport::collect(cfg, registry, supervisor);
        assert(s.configured_managers == 2 && s.registered_managers == 1);
        assert(s.active_managers == 1 && s.active_agents == 5);
        assert(s.active_symbol_assignments == 3);
        assert(s.managers[0].agent_count == 5 && s.managers[0].symbols.size() == 3);
        assert(!s.managers[1].registered && !s.managers[1].active);
        std::ostringstream output;
        devai::report::DevAIManagerStatusReport::print(s, output);
        assert(output.str().find("DevManager_002 | NOT CREATED") != std::string::npos);
        supervisor.setActive("DevManager_001", false);
        s = devai::report::DevAIManagerStatusReport::collect(cfg, registry, supervisor);
        assert(s.active_managers == 0 && s.active_agents == 0);
        fs::remove_all(root);
        std::cout << "CETE 5C-3C manager status report PASS\n";
    } catch (...) { fs::remove_all(root); throw; }
}
