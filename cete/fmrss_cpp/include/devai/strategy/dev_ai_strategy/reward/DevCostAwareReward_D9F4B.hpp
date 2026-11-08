#pragma once
#include <cstdint>
namespace devai::d9f4b {
enum class Action : std::uint8_t { WAIT, OPEN, HOLD, EXIT, SWITCH };
struct RewardInput {
    Action action = Action::WAIT;
    double equity_before = 0.0;
    double equity_after = 0.0;
    double reference_capital = 7000.0;
    double transaction_cost = 0.0; // Already included in equity_after
    bool executed = false;
};
struct RewardResult {
    double reward = 0.0;
    double net_equity_change = 0.0;
    double gross_equity_change = 0.0;
    double transaction_cost = 0.0;
};
class DevCostAwareRewardEngine final {
public:
    [[nodiscard]] static RewardResult evaluate(const RewardInput& input);
};
}
