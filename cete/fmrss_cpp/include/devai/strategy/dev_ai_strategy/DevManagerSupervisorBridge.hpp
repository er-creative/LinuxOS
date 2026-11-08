#pragma once
#include "devai/strategy/dev_ai_strategy/DevManager.hpp"
#include "devai/strategy/dev_ai_strategy/DevSupervisedManager.hpp"
#include <array>
#include <string>
#include <vector>

namespace devai::strategy {
// Adapter: preserves frozen five-slot DevManager/DevAgent contracts while
// mapping manager-local slots to globally unique agent identities.
// No market prices, orders, or model predictions are fabricated here.
class DevManagerSupervisorBridge {
public:
    DevManagerSupervisorBridge(std::string manager_id,
        std::array<std::string, DEV_AGENT_SLOT_COUNT> global_agents,
        DevinderjeetSupervisor& supervisor,
        const devai::execution::DevSecureExecutionPath& execution);

    // Reports carry global agent IDs. This function translates only their
    // identities to local slot IDs before existing manager validation.
    [[nodiscard]] DevManagerState buildState(
        std::int64_t time, const std::vector<DevAgentReport>& global_reports) const;

    // Explicit manager trade intent; SELECT_AGENT + BUY requires LONG signal.
    // EXIT/SELL requires an existing owned position. NO_TRADE/KEEP_CURRENT
    // can only HOLD unless caller explicitly requests an EXIT/SELL.
    [[nodiscard]] DevOrderReport submit(const DevManagerState& state,
        const DevManagerDecision& decision, DevSupervisedAction action,
        const std::string& order_id, std::int64_t shares, double execution_candle_open);

    [[nodiscard]] const std::string& managerId() const noexcept {return manager_id_;}
    [[nodiscard]] const std::array<std::string, DEV_AGENT_SLOT_COUNT>& agentIds() const noexcept {return global_agents_;}
    [[nodiscard]] const std::vector<DevOrderReport>& reports() const noexcept {return executor_.reports();}
private:
    std::string manager_id_;
    std::array<std::string, DEV_AGENT_SLOT_COUNT> global_agents_;
    DevinderjeetSupervisor& supervisor_;
    DevSupervisedManager executor_;
    DevManager manager_;
};
} // namespace devai::strategy
