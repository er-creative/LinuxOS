#pragma once
#include "devai/strategy/dev_ai_strategy/DevSharedMultiManagerHistoricalReplay.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace devai::strategy {
struct DevRealMinuteBar {
    std::int64_t start_epoch{0}; // UTC epoch, candle begins at this instant
    double open{0}, high{0}, low{0}, close{0}, volume{0};
};
class DevRealMinuteMarket {
public:
    void load(const std::string& symbol, const std::filesystem::path& path);
    // Strictly completed before or at decision boundary: start + 60 <= decision.
    [[nodiscard]] double completedClose(const std::string& symbol, std::int64_t decision) const;
    // First candle beginning at/after decision. This is a FUTURE execution event;
    // never use this value for model features or inference at decision time.
    [[nodiscard]] DevRealMinuteBar nextExecutionBar(const std::string& symbol,
        std::int64_t decision) const;
    [[nodiscard]] std::size_t barCount(const std::string& symbol) const;
private:
    std::map<std::string,std::vector<DevRealMinuteBar>> bars_;
};

// Historical cache contains only causal observations. Offline forward outcomes
// are deliberately discarded. One dataset per manager allows distinct stocks.
[[nodiscard]] std::vector<DevReplayTick> makeRealMultiManagerTicks(
    const std::map<std::string,DevHistoricalTrainingDataset>& per_manager,
    std::int64_t start_epoch, std::int64_t end_epoch);

struct DevRealReplayResult {
    DevReplayResult replay;
    std::size_t input_ticks{0};
    std::size_t execution_bar_checks{0};
};

// Model inference at boundary t, BUY fill at next available 1m bar OPEN.
// Unlike the earlier replay's completed-close fills, this class does not fill
// an order at a price observed before the decision. This is a historical
// simulated market-order model, not an actual exchange execution guarantee.
class DevRealMarketMultiManagerReplay {
public:
    [[nodiscard]] DevRealReplayResult run(
        const std::vector<DevReplayTick>& ticks,
        DevSharedModelMultiManagerRuntime& runtime,
        DevinderjeetSupervisor& supervisor,
        const DevRealMinuteMarket& market,
        double allocation_fraction = 0.50) const;
};
} // namespace devai::strategy
