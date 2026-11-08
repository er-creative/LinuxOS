#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.hpp"
#include <cmath>
#include <stdexcept>
namespace devai::d9f4b {
namespace {
void finite(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("Non-finite reward input");
}
}
RewardResult DevCostAwareRewardEngine::evaluate(const RewardInput& in) {
    finite(in.equity_before);
    finite(in.equity_after);
    finite(in.reference_capital);
    finite(in.transaction_cost);
    if (in.equity_before < 0 || in.equity_after < 0 || in.reference_capital <= 0 || in.transaction_cost < 0)
        throw std::invalid_argument("Invalid portfolio equity, capital, or transaction cost");
    if (!in.executed && in.transaction_cost != 0)
        throw std::invalid_argument("Unexecuted action cannot incur execution cost");
    if ((in.action == Action::WAIT || in.action == Action::HOLD) && in.executed)
        throw std::invalid_argument("WAIT and HOLD must not report executions");
    const double net = in.equity_after - in.equity_before;
    const double gross = net + in.transaction_cost;
    const double reward = net / in.reference_capital;
    finite(gross);
    finite(reward);
    return RewardResult{reward, net, gross, in.transaction_cost};
}
}
