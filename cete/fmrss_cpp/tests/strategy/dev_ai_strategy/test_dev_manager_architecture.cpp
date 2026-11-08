#include "devai/strategy/dev_ai_strategy/DevManager.hpp"

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
    p.current_symbol.clear();
    p.portfolio.position = DEV_POSITION_FLAT;
    p.portfolio.drawdown = 0.02;
    p.portfolio.holding_period = 0.0;
    p.numerical_integrity = true;
    return p;
}

DevManagerPortfolioState longPortfolio(const std::string& symbol)
{
    auto p = flatPortfolio();
    p.current_symbol = symbol;
    p.portfolio.position = DEV_POSITION_LONG;
    p.portfolio.unrealized_return = 0.01;
    p.portfolio.holding_period = 7.0;
    return p;
}

DevAgentReport report(
    std::size_t slot,
    const std::string& symbol,
    std::int64_t time,
    double score,
    DevAction action)
{
    DevAgentReport r{};
    r.slot_index = slot;
    r.agent_name = DevTopCandidateSelector::agentNames()[slot];
    r.assigned = true;
    r.symbol = symbol;
    r.decision_time = time;
    r.candidate_score = score;
    r.candidate_rank = slot + 1;
    r.q_values.values = {0.10, 0.20, 0.05, -0.10};
    r.q_values.numerical_integrity = true;
    r.preferred_action = action;
    r.preferred_q_value = r.q_values.value(action);
    r.numerical_integrity = true;
    return r;
}

void testZeroToFiveReports(TestStats& stats)
{
    DevManager manager;
    constexpr std::int64_t t = 2000;
    const std::array<std::string, 5> symbols{
        "RELIANCE", "SBIN", "BHEL", "INFY", "HDFCBANK"};

    for (std::size_t count = 0; count <= 5; ++count)
    {
        std::vector<DevAgentReport> reports;
        for (std::size_t i = 0; i < count; ++i)
            reports.push_back(report(i, symbols[i], t, 1.0 - 0.1 * i, DevAction::LONG));

        const auto state = manager.buildState(t, reports, flatPortfolio());
        require(state.report_count == count,
                "DevManager accepts " + std::to_string(count) + " assigned DevAgent reports", stats);
        require(state.numerical_integrity,
                "DevManager state has numerical integrity for " + std::to_string(count) + " reports", stats);

        for (std::size_t i = 0; i < 5; ++i)
            require(state.agent_reports[i].has_value() == (i < count),
                    "DevManager preserves assigned/EMPTY slot " + std::to_string(i + 1) +
                    " for " + std::to_string(count) + " reports", stats);
    }
}

void testDecisions(TestStats& stats)
{
    DevManager manager;
    constexpr std::int64_t t = 3000;
    std::vector<DevAgentReport> reports{
        report(0, "RELIANCE", t, 0.91, DevAction::LONG),
        report(1, "SBIN", t, 0.82, DevAction::HOLD)};

    const auto flat = manager.buildState(t, reports, flatPortfolio());
    const auto no_trade = manager.makeNoTradeDecision(flat);
    require(no_trade.type == DevManagerDecisionType::NO_TRADE,
            "DevManager can explicitly choose NO_TRADE", stats);
    require(!no_trade.selected_slot_index.has_value() && no_trade.selected_symbol.empty(),
            "NO_TRADE does not manufacture a stock selection", stats);

    const auto selected = manager.makeSelectAgentDecision(flat, 0);
    require(selected.type == DevManagerDecisionType::SELECT_AGENT,
            "DevManager can explicitly select an assigned DevAgent", stats);
    require(selected.selected_slot_index == std::optional<std::size_t>{0},
            "SELECT_AGENT preserves selected slot", stats);
    require(selected.selected_agent_name == "DevAgent_001" &&
            selected.selected_symbol == "RELIANCE",
            "SELECT_AGENT preserves selected agent and symbol", stats);

    expectThrow([&] { (void)manager.makeSelectAgentDecision(flat, 4); },
                "DevManager cannot select an EMPTY DevAgent", stats);
    expectThrow([&] { (void)manager.makeKeepCurrentDecision(flat); },
                "DevManager cannot KEEP_CURRENT while flat", stats);

    const auto holding = manager.buildState(t, reports, longPortfolio("RELIANCE"));
    const auto keep = manager.makeKeepCurrentDecision(holding);
    require(keep.type == DevManagerDecisionType::KEEP_CURRENT,
            "DevManager can KEEP_CURRENT when CETE has an open position", stats);
    require(keep.selected_symbol == "RELIANCE" && !keep.selected_slot_index.has_value(),
            "KEEP_CURRENT preserves current symbol without selecting an agent slot", stats);
}

void testValidation(TestStats& stats)
{
    DevManager manager;
    constexpr std::int64_t t = 4000;

    expectThrow([&] { (void)manager.buildState(0, {}, flatPortfolio()); },
                "DevManager rejects non-positive decision time", stats);

    std::vector<DevAgentReport> six;
    for (std::size_t i = 0; i < 5; ++i)
        six.push_back(report(i, "S" + std::to_string(i), t, 1.0, DevAction::LONG));
    six.push_back(six.front());
    expectThrow([&] { (void)manager.buildState(t, six, flatPortfolio()); },
                "DevManager rejects more than five reports", stats);

    auto duplicate_slot = std::vector<DevAgentReport>{
        report(0, "RELIANCE", t, 0.9, DevAction::LONG),
        report(0, "SBIN", t, 0.8, DevAction::LONG)};
    expectThrow([&] { (void)manager.buildState(t, duplicate_slot, flatPortfolio()); },
                "DevManager rejects duplicate DevAgent slots", stats);

    auto duplicate_symbol = std::vector<DevAgentReport>{
        report(0, "RELIANCE", t, 0.9, DevAction::LONG),
        report(1, "RELIANCE", t, 0.8, DevAction::LONG)};
    expectThrow([&] { (void)manager.buildState(t, duplicate_symbol, flatPortfolio()); },
                "DevManager rejects duplicate stock symbols", stats);

    auto wrong_time = report(0, "RELIANCE", t + 60, 0.9, DevAction::LONG);
    expectThrow([&] { (void)manager.buildState(t, {wrong_time}, flatPortfolio()); },
                "DevManager rejects report from another decision time", stats);

    auto empty_report = report(0, "RELIANCE", t, 0.9, DevAction::LONG);
    empty_report.assigned = false;
    expectThrow([&] { (void)manager.buildState(t, {empty_report}, flatPortfolio()); },
                "DevManager rejects manufactured report from EMPTY agent", stats);

    auto bad_name = report(0, "RELIANCE", t, 0.9, DevAction::LONG);
    bad_name.agent_name = "DevAgent_005";
    expectThrow([&] { (void)manager.buildState(t, {bad_name}, flatPortfolio()); },
                "DevManager rejects agent identity that does not match slot", stats);

    auto bad_q = report(0, "RELIANCE", t, 0.9, DevAction::LONG);
    bad_q.q_values.values[0] = std::numeric_limits<double>::quiet_NaN();
    expectThrow([&] { (void)manager.buildState(t, {bad_q}, flatPortfolio()); },
                "DevManager rejects non-finite agent Q-values", stats);

    auto bad_portfolio = flatPortfolio();
    bad_portfolio.current_symbol = "RELIANCE";
    expectThrow([&] { (void)manager.buildState(t, {}, bad_portfolio); },
                "DevManager rejects named holding while CETE portfolio is flat", stats);

    auto missing_symbol = longPortfolio("RELIANCE");
    missing_symbol.current_symbol.clear();
    expectThrow([&] { (void)manager.buildState(t, {}, missing_symbol); },
                "DevManager rejects open position without current symbol", stats);
}

void testArchitectureDoesNotAutoCompareQ(TestStats& stats)
{
    DevManager manager;
    constexpr std::int64_t t = 5000;

    auto first = report(0, "RELIANCE", t, 0.80, DevAction::LONG);
    auto second = report(1, "SBIN", t, 0.79, DevAction::LONG);
    first.q_values.values[1] = 0.20;
    first.preferred_q_value = 0.20;
    second.q_values.values[1] = 999.0;
    second.preferred_q_value = 999.0;

    const auto state = manager.buildState(t, {first, second}, flatPortfolio());
    const auto no_trade = manager.makeNoTradeDecision(state);

    require(no_trade.type == DevManagerDecisionType::NO_TRADE,
            "Phase 3.13 does not automatically choose largest cross-stock Q-value", stats);

    const auto explicit_first = manager.makeSelectAgentDecision(state, 0);
    require(explicit_first.selected_symbol == "RELIANCE",
            "Phase 3.13 selection is explicit, not hidden max-Q policy", stats);
}

} // namespace

int main()
{
    TestStats stats{};

    std::cout << "\n======================================================================\n";
    std::cout << "DEVAI PHASE 3.13 — DEVMANAGER ARCHITECTURE\n";
    std::cout << "======================================================================\n";

    testZeroToFiveReports(stats);
    testDecisions(stats);
    testValidation(stats);
    testArchitectureDoesNotAutoCompareQ(stats);

    std::cout << "\n======================================================================\n";
    std::cout << "PHASE 3.13 DEVMANAGER ARCHITECTURE SUMMARY\n";
    std::cout << "======================================================================\n";
    std::cout << "Checks   : " << stats.checks << '\n';
    std::cout << "Failures : " << stats.failures << '\n';
    std::cout << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout << "DEVAI PHASE 3.13 DEVMANAGER ARCHITECTURE: PASSED\n";
        return 0;
    }

    std::cout << "DEVAI PHASE 3.13 DEVMANAGER ARCHITECTURE: FAILED\n";
    return 1;
}
