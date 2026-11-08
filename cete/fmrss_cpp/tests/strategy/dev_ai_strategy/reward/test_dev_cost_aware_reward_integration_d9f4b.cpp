#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareRewardIntegration_D9F4B.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace devai;
    using namespace devai::strategy;
    using namespace devai::d9f4b;
    DevPortfolioConfig config{};
    config.initial_cash = 7000.0;
    config.max_exposure_fraction = .75;
    config.allow_short = false;
    DevPortfolioEnvironment env(config);
    DevPortfolioFrame frame{};
    frame.time = 100000;
    frame.quotes = {{"TEST",100.0,101.0}};
    auto opened = env.step(frame,{"DevAgent_001","TEST",DevPortfolioAction::LONG});
    assert(opened.executed);
    assert(opened.after.position.has_value());
    auto r1 = DevCostAwareRewardIntegration::fromPortfolioStep(opened,config.initial_cash);
    assert(std::abs(r1.reward-opened.reward)<1e-12);
    frame.time += 60;
    frame.quotes = {{"TEST",101.0,102.0}};
    auto held = env.step(frame,{"DevAgent_001","TEST",DevPortfolioAction::HOLD});
    assert(!held.executed);
    auto r2 = DevCostAwareRewardIntegration::fromPortfolioStep(held,config.initial_cash);
    frame.time += 60;
    frame.quotes = {{"TEST",102.0,102.0}};
    auto exited = env.step(frame,{"DevAgent_001","TEST",DevPortfolioAction::EXIT});
    assert(exited.executed);
    auto r3 = DevCostAwareRewardIntegration::fromPortfolioStep(exited,config.initial_cash);
    assert(std::abs(r1.reward+r2.reward+r3.reward-
        (exited.after.equity-7000.0)/7000.0)<1e-10);
    DevHistoricalExperience sample{};
    sample.decision_time = 100000;
    sample.next_decision_time = 100060;
    sample.equity_before = 7000.0;
    sample.equity_after = 6997.0;
    sample.portfolio_reward = -3.0/7000.0;
    assert(std::abs(DevCostAwareRewardIntegration::fromHistorical(sample,7000.0).reward - sample.portfolio_reward)<1e-12);
    sample.portfolio_reward=0;
    bool rejected=false;
    try { (void)DevCostAwareRewardIntegration::fromHistorical(sample,7000.0); }
    catch (const std::logic_error&) { rejected=true; }
    assert(rejected);
    std::cout << "D9F4B_PRODUCTION_ADAPTER_PASS portfolio_open_hold_exit=1 historical_reconciliation=1 mismatch_guard=1 cumulative_reward=1\n";
    std::cout << "D9F4B_CONTRACT_PASS production_policy_modified=0 model_trained=0 oos_evaluated=0\n";
}
