#include "devai/strategy/dev_ai_strategy/reward/DevHistoricalReplayIntegration_D9F4C.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace devai::strategy::d9f4c {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
bool near(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a - b) <= 1e-7 * (1.0 + std::max(std::abs(a), std::abs(b)));
}
}
HistoricalReplayAudit DevHistoricalReplayIntegration::validate(
    const ReplayResult& replay, double initial_capital) {
    require(std::isfinite(initial_capital) && initial_capital > 0.0,
            "D9F4C historical audit: invalid initial capital");
    require(!replay.steps.empty(), "D9F4C historical audit: empty replay");
    HistoricalReplayAudit audit{};
    double previous_equity = initial_capital;
    std::string previous_symbol;
    std::int64_t previous_time = 0;
    for (std::size_t i = 0; i < replay.steps.size(); ++i) {
        const auto& step = replay.steps[i];
        const auto& d = step.decision;
        const auto& e = step.execution;
        const auto& p = step.portfolio;
        require(d.decision_time > previous_time && e.decision_time == d.decision_time,
                "D9F4C historical audit: nonchronological execution");
        require(!d.terminal || i + 1 == replay.steps.size(),
                "D9F4C historical audit: terminal before last event");
        require(d.manager_action < 7 && d.legal_actions[d.manager_action],
                "D9F4C historical audit: illegal manager action");
        require(e.numerical_integrity && p.quantity >= 0 && p.last_time == d.decision_time,
                "D9F4C historical audit: invalid ledger state");
        require(near(e.equity_before, previous_equity) && near(e.equity_after, p.equity),
                "D9F4C historical audit: broken equity continuity");
        const double costs = e.brokerage + e.slippage + e.other_costs;
        require(std::isfinite(costs) && e.brokerage >= 0 &&
                e.slippage >= 0 && e.other_costs >= 0,
                "D9F4C historical audit: invalid costs");
        require(near(e.gross_pnl - costs, e.net_pnl) &&
                near(e.equity_after - e.equity_before, e.net_pnl),
                "D9F4C historical audit: gross/net mismatch");
        require(near(step.gross.execution_costs, costs) &&
                near(step.net.execution_costs, costs),
                "D9F4C historical audit: bridge costs mismatch");
        require(near(step.gross.selected_reward, step.gross.gross_reward) &&
                near(step.net.selected_reward, step.net.net_reward),
                "D9F4C historical audit: incorrect reward mode");
        require(near(step.gross.gross_reward - step.net.net_reward,
                     costs / initial_capital),
                "D9F4C historical audit: reward-cost reconciliation failed");
        require(step.gross.decision_time == d.decision_time &&
                step.net.decision_time == d.decision_time,
                "D9F4C historical audit: reward timestamp mismatch");
        const bool held = !previous_symbol.empty();
        const bool holds = held && p.held_symbol == previous_symbol;
        if (!held && !p.held_symbol.empty()) ++audit.entries;
        else if (held && p.held_symbol.empty()) ++audit.exits;
        else if (held && !p.held_symbol.empty() && !holds) ++audit.switches;
        else ++audit.holds; // includes FLAT -> FLAT WAIT
        if (holds || (!held && p.held_symbol.empty()))
            require(e.executed_legs == 0,
                    "D9F4C historical audit: unexpected execution leg");
        if (!held && !p.held_symbol.empty())
            require(e.executed_legs == 1, "D9F4C historical audit: missing entry leg");
        if (held && p.held_symbol.empty())
            require(e.executed_legs == 1, "D9F4C historical audit: missing exit leg");
        if (held && !p.held_symbol.empty() && !holds)
            require(e.executed_legs == 2, "D9F4C historical audit: missing switch leg");
        audit.total_gross_pnl += e.gross_pnl;
        audit.total_net_pnl += e.net_pnl;
        audit.total_execution_costs += costs;
        previous_equity = p.equity;
        previous_symbol = p.held_symbol;
        previous_time = d.decision_time;
        ++audit.events;
    }
    require(replay.steps.back().decision.terminal,
            "D9F4C historical audit: final event must be terminal");
    require(replay.final_portfolio.quantity == 0 &&
            replay.final_portfolio.held_symbol.empty(),
            "D9F4C historical audit: terminal position not flat");
    require(near(replay.final_portfolio.equity, previous_equity) &&
            near(audit.total_net_pnl, previous_equity - initial_capital) &&
            near(audit.total_gross_pnl - audit.total_execution_costs,
                 audit.total_net_pnl),
            "D9F4C historical audit: episode totals do not reconcile");
    audit.ending_equity = previous_equity;
    audit.terminal_flat = true;
    return audit;
}
} // namespace devai::strategy::d9f4c
