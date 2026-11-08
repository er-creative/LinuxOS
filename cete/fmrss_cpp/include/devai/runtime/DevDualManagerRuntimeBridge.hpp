#pragma once
#include "devai/config/DevDualManagerActivation.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <functional>
#include <string>
#include <vector>
namespace devai::runtime {
struct ManagerRuntimeStatus {
    std::string manager_id;
    std::string direction;
    std::string mode;
    std::string testing_type;
    bool simulated_order_route = false;
    bool real_broker_order_route = false;
    bool configured_enabled=false;
    bool registry_active=false;
    bool agents_scheduled=false;
    bool new_entry_authorized=false;
    double allocated_capital=0;
    std::vector<std::string> agents;
    std::string reason;
};
// Dispatch is only a scheduler/reporting interface, NOT an order authorization.
// The risk/exit scheduler MUST run independently even when manager disabled.
class DevDualManagerRuntimeBridge {
public:
    using Reporter = std::function<void(const ManagerRuntimeStatus&)>;
    DevDualManagerRuntimeBridge(const config::DualManagerActivation& config,
                                const DevManagerRegistry& registry,
                                Reporter devai_supervisor_report,
                                Reporter devinderjeet_supervisor_report);
    ManagerRuntimeStatus status(const std::string& manager_id) const;
    std::vector<ManagerRuntimeStatus> reportAll() const;
    bool mayScheduleStrategy(const std::string& manager_id) const;
    template<class Fn> bool scheduleIfActive(const std::string& manager_id, Fn&& run) const {
        if (!mayScheduleStrategy(manager_id)) return false;
        run();
        return true;
    }
private:
    const config::DualManagerActivation& config_;
    const DevManagerRegistry& registry_;
    Reporter technical_;
    Reporter portfolio_;
};
} // namespace devai::runtime
