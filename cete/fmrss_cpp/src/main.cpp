#include "devai/config/DevAIRuntimeConfig.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

int main(int argc, char** argv) {
    try {
        std::cout << "========================================\n"
                  << "CETE - Creative Engineer Trading Environment\n"
                  << "========================================\n"
                  << "1-Minute  Timeframe : ENABLED\n"
                  << "5-Minute  Timeframe : ENABLED\n"
                  << "15-Minute Timeframe : ENABLED\n";

        // Pass the EXISTING manager-registry root as the first argument.
        // This executable is read-only: it does not create managers, train
        // models, approve checkpoints, or grant trading authorization.
        if (argc != 2) {
            std::cerr << "\nUsage: " << argv[0]
                      << " <existing-manager-registry-directory>\n"
                      << "No registry was opened or modified.\n";
            return 2;
        }
        const std::filesystem::path registry_root(argv[1]);
        if (!std::filesystem::is_directory(registry_root)) {
            throw std::runtime_error("Registry directory not found: " + registry_root.string());
        }

        const devai::config::DevAIRuntimeConfig config;
        const devai::DevManagerRegistry registry(registry_root);
        const auto registered = registry.list_managers();

        std::unordered_map<std::string, devai::ManagedUniverse> by_id;
        for (const auto& manager : registered) {
            if (!by_id.emplace(manager.manager_id, manager).second)
                throw std::runtime_error("Duplicate registry manager ID: " + manager.manager_id);
        }

        std::size_t active_count = 0;
        std::size_t active_share_assignments = 0;
        std::size_t configured_agent_count_for_active_managers = 0;
        for (const auto& manager : registered) {
            if (!manager.active) continue;
            ++active_count;
            active_share_assignments += manager.symbols.size();
            for (const auto& setting : config.managers) {
                if (setting.manager_id == manager.manager_id) {
                    configured_agent_count_for_active_managers += setting.agents_per_manager;
                    break;
                }
            }
        }

        std::cout << "\n========== DEVAI MANAGER REGISTRY REPORT ==========\n"
                  << "Configured managers         : " << config.managers.size() << '\n'
                  << "Registered managers         : " << registered.size() << '\n'
                  << "Registry-marked active      : " << active_count << '\n'
                  << "Configured agents (active)  : " << configured_agent_count_for_active_managers << '\n'
                  << "Active share assignments    : " << active_share_assignments << '\n';

        for (const auto& setting : config.managers) {
            const auto it = by_id.find(setting.manager_id);
            std::cout << "\n" << setting.manager_id << " | ";
            if (it == by_id.end()) {
                std::cout << "NOT REGISTERED\n"
                          << "  Configured enabled : " << (setting.enabled ? "YES" : "NO") << '\n'
                          << "  Actual shares      : 0\n";
                continue;
            }
            const auto& manager = it->second;
            std::cout << (manager.active ? "REGISTRY ACTIVE" : "REGISTRY INACTIVE") << '\n'
                      << "  Configured enabled : " << (setting.enabled ? "YES" : "NO") << '\n'
                      << "  Configured agents  : " << setting.agents_per_manager << '\n'
                      << "  Assigned shares    : " << manager.symbols.size() << '\n'
                      << "  Model directory    : " << setting.model_directory << '\n'
                      << "  Symbols            : ";
            for (std::size_t i = 0; i < manager.symbols.size(); ++i)
                std::cout << (i ? ", " : "") << manager.symbols[i];
            std::cout << '\n';
        }
        for (const auto& manager : registered) {
            bool configured = false;
            for (const auto& setting : config.managers)
                if (setting.manager_id == manager.manager_id) configured = true;
            if (!configured)
                std::cout << "\nWARNING: unconfigured registered manager: " << manager.manager_id << '\n';
        }
        std::cout << "===================================================\n"
                  << "REPORT ONLY: Registry active is NOT proof of central\n"
                  << "supervisor authorization or an approved trained model.\n"
                  << "No trading was started.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "CETE startup report error: " << e.what() << '\n';
        return 1;
    }
}
