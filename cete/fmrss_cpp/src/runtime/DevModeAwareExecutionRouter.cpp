#include "devai/runtime/DevModeAwareExecutionRouter.hpp"
#include <stdexcept>
#include <utility>

namespace devai::runtime {
const char* routeResultName(RouteResult result) {
    switch (result) {
        case RouteResult::DISPATCHED: return "DISPATCHED";
        case RouteResult::MANAGER_INACTIVE: return "MANAGER_INACTIVE";
        case RouteResult::BACKEND_NOT_CONNECTED: return "BACKEND_NOT_CONNECTED";
        case RouteResult::LIVE_BLOCKED: return "LIVE_BLOCKED";
    }
    return "UNKNOWN";
}
const char* routeKindName(RouteKind kind) {
    switch (kind) {
        case RouteKind::BACKTEST: return "BACKTEST";
        case RouteKind::FORWARD: return "FORWARD";
        case RouteKind::PAPER: return "PAPER";
        case RouteKind::LIVE: return "LIVE";
    }
    return "UNKNOWN";
}
DevModeAwareExecutionRouter::DevModeAwareExecutionRouter(
    const config::DualManagerActivation& config,
    const DevDualManagerRuntimeBridge& bridge) : config_(config), bridge_(bridge) {}

void DevModeAwareExecutionRouter::connectTestingBackend(RouteKind kind, Cycle callback) {
    if (kind == RouteKind::LIVE) throw std::invalid_argument("LIVE backend registration is prohibited");
    if (!callback) throw std::invalid_argument("Testing backend must be callable");
    testing_backends_[kind] = std::move(callback);
}
RouteKind DevModeAwareExecutionRouter::selectedRoute(const std::string& manager_id) const {
    for (const auto& m : config_.managers) if (m.id == manager_id) {
        if (m.mode == config::ExecutionMode::LIVE) return RouteKind::LIVE;
        switch (m.testing_type) {
            case config::TestingType::BACKTEST: return RouteKind::BACKTEST;
            case config::TestingType::FORWARD: return RouteKind::FORWARD;
            case config::TestingType::PAPER: return RouteKind::PAPER;
        }
        throw std::logic_error("Invalid testing type");
    }
    throw std::invalid_argument("Unknown manager: " + manager_id);
}
RouteResult DevModeAwareExecutionRouter::dispatch(const std::string& manager_id) const {
    const auto status = bridge_.status(manager_id);
    if (!status.agents_scheduled) return RouteResult::MANAGER_INACTIVE;
    const auto route = selectedRoute(manager_id);
    if (route == RouteKind::LIVE) return RouteResult::LIVE_BLOCKED;
    const auto it = testing_backends_.find(route);
    if (it == testing_backends_.end()) return RouteResult::BACKEND_NOT_CONNECTED;
    for (const auto& m : config_.managers) if (m.id == manager_id) {
        // All candidate ranking, RL decisions, risk and simulated fills belong
        // to the supplied existing backend; this layer cannot create an order.
        const ManagerCycleContext ctx{m.id,
            m.direction == config::Direction::LONG ? "LONG" : "SHORT",
            config_.shared_model, m.universe_file, m.top_k, m.capital, m.allocation_percent};
        it->second(ctx); // exceptions propagate; never report success on failure
        return RouteResult::DISPATCHED;
    }
    throw std::logic_error("Manager disappeared during dispatch");
}
} // namespace devai::runtime
