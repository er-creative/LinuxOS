#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
namespace devai::strategy {
struct DevRealHistoricalTrainingConfig {
    std::filesystem::path cache_file;
    std::filesystem::path minute_folder;
    std::filesystem::path checkpoint_file;
    std::size_t epochs{3};
    double learning_rate{0.0001};
    double train_fraction{0.8};
    double initial_capital{7000.0};
    double allocation_fraction{0.75};
    double brokerage_rate{0.0003};
    double slippage_rate{0.0005};
    double other_cost_rate{0.0003};
    std::uint32_t seed{73117U};
};
struct DevRealHistoricalTrainingResult {
    std::size_t records{0}, train_records{0}, agent_samples{0}, manager_samples{0};
    double training_loss{0.0}, validation_loss{0.0};
    double behavior_final_equity{0.0};
};
class DevRealHistoricalTrainingDriver {
public:
    explicit DevRealHistoricalTrainingDriver(DevRealHistoricalTrainingConfig config);
    [[nodiscard]] DevRealHistoricalTrainingResult run() const;
private:
    DevRealHistoricalTrainingConfig config_;
};
}
