#include "devai/strategy/dev_ai_strategy/DevSupervisorTerminalSettlement.hpp"
#include <cmath>
#include <set>
#include <stdexcept>

namespace devai::strategy {
DevSupervisorSettlementResult DevSupervisorTerminalSettlement::settle(
    DevinderjeetSupervisor& supervisor,
    const std::vector<std::string>& manager_ids,
    std::int64_t terminal_boundary,
    const std::map<std::string, DevCompletedTerminalMark>& completed_marks) {
    if (terminal_boundary <= 60) throw std::invalid_argument("invalid terminal boundary");
    std::set<std::string> unique_managers;
    struct Pending { std::string manager, agent, symbol; std::int64_t shares; double price; };
    std::vector<Pending> pending;
    for (const auto& id : manager_ids) {
        if (!unique_managers.insert(id).second) throw std::invalid_argument("duplicate manager");
        const auto account = supervisor.managerAccount(id);
        for (const auto& [symbol, position] : account.positions) {
            const auto it = completed_marks.find(symbol);
            if (it == completed_marks.end() ||
                it->second.candle_open_time != terminal_boundary - 60 ||
                !std::isfinite(it->second.close) || it->second.close <= 0)
                throw std::invalid_argument("missing or stale terminal completed candle");
            pending.push_back({id, position.agent_id, symbol, position.shares, it->second.close});
        }
    }
    // Validate ALL held symbols before any settlement is executed.
    DevSupervisorSettlementResult result;
    for (const auto& p : pending) {
        supervisor.updateMark(p.symbol, p.price);
        DevSupervisedOrder order;
        order.order_id = "terminal:" + std::to_string(terminal_boundary) + ":" + p.manager + ":" + p.symbol;
        order.manager_id=p.manager; order.agent_id=p.agent; order.symbol=p.symbol;
        order.action=DevSupervisedAction::EXIT;
        order.timestamp=terminal_boundary; order.shares=p.shares; order.market_price=p.price;
        auto report=supervisor.execute(order);
        if (report.status != DevOrderStatus::FILLED)
            throw std::runtime_error("terminal liquidation rejected: " + report.reason);
        result.reports.push_back(report);
    }
    for (const auto& id : manager_ids)
        if (!supervisor.managerAccount(id).positions.empty())
            throw std::runtime_error("terminal positions remain open");
    result.after=supervisor.snapshot();
    return result;
}
}
