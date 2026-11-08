#include "devai/strategy/dev_ai_strategy/DevAgentManagerContract.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace devai::strategy
{
namespace
{

bool finiteQValues(const DevQValues& q) noexcept
{
    if (!q.numerical_integrity)
        return false;

    for (const double value : q.values)
        if (!std::isfinite(value))
            return false;

    return true;
}

} // namespace

void DevAgentManagerContract::validateReportForContract(
    const DevAgentReport& report,
    const std::int64_t decision_time,
    const std::size_t expected_slot)
{
    if (expected_slot >= DEV_AGENT_SLOT_COUNT)
        throw std::out_of_range(
            "DevAgentManagerContract expected slot is outside 0..4.");

    if (!report.assigned)
        throw std::invalid_argument(
            "DevAgentManagerContract cannot transmit an EMPTY DevAgent report.");

    if (report.slot_index != expected_slot)
        throw std::invalid_argument(
            "DevAgentManagerContract report is stored in the wrong slot.");

    const auto& expected_names =
        DevTopCandidateSelector::agentNames();

    if (report.agent_name != expected_names[expected_slot])
        throw std::invalid_argument(
            "DevAgentManagerContract agent identity does not match slot.");

    if (report.symbol.empty())
        throw std::invalid_argument(
            "DevAgentManagerContract report symbol cannot be empty.");

    if (report.decision_time != decision_time)
        throw std::invalid_argument(
            "DevAgentManagerContract report time does not match packet time.");

    // Frozen five-agent assignment contract:
    // Slots 0..4 receive candidate ranks 1..5 (LONG-only).
    // No sixth agent or slot is permitted.
    if (report.candidate_rank != expected_slot + 1U)
        throw std::invalid_argument(
            "DevAgentManagerContract candidate rank does not match agent slot.");

    if (!std::isfinite(report.candidate_score))
        throw std::invalid_argument(
            "DevAgentManagerContract candidate score must be finite.");

    if (!finiteQValues(report.q_values))
        throw std::invalid_argument(
            "DevAgentManagerContract report contains invalid Q-values.");

    if (!isValidAction(report.preferred_action))
        throw std::invalid_argument(
            "DevAgentManagerContract report contains invalid preferred action.");

    if (!std::isfinite(report.preferred_q_value))
        throw std::invalid_argument(
            "DevAgentManagerContract preferred Q-value must be finite.");

    const double expected_q =
        report.q_values.value(report.preferred_action);

    if (report.preferred_q_value != expected_q)
        throw std::invalid_argument(
            "DevAgentManagerContract preferred Q-value does not match Q-vector.");

    if (report.preferred_action != report.q_values.greedyAction())
        throw std::invalid_argument(
            "DevAgentManagerContract preferred action does not match greedy Q action.");

    if (!report.numerical_integrity)
        throw std::invalid_argument(
            "DevAgentManagerContract report lacks numerical integrity.");
}

DevAgentManagerPacket DevAgentManagerContract::makePacket(
    const std::int64_t decision_time,
    const std::vector<DevAgentReport>& reports) const
{
    if (decision_time <= 0)
        throw std::invalid_argument(
            "DevAgentManagerContract decision time must be positive.");

    if (reports.size() > DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument(
            "DevAgentManagerContract cannot transmit more than five reports.");

    DevAgentManagerPacket packet{};
    packet.decision_time = decision_time;

    std::unordered_set<std::string> symbols;

    for (const auto& report : reports)
    {
        if (report.slot_index >= DEV_AGENT_SLOT_COUNT)
            throw std::out_of_range(
                "DevAgentManagerContract report slot is outside 0..4.");

        validateReportForContract(
            report,
            decision_time,
            report.slot_index);

        if (packet.agent_reports[report.slot_index].has_value())
            throw std::invalid_argument(
                "DevAgentManagerContract received duplicate agent slot.");

        if (!symbols.insert(report.symbol).second)
            throw std::invalid_argument(
                "DevAgentManagerContract received duplicate stock symbol.");

        packet.agent_reports[report.slot_index] = report;
        ++packet.report_count;
    }

    packet.numerical_integrity = true;
    validatePacket(packet);
    return packet;
}

void DevAgentManagerContract::validatePacket(
    const DevAgentManagerPacket& packet) const
{
    if (!packet.numerical_integrity)
        throw std::invalid_argument(
            "DevAgentManagerContract packet lacks numerical integrity.");

    if (packet.decision_time <= 0)
        throw std::invalid_argument(
            "DevAgentManagerContract packet decision time must be positive.");

    if (packet.report_count > DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument(
            "DevAgentManagerContract packet report count exceeds five.");

    std::size_t count = 0;
    bool encountered_empty = false;
    std::unordered_set<std::string> symbols;

    for (std::size_t slot = 0;
         slot < DEV_AGENT_SLOT_COUNT;
         ++slot)
    {
        if (!packet.agent_reports[slot].has_value())
        {
            encountered_empty = true;
            continue;
        }

        // With Top-5 rank assignment there may not be a later populated slot
        // after an EMPTY earlier slot. 0..N-1 are populated; N..4 are EMPTY.
        if (encountered_empty)
            throw std::invalid_argument(
                "DevAgentManagerContract packet contains a hole in assigned slots.");

        const auto& report = *packet.agent_reports[slot];

        validateReportForContract(
            report,
            packet.decision_time,
            slot);

        if (!symbols.insert(report.symbol).second)
            throw std::invalid_argument(
                "DevAgentManagerContract packet contains duplicate stock symbols.");

        ++count;
    }

    if (count != packet.report_count)
        throw std::invalid_argument(
            "DevAgentManagerContract packet report count does not match slots.");
}

DevManagerState DevAgentManagerContract::buildManagerState(
    const DevAgentManagerPacket& packet,
    const DevManagerPortfolioState& portfolio,
    const DevManager& manager) const
{
    validatePacket(packet);

    std::vector<DevAgentReport> reports;
    reports.reserve(packet.report_count);

    for (const auto& maybe_report : packet.agent_reports)
        if (maybe_report.has_value())
            reports.push_back(*maybe_report);

    const auto state =
        manager.buildState(
            packet.decision_time,
            reports,
            portfolio);

    if (state.report_count != packet.report_count)
        throw std::runtime_error(
            "DevAgentManagerContract Manager state changed report count.");

    for (std::size_t slot = 0;
         slot < DEV_AGENT_SLOT_COUNT;
         ++slot)
    {
        if (state.agent_reports[slot].has_value() !=
            packet.agent_reports[slot].has_value())
            throw std::runtime_error(
                "DevAgentManagerContract Manager state changed slot occupancy.");
    }

    return state;
}

DevManagerDecisionEnvelope
DevAgentManagerContract::makeDecisionEnvelope(
    const DevManagerState& state,
    const DevManagerDecision& decision,
    const DevManager& manager) const
{
    manager.validateState(state);
    manager.validateDecision(state, decision);

    DevManagerDecisionEnvelope envelope{};
    envelope.decision_time = state.decision_time;
    envelope.decision = decision;
    envelope.numerical_integrity = true;

    validateDecisionEnvelope(
        state,
        envelope,
        manager);

    return envelope;
}

void DevAgentManagerContract::validateDecisionEnvelope(
    const DevManagerState& state,
    const DevManagerDecisionEnvelope& envelope,
    const DevManager& manager) const
{
    manager.validateState(state);

    if (!envelope.numerical_integrity)
        throw std::invalid_argument(
            "DevAgentManagerContract decision envelope lacks numerical integrity.");

    if (envelope.decision_time != state.decision_time)
        throw std::invalid_argument(
            "DevAgentManagerContract envelope time does not match Manager state.");

    if (envelope.decision.decision_time != envelope.decision_time)
        throw std::invalid_argument(
            "DevAgentManagerContract decision time does not match envelope time.");

    manager.validateDecision(
        state,
        envelope.decision);
}

} // namespace devai::strategy
