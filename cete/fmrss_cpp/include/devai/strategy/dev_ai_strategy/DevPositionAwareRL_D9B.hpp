#pragma once
#include "devai/strategy/dev_ai_strategy/DevIntradayPolicyD9A.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace devai::strategy {
// Research-only, one position per symbol. Each completed 1m observation contains
// only data known at its decision_key. All timestamps use CETE exchange-local keys.
struct DevD9BObservation {
    std::string symbol;
    std::int64_t decision_key{0};
    double close{0};
    double predicted_forward_return{0};
    double zscore_1m{0};
    double zscore_5m{0};
    double zscore_15m{0};
    bool forecast_ready{false};
    bool shortable{false};
};
struct DevD9BConfig {
    DevD9ATradingConfig permissions{};
    double round_trip_cost_bps{20.0}; // charged half on entry and half on exit
    double max_abs_one_minute_return{0.20}; // input quality bound, not a stop-loss
    double q_alpha{0.05};
    double q_gamma{0.95};
    double epsilon{0.10};
    unsigned seed{17};
    // Model observation mode: true = forecast-assisted; false = direct statistical.
    bool enable_predicted_forward_return{false}; // safe default; training runner sets explicitly
    // Absolute forecast quantiles calibrated exclusively on the training partition.
    double forecast_inner_abs{0.0};
    double forecast_outer_abs{0.0};
};
struct DevD9BPosition {
    DevD9APosition side{DevD9APosition::FLAT};
    double entry_price{0};
    std::int64_t entry_key{0};
};
struct DevD9BStep {
    DevD9AAction action{DevD9AAction::HOLD_FLAT};
    DevD9APosition position_after{DevD9APosition::FLAT};
    double reward{0}; // fractional net mark-to-market return on one unit notional
    double total_cost{0}; // fractional trading costs charged on this step
    bool terminal{false};
    std::string reason;
};
class DevD9BEnvironment final {
public:
    explicit DevD9BEnvironment(DevD9BConfig config = {});
    void reset();
    [[nodiscard]] DevD9BPosition position(const std::string& symbol) const;
    // First call for each symbol initializes the observation; subsequent calls
    // accrue prior position's one-minute return, then execute an action at current close.
    // When session changes, an unflattened prior position is an ERROR, not silently carried.
    [[nodiscard]] DevD9BStep step(const DevD9BObservation& obs, DevD9AAction requested);
    [[nodiscard]] std::vector<DevD9AAction> legalActions(const DevD9BObservation& obs) const;
    [[nodiscard]] const DevD9BConfig& config() const noexcept { return cfg_; }
private:
    struct State { DevD9BPosition position; double last_close{0}; std::int64_t last_key{0}; };
    DevD9BConfig cfg_;
    std::unordered_map<std::string, State> states_;
};

// ONE shared tabular Q model across every symbol; state includes position and
// quantized causal features, but not symbol ID or realized forward return.
class DevD9BSharedQModel final {
public:
    explicit DevD9BSharedQModel(DevD9BConfig config = {});
    [[nodiscard]] DevD9AAction choose(const DevD9BObservation& obs,
        DevD9APosition position, const std::vector<DevD9AAction>& legal,
        bool explore);
    void learn(const DevD9BObservation& obs, DevD9APosition position,
        DevD9AAction action, double reward,
        const DevD9BObservation& next, DevD9APosition next_position,
        const std::vector<DevD9AAction>& next_legal, bool terminal);
    [[nodiscard]] std::size_t stateCount() const noexcept { return q_.size(); }
    [[nodiscard]] std::string stateKey(const DevD9BObservation& obs, DevD9APosition position) const;
    [[nodiscard]] const DevD9BConfig& config() const noexcept { return cfg_; }
    void save(const std::string& path) const;
    void load(const std::string& path);
private:
    using Values = std::array<double, 8>;
    [[nodiscard]] std::string key(const DevD9BObservation&, DevD9APosition) const;
    [[nodiscard]] double value(const std::string&, DevD9AAction) const;
    DevD9BConfig cfg_;
    std::unordered_map<std::string, Values> q_;
    std::uint64_t rng_state_;
    [[nodiscard]] double random01();
};
} // namespace devai::strategy
