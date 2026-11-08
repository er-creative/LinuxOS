#pragma once
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include "devai/strategy/dev_ai_strategy/reward/DevLedgerRewardBridge_D9F4C.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy::d9f4c {
// Manager-authorized decisions only. The replay does not select candidates.
struct ReplayDecision {
    std::int64_t decision_time{}; // actual execution/mark timestamp (UTC epoch seconds)
    std::size_t manager_action{}; // 0=flat/exit, 1=keep, 2..6=five LONG candidate slots
    std::array<bool, DEV_MANAGER_ACTION_COUNT> legal_actions{};
    std::string desired_symbol;   // empty=flat; long ticker only
    bool terminal{};
};
struct ReplayStep {
    ReplayDecision decision{};
    DevOOSExecutionResult execution{};
    DevOOSLedgerState portfolio{};
    LedgerRewardTransition gross{};
    LedgerRewardTransition net{};
};
struct ReplayResult {
    std::vector<ReplayStep> steps;
    DevOOSLedgerState final_portfolio{};
};
class DevRealLedgerReplay final {
public:
    // Input prices must be last COMPLETED historical candles at each event time.
    // This class cannot independently certify price-provider causality.
    [[nodiscard]] static ReplayResult run(
        const std::vector<ReplayDecision>& decisions,
        const DevOOSPriceProvider& prices,
        DevOOSLedgerConfig config = {});
};
} // namespace devai::strategy::d9f4c
