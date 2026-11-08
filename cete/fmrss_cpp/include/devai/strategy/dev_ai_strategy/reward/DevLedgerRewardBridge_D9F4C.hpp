#pragma once
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace devai::strategy::d9f4c {
enum class LedgerRewardMode { GROSS_MARKET_REWARD, NET_EXECUTION_REWARD };
// Shared manager actions: NO_TRADE, KEEP_CURRENT, slots 0..5.
// Keep this mask synchronized with the manager encoder and shared network.
struct LedgerTransitionInput {
    std::int64_t decision_time{};
    std::int64_t next_decision_time{};
    std::size_t manager_action{};
    std::array<bool, DEV_MANAGER_ACTION_COUNT> legal_actions{};
    double equity_before{};
    double equity_after{};
    double gross_pnl{};
    double net_pnl{};
    double brokerage{};
    double slippage{};
    double other_costs{};
    double reference_capital{7000.0};
    bool terminal{};
    bool executed_ledger_reconciled{};
};
struct LedgerRewardTransition {
    std::int64_t decision_time{};
    std::int64_t next_decision_time{};
    std::size_t manager_action{};
    double selected_reward{};
    double gross_reward{};
    double net_reward{};
    double execution_costs{};
    bool terminal{};
};
class DevLedgerRewardBridge final {
public:
    [[nodiscard]] static LedgerRewardTransition build(const LedgerTransitionInput&, LedgerRewardMode);
};
} // namespace devai::strategy::d9f4c
