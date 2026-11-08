#pragma once
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalExecutionRiskGate_H47_6C1G2B1.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace devai::strategy {
// Price at t is the last COMPLETED candle close (candle start < decision t).
// Input rows: Symbol,Date,Time,Open,High,Low,Close,Volume.
class DevHistoricalCloseStore {
public:
    void loadMinuteFile(const std::filesystem::path& file, const std::string& symbol);
    void loadMinuteFolder(const std::filesystem::path& folder,
                          const std::vector<std::string>& symbols);
    [[nodiscard]] double openingPrice(const std::string& symbol,
                                      std::int64_t decision_time) const;
    [[nodiscard]] double completedClose(const std::string& symbol,
                                        std::int64_t decision_time) const;
private:
    std::map<std::string, std::map<std::int64_t, double>> prices_;
    std::map<std::string, std::map<std::int64_t, double>> opens_;
};

// Inject a real, frozen DevManager inference adapter. It receives only current
// candidate observations and objective ledger state; never offline outcomes.
// Empty return => cash. Current symbol => keep. Other symbol => rotate.
using DevFrozenManagerPolicy = std::function<std::string(
    const DevOOSDecisionPoint&, const DevOOSLedgerState&)>;

struct DevHistoricalOOSStep {
    std::int64_t decision_time{0};
    std::string previous_symbol;
    std::string desired_symbol;
    std::string held_symbol;
    std::int64_t quantity{0};
    double cash{0.0};
    DevOOSExecutionResult execution{};
};
struct DevHistoricalOOSReport {
    std::vector<DevHistoricalOOSStep> steps;
    double initial_equity{0.0};
    double final_equity{0.0};
    double net_pnl{0.0};
    double net_return{0.0};
    double max_drawdown{0.0};
    std::size_t rotations{0};
    std::size_t executed_legs{0};
};
class DevHistoricalOOSRunner {
public:
    explicit DevHistoricalOOSRunner(DevOOSLedgerConfig config = {});
    DevHistoricalOOSRunner(DevOOSLedgerConfig config,
        DevHistoricalRiskLimits_H47_6C1G2B1 risk_limits);
    [[nodiscard]] DevHistoricalOOSReport run(
        const DevHistoricalTrainingDataset& dataset,
        const DevOOSPriceProvider& prices,
        const DevFrozenManagerPolicy& policy) const;
    static void writeCsv(const std::filesystem::path& path,
                         const DevHistoricalOOSReport& report);
private:
    DevOOSLedgerConfig config_;
    DevHistoricalRiskLimits_H47_6C1G2B1 risk_limits_{};
};
}
