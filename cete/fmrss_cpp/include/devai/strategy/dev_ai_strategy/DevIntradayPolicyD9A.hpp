#pragma once
#include <cstdint>
#include <string>

namespace devai::strategy {
enum class DevD9APosition { FLAT, LONG, SHORT };
enum class DevD9AAction { HOLD_FLAT, OPEN_LONG, HOLD_LONG, CLOSE_LONG,
                          OPEN_SHORT, HOLD_SHORT, CLOSE_SHORT, FORCE_EXIT };
struct DevD9ATradingConfig {
    bool enable_long{true};
    bool enable_short{false};
    bool intraday_only{true};
    bool allow_overnight{false};
    int new_entry_cutoff_minutes{15*60+15};
    int force_exit_minutes{15*60+15};
    double minimum_long_profit_probability{0.60};
    double minimum_short_profit_probability{0.60};
};
struct DevD9AInput {
    std::int64_t decision_key{0}; // completed exchange-local minute key
    DevD9APosition position{DevD9APosition::FLAT};
    double long_profit_probability{0.0};
    double short_profit_probability{0.0};
    double expected_long_net_return{0.0};
    double expected_short_net_return{0.0};
    bool stock_shortable{false};
    bool risk_allows_new_entry{true};
};
struct DevD9AResult {
    DevD9AAction action{DevD9AAction::HOLD_FLAT};
    std::string reason;
};
class DevIntradayPolicyD9A final {
public:
    explicit DevIntradayPolicyD9A(DevD9ATradingConfig config = {});
    [[nodiscard]] DevD9AResult evaluate(const DevD9AInput& input) const;
    [[nodiscard]] const DevD9ATradingConfig& config() const noexcept { return config_; }
private:
    DevD9ATradingConfig config_;
};
} // namespace devai::strategy
