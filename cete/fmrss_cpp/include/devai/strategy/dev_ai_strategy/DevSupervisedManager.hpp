#pragma once
#include "devai/strategy/dev_ai_strategy/DevinderjeetSupervisor.hpp"
#include "devai/execution/DevSecureExecutionPath.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy {
// Manager-originated orders must use the secure gateway, never the central
// supervisor directly. The shared RL model remains outside this adapter.
class DevSupervisedManager {
public:
    DevSupervisedManager(std::string manager_id,
                         const devai::execution::DevSecureExecutionPath& execution);
    [[nodiscard]] DevOrderReport trade(const std::string& order_id,
        const std::string& agent_id, const std::string& symbol,
        DevSupervisedAction action, std::int64_t timestamp,
        std::int64_t shares, double price);
    [[nodiscard]] const std::vector<DevOrderReport>& reports() const noexcept { return reports_; }
    [[nodiscard]] const std::string& id() const noexcept { return id_; }
private:
    std::string id_;
    const devai::execution::DevSecureExecutionPath& execution_;
    std::vector<DevOrderReport> reports_;
};
} // namespace devai::strategy
