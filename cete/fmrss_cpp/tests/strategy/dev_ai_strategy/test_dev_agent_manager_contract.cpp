#include "devai/strategy/dev_ai_strategy/DevAgentManagerContract.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};

void require(bool condition, const std::string& text, TestStats& stats)
{
    ++stats.checks;
    if (condition)
        std::cout << "[PASS] " << text << '\n';
    else
    {
        ++stats.failures;
        std::cerr << "[FAIL] " << text << '\n';
    }
}

template <typename Function>
void expectThrow(Function&& fn, const std::string& text, TestStats& stats)
{
    bool threw = false;
    try { fn(); }
    catch (const std::exception&) { threw = true; }
    require(threw, text, stats);
}

DevManagerPortfolioState flatPortfolio()
{
    DevManagerPortfolioState p{};
    p.portfolio.position = DEV_POSITION_FLAT;
    p.portfolio.drawdown = 0.01;
    p.portfolio.holding_period = 0.0;
    p.numerical_integrity = true;
    return p;
}

DevManagerPortfolioState longPortfolio(const std::string& symbol)
{
    auto p = flatPortfolio();
    p.current_symbol = symbol;
    p.portfolio.position = DEV_POSITION_LONG;
    p.portfolio.unrealized_return = 0.012;
    p.portfolio.holding_period = 8.0;
    return p;
}

DevAgentReport makeReport(
    std::size_t slot,
    const std::string& symbol,
    std::int64_t time,
    double score,
    DevAction greedy_action)
{
    DevAgentReport r{};
    r.slot_index = slot;
    r.agent_name = DevTopCandidateSelector::agentNames()[slot];
    r.assigned = true;
    r.symbol = symbol;
    r.decision_time = time;
    r.candidate_score = score;
    r.candidate_rank = slot + 1;

    r.q_values.values = {-0.20, -0.10, -0.30, -0.40};
    switch (greedy_action)
    {
        case DevAction::HOLD:  r.q_values.values[0] = 0.50; break;
        case DevAction::LONG:  r.q_values.values[1] = 0.60; break;
        case DevAction::SHORT: r.q_values.values[2] = 0.70; break;
        case DevAction::EXIT:  r.q_values.values[3] = 0.80; break;
    }
    r.q_values.numerical_integrity = true;
    r.preferred_action = greedy_action;
    r.preferred_q_value = r.q_values.value(greedy_action);
    r.numerical_integrity = true;
    return r;
}

std::vector<DevAgentReport> reportsFor(
    std::size_t count,
    std::int64_t time)
{
    const std::array<std::string, 5> symbols{
        "RELIANCE", "SBIN", "BHEL", "INFY", "HDFCBANK"};

    std::vector<DevAgentReport> reports;
    for (std::size_t i = 0; i < count; ++i)
        reports.push_back(
            makeReport(i, symbols[i], time, 0.95 - 0.05 * i, DevAction::LONG));
    return reports;
}

void testZeroToFivePacketContract(TestStats& stats)
{
    DevAgentManagerContract contract;
    constexpr std::int64_t t = 6000;

    for (std::size_t count = 0; count <= 5; ++count)
    {
        const auto packet = contract.makePacket(t, reportsFor(count, t));

        require(packet.report_count == count,
                "Contract transmits exactly " + std::to_string(count) + " reports", stats);
        require(packet.decision_time == t && packet.numerical_integrity,
                "Contract preserves causal time/integrity for " + std::to_string(count) + " reports", stats);

        for (std::size_t slot = 0; slot < 5; ++slot)
            require(packet.agent_reports[slot].has_value() == (slot < count),
                    "Contract preserves assigned/EMPTY slot " + std::to_string(slot + 1) +
                    " for " + std::to_string(count) + " reports", stats);
    }
}

void testPacketToManagerState(TestStats& stats)
{
    DevAgentManagerContract contract;
    DevManager manager;
    constexpr std::int64_t t = 7000;

    const auto packet = contract.makePacket(t, reportsFor(4, t));
    const auto state = contract.buildManagerState(packet, flatPortfolio(), manager);

    require(state.decision_time == t,
            "Contract preserves decision time into DevManager state", stats);
    require(state.report_count == 4,
            "Contract preserves four reports into DevManager state", stats);
    require(!state.agent_reports[4].has_value(),
            "Contract preserves DevAgent_005 as EMPTY", stats);

    bool identities_preserved = true;
    for (std::size_t slot = 0; slot < 4; ++slot)
    {
        identities_preserved = identities_preserved &&
            state.agent_reports[slot]->slot_index == slot &&
            state.agent_reports[slot]->agent_name == DevTopCandidateSelector::agentNames()[slot] &&
            state.agent_reports[slot]->candidate_rank == slot + 1;
    }
    require(identities_preserved,
            "Contract preserves slot/name/rank identity into DevManager", stats);
}

void testDecisionEnvelope(TestStats& stats)
{
    DevAgentManagerContract contract;
    DevManager manager;
    constexpr std::int64_t t = 8000;

    const auto packet = contract.makePacket(t, reportsFor(2, t));
    const auto flat_state = contract.buildManagerState(packet, flatPortfolio(), manager);

    const auto no_trade = manager.makeNoTradeDecision(flat_state);
    const auto no_trade_envelope =
        contract.makeDecisionEnvelope(flat_state, no_trade, manager);
    require(no_trade_envelope.decision.type == DevManagerDecisionType::NO_TRADE,
            "Contract permits NO_TRADE", stats);

    const auto selected = manager.makeSelectAgentDecision(flat_state, 1);
    const auto selected_envelope =
        contract.makeDecisionEnvelope(flat_state, selected, manager);
    require(selected_envelope.decision.type == DevManagerDecisionType::SELECT_AGENT &&
            selected_envelope.decision.selected_symbol == "SBIN",
            "Contract permits SELECT_AGENT only through validated Manager decision", stats);

    const auto holding_state =
        contract.buildManagerState(packet, longPortfolio("RELIANCE"), manager);
    const auto keep = manager.makeKeepCurrentDecision(holding_state);
    const auto keep_envelope =
        contract.makeDecisionEnvelope(holding_state, keep, manager);
    require(keep_envelope.decision.type == DevManagerDecisionType::KEEP_CURRENT &&
            keep_envelope.decision.selected_symbol == "RELIANCE",
            "Contract permits KEEP_CURRENT for objective open CETE position", stats);
}

void testBoundaryRejections(TestStats& stats)
{
    DevAgentManagerContract contract;
    DevManager manager;
    constexpr std::int64_t t = 9000;

    expectThrow([&] { (void)contract.makePacket(0, {}); },
                "Contract rejects non-positive decision time", stats);

    auto wrong_time = makeReport(0, "RELIANCE", t + 60, 0.9, DevAction::LONG);
    expectThrow([&] { (void)contract.makePacket(t, {wrong_time}); },
                "Contract rejects stale/future-cycle agent report", stats);

    auto wrong_rank = makeReport(0, "RELIANCE", t, 0.9, DevAction::LONG);
    wrong_rank.candidate_rank = 2;
    expectThrow([&] { (void)contract.makePacket(t, {wrong_rank}); },
                "Contract freezes candidate rank to DevAgent slot", stats);

    auto wrong_name = makeReport(0, "RELIANCE", t, 0.9, DevAction::LONG);
    wrong_name.agent_name = "DevAgent_005";
    expectThrow([&] { (void)contract.makePacket(t, {wrong_name}); },
                "Contract rejects mismatched DevAgent identity", stats);

    auto empty = makeReport(0, "RELIANCE", t, 0.9, DevAction::LONG);
    empty.assigned = false;
    expectThrow([&] { (void)contract.makePacket(t, {empty}); },
                "Contract rejects manufactured report from EMPTY DevAgent", stats);

    auto wrong_q = makeReport(0, "RELIANCE", t, 0.9, DevAction::LONG);
    wrong_q.preferred_q_value += 1.0;
    expectThrow([&] { (void)contract.makePacket(t, {wrong_q}); },
                "Contract rejects preferred Q-value inconsistent with Q-vector", stats);

    auto wrong_action = makeReport(0, "RELIANCE", t, 0.9, DevAction::LONG);
    wrong_action.preferred_action = DevAction::HOLD;
    wrong_action.preferred_q_value = wrong_action.q_values.value(DevAction::HOLD);
    expectThrow([&] { (void)contract.makePacket(t, {wrong_action}); },
                "Contract rejects preferred action inconsistent with greedy Q action", stats);

    auto nan_q = makeReport(0, "RELIANCE", t, 0.9, DevAction::LONG);
    nan_q.q_values.values[2] = std::numeric_limits<double>::quiet_NaN();
    expectThrow([&] { (void)contract.makePacket(t, {nan_q}); },
                "Contract rejects non-finite Q-value", stats);

    auto duplicate_symbol = reportsFor(2, t);
    duplicate_symbol[1].symbol = duplicate_symbol[0].symbol;
    expectThrow([&] { (void)contract.makePacket(t, duplicate_symbol); },
                "Contract rejects duplicate stock symbols", stats);

    // Manually constructed packet with slot 0 EMPTY and slot 1 populated.
    DevAgentManagerPacket hole{};
    hole.decision_time = t;
    hole.agent_reports[1] = makeReport(1, "SBIN", t, 0.8, DevAction::LONG);
    hole.report_count = 1;
    hole.numerical_integrity = true;
    expectThrow([&] { contract.validatePacket(hole); },
                "Contract rejects holes between assigned DevAgent slots", stats);

    auto packet = contract.makePacket(t, reportsFor(2, t));
    auto state = contract.buildManagerState(packet, flatPortfolio(), manager);
    auto decision = manager.makeSelectAgentDecision(state, 0);
    auto envelope = contract.makeDecisionEnvelope(state, decision, manager);
    envelope.decision_time += 60;
    expectThrow([&] { contract.validateDecisionEnvelope(state, envelope, manager); },
                "Contract rejects decision envelope from another decision time", stats);
}

void testNoHiddenManagerPolicy(TestStats& stats)
{
    DevAgentManagerContract contract;
    DevManager manager;
    constexpr std::int64_t t = 10000;

    auto reports = reportsFor(2, t);
    reports[0].q_values.values = {0.0, 0.2, -0.1, -0.2};
    reports[0].preferred_action = DevAction::LONG;
    reports[0].preferred_q_value = 0.2;
    reports[1].q_values.values = {0.0, 999.0, -0.1, -0.2};
    reports[1].preferred_action = DevAction::LONG;
    reports[1].preferred_q_value = 999.0;

    const auto packet = contract.makePacket(t, reports);
    const auto state = contract.buildManagerState(packet, flatPortfolio(), manager);
    const auto decision = manager.makeNoTradeDecision(state);
    const auto envelope = contract.makeDecisionEnvelope(state, decision, manager);

    require(envelope.decision.type == DevManagerDecisionType::NO_TRADE,
            "Decision contract does not auto-select largest cross-stock Q-value", stats);
    require(!envelope.decision.selected_slot_index.has_value(),
            "Decision contract contains no hidden stock-selection policy", stats);
}

} // namespace

int main()
{
    TestStats stats{};

    std::cout << "\n======================================================================\n";
    std::cout << "DEVAI PHASE 3.14 — DEVAGENT -> DEVMANAGER DECISION CONTRACT\n";
    std::cout << "======================================================================\n";

    testZeroToFivePacketContract(stats);
    testPacketToManagerState(stats);
    testDecisionEnvelope(stats);
    testBoundaryRejections(stats);
    testNoHiddenManagerPolicy(stats);

    std::cout << "\n======================================================================\n";
    std::cout << "PHASE 3.14 DECISION CONTRACT SUMMARY\n";
    std::cout << "======================================================================\n";
    std::cout << "Checks   : " << stats.checks << '\n';
    std::cout << "Failures : " << stats.failures << '\n';
    std::cout << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout << "DEVAI PHASE 3.14 DECISION CONTRACT: PASSED\n";
        return 0;
    }

    std::cout << "DEVAI PHASE 3.14 DECISION CONTRACT: FAILED\n";
    return 1;
}
