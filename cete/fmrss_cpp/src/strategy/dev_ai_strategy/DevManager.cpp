#include "devai/strategy/dev_ai_strategy/DevManager.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace devai::strategy
{
namespace
{

bool finitePortfolio(const DevPortfolioState& p) noexcept
{
    return
        std::isfinite(p.position) &&
        std::isfinite(p.unrealized_return) &&
        std::isfinite(p.realized_return) &&
        std::isfinite(p.equity_return) &&
        std::isfinite(p.drawdown) &&
        std::isfinite(p.holding_period);
}

bool finiteQValues(const DevQValues& q) noexcept
{
    if (!q.numerical_integrity)
        return false;

    for (const double value : q.values)
        if (!std::isfinite(value))
            return false;

    return true;
}

void validateSlot(const std::size_t slot_index)
{
    if (slot_index >= DEV_AGENT_SLOT_COUNT)
        throw std::out_of_range(
            "DevManager agent slot index is outside 0..4.");
}

} // namespace

void DevManager::validatePortfolio(
    const DevManagerPortfolioState& state)
{
    if (!state.numerical_integrity)
        throw std::invalid_argument(
            "DevManager portfolio lacks numerical integrity.");

    if (!finitePortfolio(state.portfolio))
        throw std::invalid_argument(
            "DevManager portfolio contains non-finite values.");

    if (!isValidDevPosition(state.portfolio.position))
        throw std::invalid_argument(
            "DevManager portfolio contains invalid position state.");

    if (state.portfolio.drawdown < 0.0 ||
        state.portfolio.drawdown > 1.0)
        throw std::invalid_argument(
            "DevManager portfolio drawdown must be within [0, 1].");

    if (state.portfolio.holding_period < 0.0)
        throw std::invalid_argument(
            "DevManager holding period cannot be negative.");

    const bool flat =
        state.portfolio.position == DEV_POSITION_FLAT;

    if (flat && !state.current_symbol.empty())
        throw std::invalid_argument(
            "DevManager flat portfolio cannot name a current symbol.");

    if (!flat && state.current_symbol.empty())
        throw std::invalid_argument(
            "DevManager open position requires a current symbol.");
}

void DevManager::validateReport(
    const DevAgentReport& report,
    const std::int64_t decision_time)
{
    if (!report.assigned)
        throw std::invalid_argument(
            "DevManager cannot ingest an EMPTY DevAgent report.");

    validateSlot(report.slot_index);

    const auto& expected_names =
        DevTopCandidateSelector::agentNames();

    if (report.agent_name != expected_names[report.slot_index])
        throw std::invalid_argument(
            "DevManager report agent identity does not match slot.");

    if (report.symbol.empty())
        throw std::invalid_argument(
            "DevManager report symbol cannot be empty.");

    if (report.decision_time != decision_time)
        throw std::invalid_argument(
            "DevManager report decision time does not match manager cycle.");

    if (!std::isfinite(report.candidate_score))
        throw std::invalid_argument(
            "DevManager report candidate score must be finite.");

    if (report.candidate_rank != report.slot_index + 1)
        throw std::invalid_argument(
            "DevManager report candidate rank must match agent slot (ranks 1..5).");

    if (!finiteQValues(report.q_values))
        throw std::invalid_argument(
            "DevManager report contains invalid Q-values.");

    if (!isValidAction(report.preferred_action))
        throw std::invalid_argument(
            "DevManager report contains invalid preferred action.");

    if (!std::isfinite(report.preferred_q_value))
        throw std::invalid_argument(
            "DevManager report preferred Q-value must be finite.");

    if (!report.numerical_integrity)
        throw std::invalid_argument(
            "DevManager report lacks numerical integrity.");
}

DevManagerState DevManager::buildState(
    const std::int64_t decision_time,
    const std::vector<DevAgentReport>& reports,
    const DevManagerPortfolioState& portfolio) const
{
    if (decision_time <= 0)
        throw std::invalid_argument(
            "DevManager decision time must be positive.");

    if (reports.size() > DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument(
            "DevManager cannot ingest more than six reports.");

    validatePortfolio(portfolio);

    DevManagerState state{};
    state.decision_time = decision_time;
    state.portfolio = portfolio;

    std::unordered_set<std::string> symbols;

    for (const auto& report : reports)
    {
        validateReport(report, decision_time);

        if (state.agent_reports[report.slot_index].has_value())
            throw std::invalid_argument(
                "DevManager received duplicate agent slot report.");

        if (!symbols.insert(report.symbol).second)
            throw std::invalid_argument(
                "DevManager received duplicate stock symbol reports.");

        state.agent_reports[report.slot_index] = report;
        ++state.report_count;
    }

    state.numerical_integrity = true;
    validateState(state);
    return state;
}

void DevManager::validateState(
    const DevManagerState& state) const
{
    if (!state.numerical_integrity)
        throw std::invalid_argument(
            "DevManager state lacks numerical integrity.");

    if (state.decision_time <= 0)
        throw std::invalid_argument(
            "DevManager state decision time must be positive.");

    validatePortfolio(state.portfolio);

    std::size_t count = 0;
    std::unordered_set<std::string> symbols;

    for (std::size_t slot = 0;
         slot < DEV_AGENT_SLOT_COUNT;
         ++slot)
    {
        if (!state.agent_reports[slot].has_value())
            continue;

        const auto& report = *state.agent_reports[slot];
        validateReport(report, state.decision_time);

        if (report.slot_index != slot)
            throw std::invalid_argument(
                "DevManager state report is stored in wrong slot.");

        if (!symbols.insert(report.symbol).second)
            throw std::invalid_argument(
                "DevManager state contains duplicate symbols.");

        ++count;
    }

    if (count != state.report_count)
        throw std::invalid_argument(
            "DevManager state report count does not match populated slots.");
}

DevManagerDecision DevManager::makeNoTradeDecision(
    const DevManagerState& state) const
{
    validateState(state);

    DevManagerDecision decision{};
    decision.type = DevManagerDecisionType::NO_TRADE;
    decision.decision_time = state.decision_time;
    decision.numerical_integrity = true;

    validateDecision(state, decision);
    return decision;
}

DevManagerDecision DevManager::makeKeepCurrentDecision(
    const DevManagerState& state) const
{
    validateState(state);

    if (state.portfolio.portfolio.position == DEV_POSITION_FLAT)
        throw std::logic_error(
            "DevManager cannot KEEP_CURRENT while portfolio is flat.");

    DevManagerDecision decision{};
    decision.type = DevManagerDecisionType::KEEP_CURRENT;
    decision.decision_time = state.decision_time;
    decision.selected_symbol = state.portfolio.current_symbol;
    decision.numerical_integrity = true;

    validateDecision(state, decision);
    return decision;
}

DevManagerDecision DevManager::makeSelectAgentDecision(
    const DevManagerState& state,
    const std::size_t slot_index) const
{
    validateState(state);
    validateSlot(slot_index);

    if (!state.agent_reports[slot_index].has_value())
        throw std::logic_error(
            "DevManager cannot select an EMPTY DevAgent slot.");

    const auto& report = *state.agent_reports[slot_index];

    DevManagerDecision decision{};
    decision.type = DevManagerDecisionType::SELECT_AGENT;
    decision.decision_time = state.decision_time;
    decision.selected_slot_index = slot_index;
    decision.selected_agent_name = report.agent_name;
    decision.selected_symbol = report.symbol;
    decision.numerical_integrity = true;

    validateDecision(state, decision);
    return decision;
}

void DevManager::validateDecision(
    const DevManagerState& state,
    const DevManagerDecision& decision) const
{
    validateState(state);

    if (!decision.numerical_integrity)
        throw std::invalid_argument(
            "DevManager decision lacks numerical integrity.");

    if (!isValidManagerDecisionType(decision.type))
        throw std::invalid_argument(
            "DevManager decision type is invalid.");

    if (decision.decision_time != state.decision_time)
        throw std::invalid_argument(
            "DevManager decision time does not match state time.");

    switch (decision.type)
    {
        case DevManagerDecisionType::NO_TRADE:
            if (decision.selected_slot_index.has_value() ||
                !decision.selected_agent_name.empty() ||
                !decision.selected_symbol.empty())
                throw std::invalid_argument(
                    "NO_TRADE cannot select an agent or symbol.");
            return;

        case DevManagerDecisionType::KEEP_CURRENT:
            if (state.portfolio.portfolio.position == DEV_POSITION_FLAT)
                throw std::invalid_argument(
                    "KEEP_CURRENT requires an open position.");
            if (decision.selected_slot_index.has_value() ||
                !decision.selected_agent_name.empty() ||
                decision.selected_symbol != state.portfolio.current_symbol)
                throw std::invalid_argument(
                    "KEEP_CURRENT must preserve only the current symbol.");
            return;

        case DevManagerDecisionType::SELECT_AGENT:
            if (!decision.selected_slot_index.has_value())
                throw std::invalid_argument(
                    "SELECT_AGENT requires a selected slot.");

            validateSlot(*decision.selected_slot_index);

            if (!state.agent_reports[*decision.selected_slot_index].has_value())
                throw std::invalid_argument(
                    "SELECT_AGENT cannot reference an EMPTY slot.");

            {
                const auto& report =
                    *state.agent_reports[*decision.selected_slot_index];

                if (decision.selected_agent_name != report.agent_name ||
                    decision.selected_symbol != report.symbol)
                    throw std::invalid_argument(
                        "SELECT_AGENT identity does not match selected report.");
            }
            return;
    }

    throw std::invalid_argument(
        "DevManager decision type is invalid.");
}

} // namespace devai::strategy
