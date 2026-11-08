#pragma once

#include "devai/strategy/dev_ai_strategy/DevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace devai::strategy
{

enum class DevManagerDecisionType : std::size_t
{
    NO_TRADE = 0,
    KEEP_CURRENT = 1,
    SELECT_AGENT = 2
};

[[nodiscard]]
constexpr bool isValidManagerDecisionType(
    DevManagerDecisionType type) noexcept
{
    switch (type)
    {
        case DevManagerDecisionType::NO_TRADE:
        case DevManagerDecisionType::KEEP_CURRENT:
        case DevManagerDecisionType::SELECT_AGENT:
            return true;
    }
    return false;
}

struct DevManagerPortfolioState
{
    // Empty when CETE currently has no open stock position.
    std::string current_symbol;

    // Reuse the frozen objective portfolio/account state.
    DevPortfolioState portfolio{};

    bool numerical_integrity{false};
};

struct DevManagerState
{
    std::int64_t decision_time{0};

    // Slot i corresponds permanently to DevAgent_00(i+1).
    // A missing optional means that agent slot is EMPTY for this cycle.
    std::array<std::optional<DevAgentReport>, DEV_AGENT_SLOT_COUNT>
        agent_reports{};

    std::size_t report_count{0};
    DevManagerPortfolioState portfolio{};

    bool numerical_integrity{false};
};

struct DevManagerDecision
{
    DevManagerDecisionType type{DevManagerDecisionType::NO_TRADE};
    std::int64_t decision_time{0};

    // Populated only for SELECT_AGENT.
    std::optional<std::size_t> selected_slot_index{};
    std::string selected_agent_name;
    std::string selected_symbol;

    bool numerical_integrity{false};
};

class DevManager
{
public:
    DevManager() = default;

    // Phase 3.13 only constructs and validates manager state.
    // It deliberately does not compare Q-values or choose a stock automatically.
    [[nodiscard]]
    DevManagerState buildState(
        std::int64_t decision_time,
        const std::vector<DevAgentReport>& reports,
        const DevManagerPortfolioState& portfolio) const;

    // Explicit architecture-level decisions. Policy/learning comes later.
    [[nodiscard]]
    DevManagerDecision makeNoTradeDecision(
        const DevManagerState& state) const;

    [[nodiscard]]
    DevManagerDecision makeKeepCurrentDecision(
        const DevManagerState& state) const;

    [[nodiscard]]
    DevManagerDecision makeSelectAgentDecision(
        const DevManagerState& state,
        std::size_t slot_index) const;

    void validateState(const DevManagerState& state) const;
    void validateDecision(
        const DevManagerState& state,
        const DevManagerDecision& decision) const;

private:
    static void validatePortfolio(
        const DevManagerPortfolioState& portfolio);

    static void validateReport(
        const DevAgentReport& report,
        std::int64_t decision_time);
};

} // namespace devai::strategy
