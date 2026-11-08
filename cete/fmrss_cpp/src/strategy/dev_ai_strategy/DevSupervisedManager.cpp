#include "devai/strategy/dev_ai_strategy/DevSupervisedManager.hpp"
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevSupervisedManager::DevSupervisedManager(
    std::string id, const devai::execution::DevSecureExecutionPath& execution)
    : id_(std::move(id)), execution_(execution) {
    if (id_.empty()) throw std::invalid_argument("empty manager ID");
}
DevOrderReport DevSupervisedManager::trade(
    const std::string& order_id, const std::string& agent_id,
    const std::string& symbol, DevSupervisedAction action,
    std::int64_t timestamp, std::int64_t shares, double price) {
    // Deliberately do not call eligible() here. submit() checks the gates
    // immediately before the central supervisor is invoked.
    DevSupervisedOrder order{order_id, id_, agent_id, symbol,
                             action, timestamp, shares, price};
    auto report = execution_.submit(order);
    reports_.push_back(report);
    return report;
}
} // namespace devai::strategy
