#pragma once
#include "devai/runtime/DevDualManagerRuntimeBridge.hpp"
#include <functional>
#include <map>
#include <string>

namespace devai::runtime {
// A decision cycle is provided by the EXISTING manager/agents. No stock selection here.
struct ManagerCycleContext {
    std::string manager_id;
    std::string direction;
    std::string shared_model_path;
    std::string universe_file;
    std::size_t top_k = 5;
    double capital = 0.0;
    double allocation_percent = 0.0;
};
enum class RouteKind { BACKTEST, FORWARD, PAPER, LIVE };
enum class RouteResult { DISPATCHED, MANAGER_INACTIVE, BACKEND_NOT_CONNECTED, LIVE_BLOCKED };
const char* routeResultName(RouteResult result);
const char* routeKindName(RouteKind kind);

// Callbacks are strategy-cycle adapters, NOT broker order endpoints.
// Live callback deliberately unavailable in this foundation.
class DevModeAwareExecutionRouter {
public:
    using Cycle = std::function<void(const ManagerCycleContext&)>;
    DevModeAwareExecutionRouter(const config::DualManagerActivation& config,
                                const DevDualManagerRuntimeBridge& bridge);
    void connectTestingBackend(RouteKind kind, Cycle callback);
    RouteResult dispatch(const std::string& manager_id) const;
    RouteKind selectedRoute(const std::string& manager_id) const;
private:
    const config::DualManagerActivation& config_;
    const DevDualManagerRuntimeBridge& bridge_;
    std::map<RouteKind, Cycle> testing_backends_;
};
} // namespace devai::runtime
