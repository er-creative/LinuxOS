#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using devai::strategy::DEV_AGENT_SLOT_COUNT;
using devai::strategy::DevCandidateScore;
using devai::strategy::DevTopCandidateSelector;

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};

void require(bool condition, const std::string& message, TestStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout << "[PASS] " << message << '\n';
        return;
    }

    ++stats.failures;
    std::cout << "[FAIL] " << message << '\n';
}

template <typename Fn>
void requireThrows(Fn&& fn, const std::string& message, TestStats& stats)
{
    bool threw = false;

    try
    {
        fn();
    }
    catch (const std::exception&)
    {
        threw = true;
    }

    require(threw, message, stats);
}

DevCandidateScore candidate(
    std::string symbol,
    double score,
    bool eligible = true,
    std::int64_t decision_time = 1000)
{
    DevCandidateScore value;
    value.symbol = std::move(symbol);
    value.decision_time = decision_time;
    value.score = score;
    value.eligible = eligible;
    value.numerical_integrity = true;
    return value;
}

} // namespace

int main()
{
    TestStats stats;

    std::cout
        << "\n======================================================================\n"
        << "DEVAI PHASE 3.11 — TOP-5 SELECTION FOUNDATION\n"
        << "======================================================================\n";

    require(
        DEV_AGENT_SLOT_COUNT == 5,
        "Exactly five DevAgent assignment slots are frozen",
        stats);

    const auto& names = DevTopCandidateSelector::agentNames();

    require(names[0] == "DevAgent_001", "Slot 1 name is DevAgent_001", stats);
    require(names[1] == "DevAgent_002", "Slot 2 name is DevAgent_002", stats);
    require(names[2] == "DevAgent_003", "Slot 3 name is DevAgent_003", stats);
    require(names[3] == "DevAgent_004", "Slot 4 name is DevAgent_004", stats);
    require(names[4] == "DevAgent_005", "Slot 5 name is DevAgent_005", stats);

    DevTopCandidateSelector selector;

    require(
        selector.maximumSelected() == 5,
        "Default selector maximum is five candidates",
        stats);

    const std::vector<DevCandidateScore> universe =
    {
        candidate("RELIANCE", 0.82),
        candidate("SBIN", 0.76),
        candidate("BHEL", 0.91),
        candidate("INFY", 0.79),
        candidate("HDFCBANK", 0.74),
        candidate("ICICIBANK", 0.70),
        candidate("AXISBANK", 0.68),
        candidate("TCS", 0.95, false)
    };

    const auto selection = selector.select(universe, "NIFTY%2050");

    require(selection.numerical_integrity,
            "Top-5 result has numerical integrity", stats);
    require(selection.decision_time == 1000,
            "Selection preserves exact decision time", stats);
    require(selection.benchmark_symbol == "NIFTY%2050",
            "Selection preserves benchmark identity", stats);
    require(selection.input_candidate_count == 8,
            "Selection records complete input candidate count", stats);
    require(selection.eligible_candidate_count == 7,
            "Ineligible candidate is excluded before Top-5 selection", stats);
    require(selection.selected_candidate_count == 5,
            "Five highest-scoring eligible LONG candidates are selected", stats);

    require(selection.assignments[0].symbol == "BHEL",
            "Highest eligible score is assigned to DevAgent_001", stats);
    require(selection.assignments[1].symbol == "RELIANCE",
            "Second eligible score is assigned to DevAgent_002", stats);
    require(selection.assignments[2].symbol == "INFY",
            "Third eligible score is assigned to DevAgent_003", stats);
    require(selection.assignments[3].symbol == "SBIN",
            "Fourth eligible score is assigned to DevAgent_004", stats);
    require(selection.assignments[4].assigned && selection.assignments[4].symbol == "HDFCBANK",
            "Fifth-ranked LONG candidate is assigned to DevAgent_005", stats);

    for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i)
    {
        require(selection.assignments[i].assigned == true,
                "Selected DevAgent slot is marked assigned", stats);
        require(selection.assignments[i].candidate_rank == i + 1,
                "Selected DevAgent slot preserves deterministic candidate rank", stats);
        require(selection.assignments[i].agent_name == names[i],
                "Selected slot preserves frozen DevAgent identity", stats);
        require(selection.assignments[i].decision_time == 1000,
                "Selected slot preserves causal decision time", stats);
    }

    require(
        selection.assignments[0].symbol != "TCS" &&
        selection.assignments[1].symbol != "TCS" &&
        selection.assignments[2].symbol != "TCS" &&
        selection.assignments[3].symbol != "TCS" &&
        selection.assignments[4].symbol != "TCS",
        "Higher-scoring but ineligible candidate cannot enter Top-5",
        stats);

    const std::vector<DevCandidateScore> tie_universe =
    {
        candidate("ZZZ", 0.80),
        candidate("AAA", 0.80),
        candidate("MMM", 0.80)
    };

    const auto tie_selection = selector.select(tie_universe, "NIFTY%2050");

    require(tie_selection.assignments[0].symbol == "AAA",
            "Equal-score tie uses symbol ordering deterministically", stats);
    require(tie_selection.assignments[1].symbol == "MMM",
            "Equal-score deterministic ordering is stable", stats);
    require(tie_selection.assignments[2].symbol == "ZZZ",
            "Equal-score deterministic ordering completes correctly", stats);
    require(!tie_selection.assignments[3].assigned,
            "Unused DevAgent_004 slot remains unassigned", stats);
    require(!tie_selection.assignments[4].assigned,
            "Unused DevAgent_005 slot remains unassigned", stats);

    requireThrows(
        [] { DevTopCandidateSelector invalid(0); },
        "Zero maximum-selected configuration is rejected",
        stats);

    requireThrows(
        [] { DevTopCandidateSelector invalid(6); },
        "More than five selected candidates is rejected",
        stats);

    requireThrows(
        [&selector]
        {
            selector.select({}, "NIFTY%2050");
        },
        "Empty candidate universe is rejected",
        stats);

    requireThrows(
        [&selector, &universe]
        {
            selector.select(universe, "");
        },
        "Empty benchmark identity is rejected",
        stats);

    requireThrows(
        [&selector]
        {
            auto values = std::vector<DevCandidateScore>
            {
                candidate("NIFTY%2050", 1.0)
            };
            selector.select(values, "NIFTY%2050");
        },
        "Benchmark contamination is rejected",
        stats);

    requireThrows(
        [&selector]
        {
            auto values = std::vector<DevCandidateScore>
            {
                candidate("RELIANCE", 0.8),
                candidate("RELIANCE", 0.7)
            };
            selector.select(values, "NIFTY%2050");
        },
        "Duplicate stock symbols are rejected",
        stats);

    requireThrows(
        [&selector]
        {
            auto values = std::vector<DevCandidateScore>
            {
                candidate("RELIANCE", 0.8, true, 1000),
                candidate("SBIN", 0.7, true, 1060)
            };
            selector.select(values, "NIFTY%2050");
        },
        "Mixed decision times are rejected",
        stats);

    requireThrows(
        [&selector]
        {
            auto bad = candidate("RELIANCE", 0.8);
            bad.score = std::numeric_limits<double>::quiet_NaN();
            selector.select({bad}, "NIFTY%2050");
        },
        "Non-finite candidate score is rejected",
        stats);

    requireThrows(
        [&selector]
        {
            auto bad = candidate("RELIANCE", 0.8);
            bad.numerical_integrity = false;
            selector.select({bad}, "NIFTY%2050");
        },
        "Candidate without numerical integrity is rejected",
        stats);

    std::cout
        << "\n----------------------------------------------------------------------\n"
        << "TOP-5 ASSIGNMENT SUMMARY\n"
        << "----------------------------------------------------------------------\n";

    for (const auto& assignment : selection.assignments)
    {
        std::cout
            << assignment.agent_name
            << " -> "
            << (assignment.assigned ? assignment.symbol : "UNASSIGNED")
            << " | Rank=" << assignment.candidate_rank
            << " | Score=" << assignment.candidate_score
            << '\n';
    }

    std::cout
        << "\n======================================================================\n"
        << "PHASE 3.11 TOP-5 SELECTION FOUNDATION SUMMARY\n"
        << "======================================================================\n"
        << "Checks   : " << stats.checks << '\n'
        << "Failures : " << stats.failures << '\n'
        << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.11 TOP-5 SELECTION FOUNDATION: PASSED\n";
        return 0;
    }

    std::cout
        << "DEVAI PHASE 3.11 TOP-5 SELECTION FOUNDATION: FAILED\n";
    return 1;
}
