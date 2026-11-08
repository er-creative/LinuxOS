#include "devai/strategy/dev_ai_strategy/DevMultiStockOOSValidator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace devai::strategy
{
namespace
{
bool finite(double v) noexcept { return std::isfinite(v); }
bool same(double a, double b) noexcept
{
    return finite(a) && finite(b) && std::fabs(a-b) <= 1.0e-8;
}

void validateExecution(const DevOOSExecutionResult& x, std::int64_t t)
{
    if (!x.numerical_integrity || x.decision_time != t ||
        !finite(x.equity_before) || !finite(x.equity_after) ||
        !finite(x.gross_pnl) || !finite(x.brokerage) || !finite(x.slippage) ||
        !finite(x.other_costs) || !finite(x.net_pnl) || !finite(x.drawdown) ||
        x.equity_before <= 0.0 || x.equity_after <= 0.0 ||
        x.brokerage < 0.0 || x.slippage < 0.0 || x.other_costs < 0.0 ||
        x.drawdown < 0.0 || x.drawdown > 1.0)
        throw std::invalid_argument("DevMultiStockOOSValidator: invalid CETE execution result.");

    const double expected_net = x.gross_pnl - x.brokerage - x.slippage - x.other_costs;
    if (!same(expected_net, x.net_pnl) || !same(x.equity_before + x.net_pnl, x.equity_after))
        throw std::invalid_argument("DevMultiStockOOSValidator: CETE economic reconciliation failed.");
}
}

DevMultiStockOOSValidator::DevMultiStockOOSValidator(
    const DevQNetwork& frozen_agent_network,
    const DevManagerQNetwork& frozen_manager_network,
    DevPortfolioAllocationConfig allocation)
    : agent_network_(&frozen_agent_network),
      manager_network_(&frozen_manager_network),
      rotation_engine_(allocation)
{}

void DevMultiStockOOSValidator::validatePoint(const DevOOSDecisionPoint& point)
{
    if (!point.numerical_integrity || point.decision_time <= 0 ||
        point.benchmark_symbol.empty() || point.candidates.size() > DEV_AGENT_SLOT_COUNT ||
        !point.portfolio.numerical_integrity)
        throw std::invalid_argument("DevMultiStockOOSValidator: invalid OOS point.");

    std::unordered_set<std::string> symbols;
    for (std::size_t i=0; i<point.candidates.size(); ++i)
    {
        const auto& c = point.candidates[i];
        const auto& a = c.assignment;
        const std::size_t expected_rank = i + 1;
        auto invalid = [&](const char* field) {
            throw std::invalid_argument(
                "DevMultiStockOOSValidator: invalid candidate/observation contract: slot=" +
                std::to_string(i) + ", field=" + field +
                ", actual_rank=" + std::to_string(a.candidate_rank) +
                ", expected_rank=" + std::to_string(expected_rank));
        };
        if (!a.assigned) invalid("assigned");
        if (a.slot_index != i) invalid("slot_index");
        if (a.candidate_rank != expected_rank) invalid("candidate_rank");
        if (a.agent_name != DevTopCandidateSelector::agentNames()[i]) invalid("agent_name");
        if (a.symbol.empty()) invalid("symbol_empty");
        if (a.symbol == point.benchmark_symbol) invalid("benchmark_contamination");
        if (a.decision_time != point.decision_time) invalid("assignment_decision_time");
        if (!finite(a.candidate_score)) invalid("candidate_score");
        if (!c.observation.numerical_integrity) invalid("observation_numerical_integrity");
        if (!c.observation.ready) invalid("observation_ready");
        if (c.observation.decision_time != point.decision_time) invalid("observation_decision_time");
        if (c.observation.symbol != a.symbol) invalid("observation_symbol");
        if (!symbols.insert(a.symbol).second) invalid("duplicate_symbol");
    }
}

DevManagerDecision DevMultiStockOOSValidator::materializeGreedyDecision(
    DevManagerAction action, const DevManagerState& state, const DevManager& manager)
{
    switch (action)
    {
        case DevManagerAction::NO_TRADE: return manager.makeNoTradeDecision(state);
        case DevManagerAction::KEEP_CURRENT: return manager.makeKeepCurrentDecision(state);
        case DevManagerAction::SELECT_AGENT_001: return manager.makeSelectAgentDecision(state,0);
        case DevManagerAction::SELECT_AGENT_002: return manager.makeSelectAgentDecision(state,1);
        case DevManagerAction::SELECT_AGENT_003: return manager.makeSelectAgentDecision(state,2);
        case DevManagerAction::SELECT_AGENT_004: return manager.makeSelectAgentDecision(state,3);
        case DevManagerAction::SELECT_AGENT_005: return manager.makeSelectAgentDecision(state,4);
    }
    throw std::invalid_argument("DevMultiStockOOSValidator: invalid Manager action.");
}

DevMultiStockOOSSummary DevMultiStockOOSValidator::run(
    const std::vector<DevOOSDecisionPoint>& points,
    const DevOOSExecutionProvider& execution_provider) const
{
    if (points.empty() || !execution_provider)
        throw std::invalid_argument("DevMultiStockOOSValidator: OOS sequence/provider cannot be empty.");

    const auto agent_before = agent_network_->parameters();
    const auto manager_before = manager_network_->parameters();

    DevManager manager;
    DevAgentManagerContract contract;
    DevManagerStateEncoder manager_encoder;
    DevMultiStockOOSSummary s{};
    std::int64_t previous_time = 0;
    double previous_equity_after = 0.0;

    for (const auto& point : points)
    {
        validatePoint(point);
        if (previous_time != 0 && point.decision_time <= previous_time)
            throw std::invalid_argument("DevMultiStockOOSValidator: OOS points are not strictly chronological.");

        DevAgents agents(*agent_network_);
        DevTopCandidateSelection selection{};
        selection.decision_time = point.decision_time;
        selection.benchmark_symbol = point.benchmark_symbol;
        selection.input_candidate_count = point.candidates.size();
        selection.eligible_candidate_count = point.candidates.size();
        selection.selected_candidate_count = point.candidates.size();
        selection.numerical_integrity = true;
        for (std::size_t i=0; i<point.candidates.size(); ++i)
            selection.assignments[i] = point.candidates[i].assignment;
        agents.applySelection(selection);

        std::vector<DevAgentReport> reports;
        reports.reserve(point.candidates.size());
        for (std::size_t i=0; i<point.candidates.size(); ++i)
            reports.push_back(agents.agent(i).evaluate(point.candidates[i].observation));

        const auto packet = contract.makePacket(point.decision_time, reports);
        const auto manager_state = contract.buildManagerState(packet, point.portfolio, manager);
        const auto encoded = manager_encoder.encode(manager_state);
        const auto q = manager_network_->predict(encoded);
        const auto action = q.greedyAction(encoded.valid_actions);
        const auto decision = materializeGreedyDecision(action, manager_state, manager);
        manager.validateDecision(manager_state, decision);
        const auto plan = rotation_engine_.plan(manager_state, decision, manager);

        const auto economic = execution_provider(point, manager_state, decision, plan);
        validateExecution(economic, point.decision_time);
        if (s.decision_count == 0) s.initial_equity = economic.equity_before;
        else if (!same(previous_equity_after, economic.equity_before))
            throw std::invalid_argument("DevMultiStockOOSValidator: equity path is discontinuous.");

        ++s.decision_count;
        if (decision.type == DevManagerDecisionType::NO_TRADE) ++s.no_trade_count;
        else if (decision.type == DevManagerDecisionType::KEEP_CURRENT) ++s.keep_current_count;
        else ++s.select_agent_count;
        if (plan.rotation_type == DevPortfolioRotationType::ROTATE_TO_SELECTED ||
            plan.rotation_type == DevPortfolioRotationType::REVERSE_SELECTED) ++s.rotation_count;
        s.executed_legs += economic.executed_legs;
        s.gross_pnl += economic.gross_pnl;
        s.brokerage += economic.brokerage;
        s.slippage += economic.slippage;
        s.other_costs += economic.other_costs;
        s.net_pnl += economic.net_pnl;
        s.maximum_drawdown = std::max(s.maximum_drawdown, economic.drawdown);
        if (economic.net_pnl > 0.0) { ++s.profitable_steps; s.gross_profit += economic.net_pnl; }
        else if (economic.net_pnl < 0.0) { ++s.losing_steps; s.gross_loss += -economic.net_pnl; }
        previous_equity_after = economic.equity_after;
        previous_time = point.decision_time;
    }

    s.final_equity = previous_equity_after;
    s.net_return = s.net_pnl / s.initial_equity;
    s.profit_factor = s.gross_loss > 0.0 ? s.gross_profit/s.gross_loss :
                      (s.gross_profit > 0.0 ? std::numeric_limits<double>::infinity() : 0.0);
    s.numerical_integrity = finite(s.initial_equity) && finite(s.final_equity) &&
                            finite(s.net_pnl) && finite(s.net_return) && finite(s.maximum_drawdown);

    // OOS is evaluation-only: neither frozen network may be mutated.
    const auto agent_after = agent_network_->parameters();
    const auto manager_after = manager_network_->parameters();
    if (agent_before.weights1 != agent_after.weights1 || agent_before.biases1 != agent_after.biases1 ||
        agent_before.weights2 != agent_after.weights2 || agent_before.biases2 != agent_after.biases2 ||
        agent_before.weights3 != agent_after.weights3 || agent_before.biases3 != agent_after.biases3 ||
        manager_before.weights1 != manager_after.weights1 || manager_before.biases1 != manager_after.biases1 ||
        manager_before.weights2 != manager_after.weights2 || manager_before.biases2 != manager_after.biases2 ||
        manager_before.weights3 != manager_after.weights3 || manager_before.biases3 != manager_after.biases3)
        throw std::runtime_error("DevMultiStockOOSValidator: OOS evaluation mutated a frozen model.");

    return s;
}

} // namespace devai::strategy
