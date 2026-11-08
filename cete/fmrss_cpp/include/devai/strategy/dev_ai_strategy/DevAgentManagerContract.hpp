#pragma once

#include "devai/strategy/dev_ai_strategy/DevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevManager.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace devai::strategy
{

// ============================================================================
// PHASE 3.14 — DEVAGENT -> DEVMANAGER DECISION CONTRACT
//
// This is a structural/causal boundary only. It does NOT:
//   - rank stocks
//   - evaluate observations
//   - compare Q-values across stocks
//   - choose a Manager policy
//   - allocate capital
//   - calculate brokerage/slippage/P&L/risk
//   - execute trades
//   - expose future returns/outcomes
// ============================================================================

struct DevAgentManagerPacket
{
    std::int64_t decision_time{0};

    // Fixed five-slot wire contract. Missing optional = EMPTY DevAgent.
    std::array<std::optional<DevAgentReport>, DEV_AGENT_SLOT_COUNT>
        agent_reports{};

    std::size_t report_count{0};
    bool numerical_integrity{false};
};

struct DevManagerDecisionEnvelope
{
    std::int64_t decision_time{0};
    DevManagerDecision decision{};
    bool numerical_integrity{false};
};

class DevAgentManagerContract
{
public:
    // Convert the reports produced by assigned DevAgents into one fixed-slot,
    // same-decision-time packet for DevManager.
    [[nodiscard]]
    DevAgentManagerPacket makePacket(
        std::int64_t decision_time,
        const std::vector<DevAgentReport>& reports) const;

    // Validate an already-constructed packet. This is intentionally public so
    // persisted/replayed/integration data can be checked at the boundary.
    void validatePacket(
        const DevAgentManagerPacket& packet) const;

    // Convert the validated packet plus objective CETE portfolio state into the
    // frozen Phase-3.13 DevManagerState. DevManager remains the owner of its
    // own state validation.
    [[nodiscard]]
    DevManagerState buildManagerState(
        const DevAgentManagerPacket& packet,
        const DevManagerPortfolioState& portfolio,
        const DevManager& manager) const;

    // Seal a Manager decision for downstream DevAI/CETE integration. This does
    // not create or modify the decision; it only validates the exact decision
    // against the exact Manager state that produced it.
    [[nodiscard]]
    DevManagerDecisionEnvelope makeDecisionEnvelope(
        const DevManagerState& state,
        const DevManagerDecision& decision,
        const DevManager& manager) const;

    void validateDecisionEnvelope(
        const DevManagerState& state,
        const DevManagerDecisionEnvelope& envelope,
        const DevManager& manager) const;

private:
    static void validateReportForContract(
        const DevAgentReport& report,
        std::int64_t decision_time,
        std::size_t expected_slot);
};

} // namespace devai::strategy
