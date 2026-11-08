#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace devai::strategy {
// 5C-3I-B: checks that full-universe Phase-4 ranking and the existing
// supervisor-accounted historical replay describe the SAME decisions.
// It does not fabricate model inputs, ranking scores, rewards, or orders.
struct DevRLHistoricalTrainingReadinessReport {
    bool ready{false};
    std::size_t decisions{0};
    std::size_t candidate_assignments{0};
    std::vector<std::string> errors;
};
class DevRLHistoricalTrainingReadiness {
public:
    // `ranking` must come from the existing Phase-4 causal ranking producer.
    // `history` must come from DevHistoricalExperienceIntegration::run.
    // held_symbols_before: optional, but REQUIRED if verifying that a position
    // outside the top five is retained in the manager's state. An empty symbol
    // means the manager is flat. This validator cannot independently prove
    // that a forecast did not use future candles.
    [[nodiscard]] static DevRLHistoricalTrainingReadinessReport validate(
        const std::vector<DevUniverseRankingTick>& ranking,
        const std::vector<std::string>& universe,
        const std::vector<DevHistoricalExperience>& history,
        std::int64_t training_end_exclusive,
        const std::vector<std::string>& held_symbols_before = {});
};
} // namespace devai::strategy
