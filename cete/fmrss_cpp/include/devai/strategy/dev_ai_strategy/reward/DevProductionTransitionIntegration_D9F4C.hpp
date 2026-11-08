#pragma once
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include "devai/strategy/dev_ai_strategy/reward/DevLedgerRewardBridge_D9F4C.hpp"
#include <vector>

namespace devai::strategy::d9f4c {
// One record per ACTUAL ledger.execute event, not a fabricated portfolio mark.
// The caller supplies causal manager states and the actual chosen shared-manager action.
// This class NEVER trains or writes a checkpoint.
struct ProductionExecution {
    DevManagerState state{};
    DevManagerAction action{DevManagerAction::NO_TRADE};
    DevOOSExecutionResult execution{};
    DevManagerState next_state{};
    bool terminal{false};
};
struct ProductionTransition {
    DevManagerTransition manager{};
    LedgerRewardTransition reward_audit{};
};
class DevProductionTransitionIntegration final {
public:
    [[nodiscard]] static ProductionTransition build(
        const ProductionExecution& event, LedgerRewardMode mode,
        double reference_capital = 7000.0);
    [[nodiscard]] static std::vector<ProductionTransition> buildSequence(
        const std::vector<ProductionExecution>& events, LedgerRewardMode mode,
        double reference_capital = 7000.0);
};
} // namespace devai::strategy::d9f4c
