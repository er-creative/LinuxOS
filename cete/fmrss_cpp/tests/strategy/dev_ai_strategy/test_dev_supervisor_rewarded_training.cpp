#include "devai/strategy/dev_ai_strategy/DevSupervisorRewardedTraining.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    DevRewardedTrainingSample s;
    s.historical.decision_time = 100;
    s.historical.next_decision_time = 200;
    s.historical.equity_before = 100000;
    s.historical.equity_after = 100010;
    s.historical.portfolio_reward = 0.0001;
    s.manager.decision_time = 100;
    s.manager.next_decision_time = 200;
    s.manager.reward = 0.0001;
    s.agents.push_back(DevTransition{});
    s.agents.front().decision_time = 100;
    s.agents.front().next_decision_time = 200;
    auto expect_reject = [](const std::vector<DevRewardedTrainingSample>& samples) {
        bool rejected = false;
        try { DevSupervisorRewardedTraining::validate(samples, 1000); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    };
    auto x=s; x.manager_id="DevManager_002"; expect_reject({x});
    x=s; x.manager.reward=0.2; expect_reject({x});
    x=s; x.historical.next_decision_time=1000; expect_reject({x});
    x=s; x.agents.clear(); expect_reject({x});
    x=s; x.agents.front().next_decision_time=300; expect_reject({x});
    x=s; x.manager_reward_from_supervisor=false; expect_reject({x});
    std::cout << "PASS supervisor rewarded training guardrails: single manager, reward provenance, chronological split, agent attribution\n";
}
