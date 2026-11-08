#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace devai {

// A strategy is assigned to each manager by DevAISupervisor.
enum class DevStrategyKind { MathematicalStatistical, PriceAction, Custom };

struct DevConfigurableManagerSpec {
    std::string manager_id;
    DevStrategyKind strategy = DevStrategyKind::MathematicalStatistical;
    std::vector<std::string> universe;
    std::size_t top_k = 0;
    std::string model_id;
    std::int64_t interval_seconds = 300;
};

struct DevRankedStock {
    std::string symbol;
    double ranking_score = 0.0; // Strategy's established ranking metric
    std::int64_t latest_observation_time = 0; // Unix seconds; must be causal
};

struct DevModelAgentReport {
    std::string manager_id;
    std::string agent_id;
    std::string symbol;
    double buy_probability = 0.0;
    double sell_probability = 0.0;
    double exit_probability = 0.0;
};

struct DevManagerCandidateFrame {
    std::string manager_id;
    DevStrategyKind strategy = DevStrategyKind::MathematicalStatistical;
    std::int64_t decision_time = 0;
    std::vector<DevRankedStock> selected;
    std::vector<DevModelAgentReport> reports;
};

// This callback MUST invoke the manager's configured real Phase-4 strategy.
// It evaluates every symbol in the assigned universe; output must be one row per symbol.
using DevStrategyRankingCallback = std::function<std::vector<DevRankedStock>(
    DevStrategyKind, const std::vector<std::string>&, std::int64_t)>;

// One model_id per manager; each of its K agents uses that same model.
using DevAgentInferenceCallback = std::function<DevModelAgentReport(
    const std::string& model_id, const std::string& manager_id,
    const std::string& agent_id, const DevRankedStock&, std::int64_t)>;

// Called BY DevManager, not a replacement DevManager or supervisor.
// No execution, no RL training, no future-outcome labels in inference.
class DevConfigurablePhase4Integration final {
public:
    DevConfigurablePhase4Integration(DevConfigurableManagerSpec spec,
                                     DevStrategyRankingCallback strategy,
                                     DevAgentInferenceCallback model);
    DevManagerCandidateFrame evaluate(std::int64_t completed_boundary_time) const;
    const DevConfigurableManagerSpec& spec() const noexcept { return spec_; }
private:
    DevConfigurableManagerSpec spec_;
    DevStrategyRankingCallback strategy_;
    DevAgentInferenceCallback model_;
};

} // namespace devai
