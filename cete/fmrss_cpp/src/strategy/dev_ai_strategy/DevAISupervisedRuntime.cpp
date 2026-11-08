#include "devai/strategy/dev_ai_strategy/DevAISupervisedRuntime.hpp"
#include <algorithm>
#include <stdexcept>

namespace devai::strategy {
DevAISupervisedRuntime::DevAISupervisedRuntime(
    DevAISupervisor& administrator, devai::DevManagerRegistry& registry,
    DevinderjeetSupervisor& central, DevSharedModelMultiManagerRuntime& runtime)
    : administrator_(administrator), registry_(registry), central_(central), runtime_(runtime) {}

void DevAISupervisedRuntime::requireEligible(
    const std::string& manager_id, const std::vector<std::string>& symbols) const {
    if (manager_id.empty()) throw std::invalid_argument("manager ID required");
    const auto m = registry_.load_manager(manager_id);
    if (!m.active) throw std::runtime_error("inactive DevAI manager: " + manager_id);
    // Revalidate CETE registration; this also fails closed after a restart
    // when only the persistent registry was restored.
    const auto account = central_.managerAccount(manager_id);
    if (account.registration.manager_id != manager_id ||
        account.registration.agents.size() != DEV_AGENT_SLOT_COUNT)
        throw std::runtime_error("invalid CETE manager registration");
    if (central_.snapshot().halted)
        throw std::runtime_error("CETE supervisor halted");
    for (const auto& symbol : symbols) {
        if (symbol.empty() ||
            std::find(m.symbols.begin(), m.symbols.end(), symbol) == m.symbols.end())
            throw std::runtime_error("symbol not assigned to DevAI manager: " + symbol);
    }
}

void DevAISupervisedRuntime::attachManager(
    const std::string& manager_id,
    const std::array<std::string, DEV_AGENT_SLOT_COUNT>& agents) {
    requireEligible(manager_id, {});
    const auto account = central_.managerAccount(manager_id);
    if (account.registration.agents != std::vector<std::string>(agents.begin(), agents.end()))
        throw std::invalid_argument("runtime roster differs from CETE registration");
    runtime_.addManager(manager_id, agents);
}

DevSharedManagerCycle DevAISupervisedRuntime::evaluate(
    const std::string& manager_id, std::int64_t decision_time,
    const std::vector<DevSharedCandidate>& candidates) const {
    std::vector<std::string> symbols;
    symbols.reserve(candidates.size());
    for (const auto& candidate : candidates) symbols.push_back(candidate.symbol);
    requireEligible(manager_id, symbols);
    return runtime_.evaluate(manager_id, decision_time, candidates);
}

DevOrderReport DevAISupervisedRuntime::submit(
    const DevSharedManagerCycle& cycle, DevSupervisedAction action,
    const std::string& order_id, std::int64_t shares, double execution_price) {
    // Recheck the CURRENT registry and central state, not only the state
    // at evaluation time. An operator may have deactivated the manager.
    requireEligible(cycle.manager_id, {});
    if (action == DevSupervisedAction::BUY) {
        // The selected symbol must still belong to this manager.
        if (cycle.decision.selected_symbol.empty())
            throw std::invalid_argument("BUY requires a selected symbol");
        requireEligible(cycle.manager_id, {cycle.decision.selected_symbol});
    }
    // EXIT/SELL remains permitted for previously owned symbols even if the
    // assigned universe changed; central ownership/risk checks still apply.
    return runtime_.submit(cycle, action, order_id, shares, execution_price);
}
} // namespace devai::strategy
