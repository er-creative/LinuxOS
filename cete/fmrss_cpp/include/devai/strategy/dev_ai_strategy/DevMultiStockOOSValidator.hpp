#pragma once

#include "devai/strategy/dev_ai_strategy/DevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevAgentManagerContract.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevPortfolioRotation.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy
{

// Phase 3.19 validates the frozen DevAI decision stack on chronological OOS
// states. Economic execution is injected by CETE through ExecutionProvider;
// this class never invents prices, fills, brokerage, slippage or P&L.
struct DevOOSCandidate
{
    DevAgentAssignment assignment{};
    DevAIObservation observation{};
};

struct DevOOSDecisionPoint
{
    std::int64_t decision_time{0};
    std::string benchmark_symbol;
    std::vector<DevOOSCandidate> candidates; // Up to five contiguous LONG-only slots: ranks 1..5
    DevManagerPortfolioState portfolio{};    // objective CETE state at t
    bool numerical_integrity{false};
};

struct DevOOSExecutionResult
{
    std::int64_t decision_time{0};
    double equity_before{0.0};
    double equity_after{0.0};
    double gross_pnl{0.0};
    double brokerage{0.0};
    double slippage{0.0};
    double other_costs{0.0};
    double net_pnl{0.0};
    double drawdown{0.0};
    std::size_t executed_legs{0};
    bool numerical_integrity{false};
};

using DevOOSExecutionProvider = std::function<DevOOSExecutionResult(
    const DevOOSDecisionPoint&,
    const DevManagerState&,
    const DevManagerDecision&,
    const DevPortfolioRotationPlan&)>;

struct DevMultiStockOOSSummary
{
    std::size_t decision_count{0};
    std::size_t no_trade_count{0};
    std::size_t keep_current_count{0};
    std::size_t select_agent_count{0};
    std::size_t rotation_count{0};
    std::size_t executed_legs{0};
    std::size_t profitable_steps{0};
    std::size_t losing_steps{0};

    double initial_equity{0.0};
    double final_equity{0.0};
    double gross_pnl{0.0};
    double brokerage{0.0};
    double slippage{0.0};
    double other_costs{0.0};
    double net_pnl{0.0};
    double net_return{0.0};
    double gross_profit{0.0};
    double gross_loss{0.0};
    double profit_factor{0.0};
    double maximum_drawdown{0.0};

    bool numerical_integrity{false};
};

class DevMultiStockOOSValidator
{
public:
    DevMultiStockOOSValidator(
        const DevQNetwork& frozen_agent_network,
        const DevManagerQNetwork& frozen_manager_network,
        DevPortfolioAllocationConfig allocation = {});

    [[nodiscard]] DevMultiStockOOSSummary run(
        const std::vector<DevOOSDecisionPoint>& points,
        const DevOOSExecutionProvider& execution_provider) const;

    static void validatePoint(const DevOOSDecisionPoint& point);

private:
    [[nodiscard]] static DevManagerDecision materializeGreedyDecision(
        DevManagerAction action,
        const DevManagerState& state,
        const DevManager& manager);

    const DevQNetwork* agent_network_{nullptr};
    const DevManagerQNetwork* manager_network_{nullptr};
    DevPortfolioRotationEngine rotation_engine_{};
};

} // namespace devai::strategy
