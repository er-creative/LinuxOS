#pragma once

#include "devai/strategy/dev_ai_strategy/DevinderjeetSupervisor.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace devai::execution {
// Local decision result only. Never submit this object to an order gateway.
// A HELD result does not imply that the manager is approved for trading.
[[nodiscard]] inline cete::supervision::DevOrderReport makeHoldDecision(
    const std::string& order_id,
    const std::string& manager_id,
    std::int64_t timestamp) {
    if (order_id.empty() || manager_id.empty() || timestamp <= 0)
        throw std::invalid_argument("HOLD requires order ID, manager ID and positive time");
    cete::supervision::DevOrderReport report{};
    report.order_id = order_id;
    report.manager_id = manager_id;
    report.action = cete::supervision::DevSupervisedAction::HOLD;
    report.status = cete::supervision::DevOrderStatus::HELD;
    report.timestamp = timestamp;
    report.shares = 0;
    report.execution_price = 0.0;
    report.fee = 0.0;
    report.realized_pnl = 0.0;
    report.reason = "HOLD: no executable order";
    return report;
}
} // namespace devai::execution
