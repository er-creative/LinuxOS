#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace devai::strategy
{

inline constexpr std::size_t DEV_AGENT_SLOT_COUNT = 5;

// Strategy-side candidate supplied to Phase 3.11.
// score is an ordering score, NOT a probability.
struct DevCandidateScore
{
    std::string symbol;
    std::int64_t decision_time{0};
    double score{0.0};
    bool eligible{false};
    bool numerical_integrity{false};
};

struct DevAgentAssignment
{
    std::size_t slot_index{0};              // 0..4
    std::string agent_name;                 // DevAgent_001 .. DevAgent_005
    std::string symbol;
    std::int64_t decision_time{0};
    double candidate_score{0.0};
    std::size_t candidate_rank{0};          // 1..5, LONG-only; 0 if unassigned
    bool assigned{false};
};

struct DevTopCandidateSelection
{
    std::int64_t decision_time{0};
    std::string benchmark_symbol;

    std::size_t input_candidate_count{0};
    std::size_t eligible_candidate_count{0};
    std::size_t selected_candidate_count{0};

    std::array<DevAgentAssignment, DEV_AGENT_SLOT_COUNT> assignments{};

    bool numerical_integrity{false};
};

class DevTopCandidateSelector
{
public:
    explicit DevTopCandidateSelector(
        std::size_t maximum_selected = DEV_AGENT_SLOT_COUNT);

    [[nodiscard]]
    std::size_t maximumSelected() const noexcept;

    [[nodiscard]]
    DevTopCandidateSelection select(
        const std::vector<DevCandidateScore>& candidates,
        const std::string& benchmark_symbol) const;

    [[nodiscard]]
    static const std::array<std::string, DEV_AGENT_SLOT_COUNT>&
    agentNames() noexcept;

private:
    std::size_t maximum_selected_;
};

} // namespace devai::strategy
