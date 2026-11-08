#pragma once
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace devai::strategy {
struct DevOutcomeRewardConfig {
    std::filesystem::path minute_data_folder;
    std::vector<std::size_t> horizons{5, 10, 15};
    double initial_capital{7000.0};
    double maximum_exposure_fraction{0.75};
    double transaction_cost_bps_per_side{5.0}; // illustrative, configure actual fees
    double slippage_bps_per_side{2.0};         // illustrative, configure actual slippage
    bool allow_fractional_shares{false};
};
struct DevOutcomeRewardStats {
    std::size_t records{0}, candidates{0}, available{0}, unavailable{0};
    std::size_t rejected_gaps{0}, rejected_session{0}, rejected_history{0};
};
class DevHistoricalOutcomeRewardValidator {
public:
    explicit DevHistoricalOutcomeRewardValidator(DevOutcomeRewardConfig config);
    // Updates OFFLINE targets only. Observations are not modified.
    DevOutcomeRewardStats enrich(DevHistoricalTrainingDataset& dataset,
                                 const std::filesystem::path& report_csv) const;
private:
    DevOutcomeRewardConfig config_;
};
} // namespace devai::strategy
