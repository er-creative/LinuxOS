#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/reward/DevLedgerRewardBridge_D9F4C.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace devai::strategy::d9f4c {
// Read-only historical replay. No training, checkpoint writes or live orders.
struct CandleReplayConfig {
    std::filesystem::path cache_file;
    std::filesystem::path minute_folder;
    std::filesystem::path shared_checkpoint; // Required: no random untrained policy.
    double capital{7000.0};
    double allocation{0.75};
    double brokerage_rate{0.0003};
    double slippage_rate{0.0005};
    double other_cost_rate{0.0003};
    LedgerRewardMode mode{LedgerRewardMode::NET_EXECUTION_REWARD};
    // F3E-D1: zero preserves F3C policy and audit output.
    int minimum_holding_minutes{0};
};
struct CandleReplayStep {
    std::int64_t decision_time{}, next_decision_time{};
    DevManagerEncodedState state{}, next_state{};
    std::size_t action{};
    std::string desired, held_after;
    double equity_before{}, equity_after_mark{};
    LedgerRewardTransition reward{};
    bool terminal{};
};
struct CandleDecisionAudit {
    std::int64_t decision_time{};
    std::size_t raw_action{}, final_action{};
    std::array<double, DEV_MANAGER_ACTION_COUNT> q_values{};
    std::array<bool, DEV_MANAGER_ACTION_COUNT> legal_actions{};
    std::array<std::string, DEV_AGENT_SLOT_COUNT> candidate_symbols{};
    std::array<double, DEV_AGENT_SLOT_COUNT> candidate_scores{};
    std::string held_before, desired, held_after;
    std::string supervisor_priority, supervisor_recommendation;
    int holding_age_minutes{};
    bool stagnant{}, stop_triggered{};
    std::size_t executed_legs{};
};
struct CandleReplayReport {
    std::vector<CandleReplayStep> steps;
    std::vector<CandleDecisionAudit> audit;
    double final_equity{};
    std::size_t executed_legs{};
};
class DevCandleManagerReplay final {
public:
    [[nodiscard]] static CandleReplayReport run(const CandleReplayConfig& cfg);
    static void writeCsv(const std::filesystem::path& file, const CandleReplayReport& report);
    [[nodiscard]] static int minuteIST(std::int64_t unix_seconds) noexcept;
    [[nodiscard]] static double markedEquity(const DevOOSLedgerState& ledger,
                                              double held_completed_close);
};
}
