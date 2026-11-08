#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareReward_D9F4B.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace devai::d9f4b;
namespace {
bool near(double a, double b) { return std::abs(a - b) < 1e-10; }
void reject(const RewardInput& in) {
    bool thrown = false;
    try { (void)DevCostAwareRewardEngine::evaluate(in); }
    catch (const std::invalid_argument&) { thrown = true; }
    assert(thrown);
}
}
int main() {
    const auto entry = DevCostAwareRewardEngine::evaluate({Action::OPEN, 7000, 6995, 7000, 5, true});
    const auto hold = DevCostAwareRewardEngine::evaluate({Action::HOLD, 6995, 7025, 7000, 0, false});
    const auto exit = DevCostAwareRewardEngine::evaluate({Action::EXIT, 7025, 7021, 7000, 4, true});
    const auto wait = DevCostAwareRewardEngine::evaluate({Action::WAIT, 7000, 7000, 7000, 0, false});
    const auto sw = DevCostAwareRewardEngine::evaluate({Action::SWITCH, 7050, 7039, 7000, 11, true});
    assert(near(entry.reward, -5.0 / 7000));
    assert(near(entry.gross_equity_change, 0));
    assert(near(hold.reward, 30.0 / 7000));
    assert(near(exit.reward, -4.0 / 7000));
    assert(near(wait.reward, 0));
    assert(near(sw.reward, -11.0 / 7000));
    assert(near(sw.gross_equity_change, 0));
    assert(near(entry.reward + hold.reward + exit.reward, 21.0 / 7000));
    assert(DevCostAwareRewardEngine::evaluate({Action::HOLD, 7000, 6960, 7000, 0, false}).reward < 0);
    reject({Action::WAIT, 7000, 7000, 0, 0, false});
    reject({Action::WAIT, 7000, std::numeric_limits<double>::quiet_NaN(), 7000, 0, false});
    reject({Action::WAIT, 7000, 7000, 7000, 5, false});
    reject({Action::WAIT, 7000, 7000, 7000, 0, true});
    std::cout << "D9F4B_COST_AWARE_REWARD_PASS entry_cost=1 hold_mark_to_market=1 exit_cost=1 wait_guard=1 switch_cost=1 loss_guard=1 finite_guard=1 capital_guard=1 execution_guard=1 telescoping_reward=1\n";
    std::cout << "D9F4B_CONTRACT_PASS production_policy_modified=0 model_trained=0 oos_evaluated=0\n";
}
