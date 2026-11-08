#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy
{

namespace
{

static_assert(DEV_AGENT_SLOT_COUNT == 5,
    "CETE DevTopCandidateSelector requires exactly five agent slots.");

const std::array<std::string, DEV_AGENT_SLOT_COUNT> AGENT_NAMES =
{
    "DevAgent_001",
    "DevAgent_002",
    "DevAgent_003",
    "DevAgent_004",
    "DevAgent_005"
};

void validateCandidateIdentity(
    const DevCandidateScore& candidate,
    const std::string& benchmark_symbol,
    std::int64_t expected_decision_time)
{
    if (candidate.symbol.empty())
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: candidate symbol cannot be empty.");
    }

    if (candidate.symbol == benchmark_symbol)
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: benchmark cannot be selected as a DevAgent candidate.");
    }

    if (candidate.decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: candidate decision time must be positive.");
    }

    if (candidate.decision_time != expected_decision_time)
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: all candidates must share one decision time.");
    }

    if (!std::isfinite(candidate.score))
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: candidate score must be finite.");
    }

    if (!candidate.numerical_integrity)
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: candidate failed numerical-integrity validation.");
    }
}

} // namespace

DevTopCandidateSelector::DevTopCandidateSelector(
    std::size_t maximum_selected)
    : maximum_selected_(maximum_selected)
{
    if (maximum_selected_ == 0 ||
        maximum_selected_ > DEV_AGENT_SLOT_COUNT)
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: invalid maximum_selected=" +
            std::to_string(maximum_selected_) +
            ", DEV_AGENT_SLOT_COUNT=" +
            std::to_string(DEV_AGENT_SLOT_COUNT));
    }
}

std::size_t
DevTopCandidateSelector::maximumSelected() const noexcept
{
    return maximum_selected_;
}

const std::array<std::string, DEV_AGENT_SLOT_COUNT>&
DevTopCandidateSelector::agentNames() noexcept
{
    return AGENT_NAMES;
}

DevTopCandidateSelection
DevTopCandidateSelector::select(
    const std::vector<DevCandidateScore>& candidates,
    const std::string& benchmark_symbol) const
{
    if (candidates.empty())
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: candidate set cannot be empty.");
    }

    if (benchmark_symbol.empty())
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: benchmark symbol cannot be empty.");
    }

    const std::int64_t decision_time =
        candidates.front().decision_time;

    if (decision_time <= 0)
    {
        throw std::invalid_argument(
            "DevTopCandidateSelector: decision time must be positive.");
    }

    std::unordered_set<std::string> symbols;
    symbols.reserve(candidates.size());

    std::vector<DevCandidateScore> eligible;
    eligible.reserve(candidates.size());

    for (const auto& candidate : candidates)
    {
        validateCandidateIdentity(
            candidate,
            benchmark_symbol,
            decision_time);

        if (!symbols.insert(candidate.symbol).second)
        {
            throw std::invalid_argument(
                "DevTopCandidateSelector: duplicate candidate symbol: " +
                candidate.symbol);
        }

        if (candidate.eligible)
        {
            eligible.push_back(candidate);
        }
    }

    // Higher strategy-side score ranks first.
    // Symbol is the deterministic tie-breaker.
    std::sort(
        eligible.begin(),
        eligible.end(),
        [](const DevCandidateScore& lhs,
           const DevCandidateScore& rhs)
        {
            if (lhs.score != rhs.score)
            {
                return lhs.score > rhs.score;
            }

            return lhs.symbol < rhs.symbol;
        });

    DevTopCandidateSelection result;
    result.decision_time = decision_time;
    result.benchmark_symbol = benchmark_symbol;
    result.input_candidate_count = candidates.size();
    result.eligible_candidate_count = eligible.size();

    for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i)
    {
        result.assignments[i].slot_index = i;
        result.assignments[i].agent_name = AGENT_NAMES[i];
        result.assignments[i].decision_time = decision_time;
    }

    // Stage 5D: all five slots are LONG-eligible top-ranked candidates.
    const std::size_t selected_count =
        std::min(maximum_selected_, eligible.size());

    for (std::size_t i = 0; i < selected_count; ++i)
    {
        auto& assignment = result.assignments[i];
        assignment.symbol = eligible[i].symbol;
        assignment.candidate_score = eligible[i].score;
        assignment.candidate_rank = i + 1;
        assignment.assigned = true;
    }

    result.selected_candidate_count = 0;
    for (const auto& assignment : result.assignments)
    {
        if (assignment.assigned)
        {
            ++result.selected_candidate_count;
        }
    }
    result.numerical_integrity = true;

    return result;
}

} // namespace devai::strategy
