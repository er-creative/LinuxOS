#pragma once
#include "devai/strategy/dev_ai_strategy/reward/DevRealLedgerReplay_D9F4C.hpp"
#include <cstddef>
#include <vector>

namespace devai::strategy::d9f4c {
struct HistoricalReplayAudit {
    std::size_t events{};
    std::size_t entries{};
    std::size_t holds{};
    std::size_t switches{};
    std::size_t exits{};
    double total_gross_pnl{};
    double total_net_pnl{};
    double total_execution_costs{};
    double ending_equity{};
    bool terminal_flat{};
};

// Audits actual replay executions. Does not generate policy decisions,
// train a model, write checkpoints, or claim causal historical data sourcing.
class DevHistoricalReplayIntegration final {
public:
    [[nodiscard]] static HistoricalReplayAudit validate(
        const ReplayResult& replay, double initial_capital = 7000.0);
};
} // namespace devai::strategy::d9f4c
