#pragma once

#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace devai::strategy
{

struct DevAgentReport
{
    std::size_t slot_index{0};
    std::string agent_name;
    bool assigned{false};

    std::string symbol;
    std::int64_t decision_time{0};
    double candidate_score{0.0};
    std::size_t candidate_rank{0};

    DevQValues q_values{};
    DevAction preferred_action{DevAction::HOLD};
    double preferred_q_value{0.0};

    bool numerical_integrity{false};
};

class DevAgent
{
public:
    DevAgent(
        std::size_t slot_index,
        std::string agent_name,
        const DevQNetwork& shared_network);

    [[nodiscard]] std::size_t slotIndex() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] bool assigned() const noexcept;
    [[nodiscard]] const DevAgentAssignment& assignment() const noexcept;

    void assign(const DevAgentAssignment& assignment);
    void clear() noexcept;

    [[nodiscard]] DevAgentReport evaluate(
        const DevAIObservation& observation) const;

private:
    std::size_t slot_index_;
    std::string agent_name_;
    const DevQNetwork* shared_network_{nullptr};
    DevAgentAssignment assignment_{};
};

class DevAgents
{
public:
    explicit DevAgents(const DevQNetwork& shared_network);

    void applySelection(const DevTopCandidateSelection& selection);
    void clear() noexcept;

    [[nodiscard]] const DevAgent& agent(std::size_t slot_index) const;
    [[nodiscard]] DevAgent& agent(std::size_t slot_index);
    [[nodiscard]] std::size_t assignedCount() const noexcept;

private:
    std::array<DevAgent, DEV_AGENT_SLOT_COUNT> agents_;
};

} // namespace devai::strategy
