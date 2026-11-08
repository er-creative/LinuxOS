#include "devai/runtime/DevDualManagerRuntimeBridge.hpp"
#include <stdexcept>
#include <utility>
namespace devai::runtime {
DevDualManagerRuntimeBridge::DevDualManagerRuntimeBridge(
    const config::DualManagerActivation& config, const DevManagerRegistry& registry,
    Reporter technical, Reporter portfolio)
    : config_(config), registry_(registry), technical_(std::move(technical)), portfolio_(std::move(portfolio)) {
    if (!technical_ || !portfolio_) throw std::invalid_argument("Both supervisor reporters required");
}
ManagerRuntimeStatus DevDualManagerRuntimeBridge::status(const std::string& id) const {
    for (const auto& m: config_.managers) if (m.id == id) {
        ManagerRuntimeStatus out;
        out.manager_id=m.id; out.direction=m.direction==config::Direction::LONG?"LONG":"SHORT";
        out.mode=config::modeName(m.mode);
        out.testing_type=m.mode==config::ExecutionMode::TESTING ? config::testingTypeName(m.testing_type) : "N/A";
        // Fail closed: this bridge NEVER authorizes broker orders.
        out.real_broker_order_route=false;
        out.simulated_order_route=false;
        out.configured_enabled=m.enabled; out.allocated_capital=m.capital;
        out.agents.assign(m.agents.begin(),m.agents.end());
        if (!m.enabled) {out.reason="DISABLED_BY_CONFIG";return out;}
        try {
            auto registered=registry_.load_manager(id);
            out.registry_active=registered.active;
            if (!registered.active) {out.reason="INACTIVE_IN_REGISTRY";return out;}
            if (registered.symbols.size()<m.top_k) {out.reason="UNIVERSE_BELOW_TOP_K";return out;}
        } catch (const std::exception&) {out.reason="MANAGER_NOT_REGISTERED";return out;}
        out.agents_scheduled=true;
        // The current CETE supervisor supports LONG inventory only; it also has
        // a global one-position limit. Do not infer order permission from this flag.
        out.new_entry_authorized=false;
        out.reason="STRATEGY_SCHEDULING_ONLY_EXECUTION_BLOCKED";
        return out;
    }
    throw std::invalid_argument("Unknown manager ID: "+id);
}
std::vector<ManagerRuntimeStatus> DevDualManagerRuntimeBridge::reportAll() const {
    std::vector<ManagerRuntimeStatus> statuses;
    for (const auto& m:config_.managers) {
        auto s=status(m.id);
        technical_(s);
        portfolio_(s);
        statuses.push_back(std::move(s));
    }
    return statuses;
}
bool DevDualManagerRuntimeBridge::mayScheduleStrategy(const std::string& id) const {
    return status(id).agents_scheduled;
}
} // namespace devai::runtime
