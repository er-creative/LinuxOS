#include "devai/report/DevAIManagerStatusReport.hpp"
#include <iomanip>
#include <stdexcept>
#include <unordered_map>

namespace devai::report {
ManagerStatusSnapshot DevAIManagerStatusReport::collect(
    const devai::config::DevAIRuntimeConfig& config,
    const devai::DevManagerRegistry& registry,
    const devai::strategy::DevAISupervisor& supervisor) {
    ManagerStatusSnapshot result;
    result.configured_managers = config.managers.size();
    const auto registered = registry.list_managers();
    result.registered_managers = registered.size();
    std::unordered_map<std::string, devai::ManagedUniverse> by_id;
    for (const auto& m : registered) by_id.emplace(m.manager_id, m);
    for (const auto& cfg : config.managers) {
        ManagerStatus status;
        status.manager_id = cfg.manager_id;
        status.model_directory = cfg.model_directory;
        const auto it = by_id.find(cfg.manager_id);
        if (it != by_id.end()) {
            status.registered = true;
            status.symbols = it->second.symbols;
            // Registry state is actual persisted state, not a proposed config value.
            status.active = it->second.active;
            // Supervisor registration is the authoritative source for agent count.
            const auto account = supervisor.account(cfg.manager_id);
            status.agent_count = account.registration.agents.size();
            if (status.active) {
                ++result.active_managers;
                result.active_agents += status.agent_count;
                result.active_symbol_assignments += status.symbols.size();
            }
        }
        result.managers.push_back(std::move(status));
    }
    // Fail rather than silently omit managers registered outside configuration.
    for (const auto& m : registered)
        if (!config.managers.empty() &&
            by_id.count(m.manager_id) &&
            [&] {
                for (const auto& c : config.managers)
                    if (c.manager_id == m.manager_id) return false;
                return true;
            }())
            throw std::runtime_error("registered manager absent from configuration: " + m.manager_id);
    return result;
}
void DevAIManagerStatusReport::print(const ManagerStatusSnapshot& s, std::ostream& out) {
    out << "\n========== CETE / DevAI MANAGER STATUS ==========\n"
        << "Configured managers : " << s.configured_managers << '\n'
        << "Registered managers : " << s.registered_managers << '\n'
        << "Active managers     : " << s.active_managers << '\n'
        << "Active agents       : " << s.active_agents << '\n'
        << "Active assignments  : " << s.active_symbol_assignments << '\n';
    for (const auto& m : s.managers) {
        out << "\n" << m.manager_id << " | "
            << (m.registered ? (m.active ? "ACTIVE" : "INACTIVE") : "NOT CREATED")
            << "\n  Agents assigned : " << m.agent_count
            << "\n  Shares assigned : " << m.symbols.size()
            << "\n  Model directory : " << m.model_directory << '\n';
        if (!m.symbols.empty()) {
            out << "  Symbols         : ";
            for (std::size_t i=0; i<m.symbols.size(); ++i)
                out << (i ? ", " : "") << m.symbols[i];
            out << '\n';
        }
    }
    out << "=================================================\n";
}
} // namespace devai::report
