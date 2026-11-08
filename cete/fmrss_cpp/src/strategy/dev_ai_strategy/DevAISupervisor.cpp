#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevAISupervisor::DevAISupervisor(devai::DevManagerRegistry& registry,
                                 DevinderjeetSupervisor& central,
                                 Authorize authorization)
    : registry_(registry), central_(central), authorization_(std::move(authorization)) {
    if (!authorization_) throw std::invalid_argument("authorization callback required");
}
void DevAISupervisor::requireAuthorized(const std::string& operation,
                                        const std::string& manager_id) const {
    if (!authorization_(operation, manager_id))
        throw std::runtime_error("CETE authorization denied: " + operation);
}
devai::ManagedUniverse DevAISupervisor::createManager(
    const std::vector<std::string>& symbols,
    const std::array<std::string, 5>& agent_ids,
    double allocated_capital) {
    requireAuthorized("create_manager", "");
    if (!std::isfinite(allocated_capital) || allocated_capital <= 0.0)
        throw std::invalid_argument("invalid capital allocation");
    const auto checked = devai::DevManagerRegistry::validate_symbols(symbols);
    // Registry chooses the next persistent manager ID. No execution is possible
    // until CETE has accepted the corresponding manager registration.
    auto manager = registry_.create_manager(checked);
    try {
        central_.registerManager(DevManagerRegistration{
            manager.manager_id,
            std::vector<std::string>(agent_ids.begin(), agent_ids.end()),
            allocated_capital});
    } catch (...) {
        // Fail closed. Keep ID for diagnosis and avoid accidental ID reuse.
        registry_.set_active(manager.manager_id, false);
        throw;
    }
    return manager;
}
void DevAISupervisor::assignShares(const std::string& id,
                                   const std::vector<std::string>& symbols) {
    requireAuthorized("assign_shares", id);
    (void)central_.managerAccount(id); // must be centrally registered
    registry_.assign_shares(id, symbols);
}
void DevAISupervisor::setActive(const std::string& id, bool active) {
    requireAuthorized("set_active", id);
    (void)central_.managerAccount(id);
    // Registry active state is an administrative flag. Callers must check it
    // before runtime evaluation/submit; it does not halt CETE execution itself.
    registry_.set_active(id, active);
}
std::vector<devai::ManagedUniverse> DevAISupervisor::managers() const {
    requireAuthorized("list_managers", "");
    return registry_.list_managers();
}
DevManagerAccount DevAISupervisor::account(const std::string& id) const {
    requireAuthorized("view_account", id);
    return central_.managerAccount(id);
}
DevSupervisorSnapshot DevAISupervisor::consolidatedSnapshot() const {
    requireAuthorized("view_consolidated_snapshot", "");
    // CETE-wide snapshot, NOT a DevAI-only strategy report.
    return central_.snapshot();
}
} // namespace devai::strategy
