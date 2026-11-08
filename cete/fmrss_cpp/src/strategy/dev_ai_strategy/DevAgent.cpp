#include "devai/strategy/dev_ai_strategy/DevAgent.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy
{
namespace
{
void validateSlot(std::size_t slot_index)
{
    if (slot_index >= DEV_AGENT_SLOT_COUNT)
        throw std::out_of_range("DevAgent slot index is outside 0..4.");
}

bool finiteQValues(const DevQValues& q)
{
    if (!q.numerical_integrity)
        return false;
    for (double value : q.values)
        if (!std::isfinite(value))
            return false;
    return true;
}
} // namespace

DevAgent::DevAgent(
    std::size_t slot_index,
    std::string agent_name,
    const DevQNetwork& shared_network)
    : slot_index_(slot_index),
      agent_name_(std::move(agent_name)),
      shared_network_(&shared_network)
{
    validateSlot(slot_index_);
    if (agent_name_.empty())
        throw std::invalid_argument("DevAgent name cannot be empty.");
    clear();
}

std::size_t DevAgent::slotIndex() const noexcept { return slot_index_; }
const std::string& DevAgent::name() const noexcept { return agent_name_; }
bool DevAgent::assigned() const noexcept { return assignment_.assigned; }
const DevAgentAssignment& DevAgent::assignment() const noexcept { return assignment_; }

void DevAgent::assign(const DevAgentAssignment& assignment)
{
    if (!assignment.assigned)
        throw std::invalid_argument("DevAgent cannot assign an unassigned slot record.");
    if (assignment.slot_index != slot_index_)
        throw std::invalid_argument("DevAgent assignment slot does not match agent slot.");
    if (assignment.agent_name != agent_name_)
        throw std::invalid_argument("DevAgent assignment name does not match agent identity.");
    if (assignment.symbol.empty())
        throw std::invalid_argument("DevAgent assigned symbol cannot be empty.");
    if (assignment.decision_time <= 0)
        throw std::invalid_argument("DevAgent assignment decision time must be positive.");
    if (!std::isfinite(assignment.candidate_score))
        throw std::invalid_argument("DevAgent candidate score must be finite.");
    const std::size_t expected_rank = slot_index_ + 1U;
    if (assignment.candidate_rank != expected_rank)
        throw std::invalid_argument("DevAgent candidate rank mismatch: slot=" + std::to_string(slot_index_) + ", actual=" + std::to_string(assignment.candidate_rank) + ", expected=" + std::to_string(expected_rank) + " (1..5).");
    assignment_ = assignment;
}

void DevAgent::clear() noexcept
{
    assignment_ = {};
    assignment_.slot_index = slot_index_;
    assignment_.agent_name = agent_name_;
    assignment_.assigned = false;
}

DevAgentReport DevAgent::evaluate(const DevAIObservation& observation) const
{
    if (!assigned())
        throw std::logic_error("EMPTY DevAgent cannot evaluate an observation.");
    if (shared_network_ == nullptr)
        throw std::logic_error("DevAgent has no shared Q-network.");
    if (!observation.ready || !observation.numerical_integrity)
        throw std::invalid_argument("DevAgent observation is not ready with numerical integrity.");
    if (observation.symbol != assignment_.symbol)
        throw std::invalid_argument("DevAgent observation symbol does not match assigned symbol.");
    if (observation.decision_time != assignment_.decision_time)
        throw std::invalid_argument("DevAgent observation time does not match assignment time.");

    DevAgentReport report{};
    report.slot_index = slot_index_;
    report.agent_name = agent_name_;
    report.assigned = true;
    report.symbol = assignment_.symbol;
    report.decision_time = assignment_.decision_time;
    report.candidate_score = assignment_.candidate_score;
    report.candidate_rank = assignment_.candidate_rank;
    report.q_values = shared_network_->predict(observation);

    if (!finiteQValues(report.q_values))
        throw std::runtime_error("DevAgent Q-network produced non-finite output.");

    report.preferred_action = report.q_values.greedyAction();
    if (!isValidAction(report.preferred_action))
        throw std::runtime_error("DevAgent Q-network produced an invalid action.");

    report.preferred_q_value = report.q_values.value(report.preferred_action);
    report.numerical_integrity = std::isfinite(report.preferred_q_value);
    if (!report.numerical_integrity)
        throw std::runtime_error("DevAgent report failed numerical integrity.");
    return report;
}

DevAgents::DevAgents(const DevQNetwork& shared_network)
    : agents_{
          DevAgent{0, DevTopCandidateSelector::agentNames()[0], shared_network},
          DevAgent{1, DevTopCandidateSelector::agentNames()[1], shared_network},
          DevAgent{2, DevTopCandidateSelector::agentNames()[2], shared_network},
          DevAgent{3, DevTopCandidateSelector::agentNames()[3], shared_network},
          DevAgent{4, DevTopCandidateSelector::agentNames()[4], shared_network}}
{
}

void DevAgents::applySelection(const DevTopCandidateSelection& selection)
{
    if (!selection.numerical_integrity)
        throw std::invalid_argument("DevAgents selection lacks numerical integrity.");
    if (selection.decision_time <= 0)
        throw std::invalid_argument("DevAgents selection decision time must be positive.");
    if (selection.selected_candidate_count > DEV_AGENT_SLOT_COUNT)
        throw std::invalid_argument("DevAgents selection exceeds five slots.");

    // Validate and stage every assignment before changing the active agents.
    // A rejected fifth-slot assignment must not leave slots 0..3 modified.
    auto staged = agents_;
    for (auto& agent : staged)
        agent.clear();

    std::size_t counted = 0;
    for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i)
    {
        const auto& a = selection.assignments[i];
        if (!a.assigned)
            continue;
        if (a.slot_index != i)
            throw std::invalid_argument("DevAgents selection contains mismatched slot index.");
        staged[i].assign(a);
        ++counted;
    }
    if (counted != selection.selected_candidate_count)
        throw std::invalid_argument("DevAgents selected count does not match assigned slots.");

    // Commit only after all checks pass.
    agents_.swap(staged);
}

void DevAgents::clear() noexcept
{
    for (auto& a : agents_)
        a.clear();
}

const DevAgent& DevAgents::agent(std::size_t slot_index) const
{
    validateSlot(slot_index);
    return agents_[slot_index];
}

DevAgent& DevAgents::agent(std::size_t slot_index)
{
    validateSlot(slot_index);
    return agents_[slot_index];
}

std::size_t DevAgents::assignedCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& a : agents_)
        if (a.assigned()) ++count;
    return count;
}

} // namespace devai::strategy
