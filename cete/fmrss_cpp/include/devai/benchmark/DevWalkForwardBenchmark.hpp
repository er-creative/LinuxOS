#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace devai::benchmark {
// The input is an independently generated, chronological out-of-sample decision ledger.
// A candidate's predictions MUST have been generated without access to future prices.
struct WFDecision {
    std::string candidate, manager, symbol;
    std::int64_t timestamp{};  // YYYYMMDDHHMM in IST, comparable as integer
    double price{}, score{};
    int action{};              // 0 WAIT, 1 BUY/SHORT, 2 HOLD, 3 EXIT
    bool legal{};
};
struct WFConfig {
    double initial_capital{7000.0};
    double allocation_fraction{0.75};
    double fee_per_order{20.0}; // illustrative; replace with audited CETE charge model
    double slippage_bps{0.0};
    std::size_t min_test_days{2};
};
struct WFReport {
    std::string candidate, manager;
    std::size_t rows{}, trades{}, rejected{}, test_days{};
    double starting_equity{}, ending_equity{}, net_pnl{}, max_drawdown_pct{};
    double win_rate_pct{}, profit_factor{};
};
std::vector<WFDecision> readWalkForwardCSV(const std::filesystem::path&);
std::vector<WFReport> evaluateWalkForward(const std::vector<WFDecision>&, const WFConfig&);
void writeWalkForwardCSV(const std::filesystem::path&, const std::vector<WFReport>&);
}
