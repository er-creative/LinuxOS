#include "devai/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai::strategy;
namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error("STEP5_AUDIT_FAIL " + message);
}
void near(double got, double expected, double tol, const std::string& message) {
    if (std::abs(got - expected) > tol) {
        throw std::runtime_error("STEP5_AUDIT_FAIL " + message +
                                 " got=" + std::to_string(got) +
                                 " expected=" + std::to_string(expected));
    }
}
DevD9BObservation obs(int minute, double close, double forecast = 0.00002) {
    DevD9BObservation o;
    o.symbol = "TEST";
    o.decision_key = 100000 * 1440LL + minute;
    o.close = close;
    o.predicted_forward_return = forecast;
    o.zscore_1m = 0.3;
    o.zscore_5m = 0.5;
    o.zscore_15m = 0.1;
    o.forecast_ready = true;
    return o;
}
DevD9BConfig config(double cost_bps = 20.0) {
    DevD9BConfig c;
    c.permissions.enable_long = true;
    c.permissions.enable_short = false;
    c.round_trip_cost_bps = cost_bps;
    c.epsilon = 0.0;
    c.q_alpha = 1.0;
    c.q_gamma = 0.0;
    return c;
}
void reward_and_cost_test() {
    DevD9BEnvironment env(config());
    const auto start = env.step(obs(555, 100.0), DevD9AAction::OPEN_LONG);
    near(start.reward, -0.001, 1e-12, "entry must pay 10bps");
    near(start.total_cost, 0.001, 1e-12, "entry cost");
    require(start.position_after == DevD9APosition::LONG, "position after open");
    const auto hold = env.step(obs(556, 101.0), DevD9AAction::HOLD_LONG);
    near(hold.reward, 0.01, 1e-12, "held minute return");
    near(hold.total_cost, 0.0, 1e-12, "hold cost");
    const auto close = env.step(obs(557, 102.0), DevD9AAction::CLOSE_LONG);
    near(close.reward, 102.0 / 101.0 - 1.0 - 0.001, 1e-12,
         "closing minute return and exit cost");
    require(close.position_after == DevD9APosition::FLAT, "position after close");
    const auto flat = env.step(obs(558, 120.0), DevD9AAction::HOLD_FLAT);
    near(flat.reward, 0.0, 1e-12, "flat cannot earn future price movement");
    std::cout << "STEP5_REWARD_TIMING PASS entry_cost_bps=10 exit_cost_bps=10 "
                 "held_price_change_before_current_action=yes\n";
}
void no_instant_profit_test() {
    DevD9BEnvironment env(config());
    (void)env.step(obs(555, 100.0), DevD9AAction::HOLD_FLAT);
    const auto opened = env.step(obs(556, 110.0), DevD9AAction::OPEN_LONG);
    near(opened.reward, -0.001, 1e-12,
         "open at current close cannot earn preceding minute return");
    const auto held = env.step(obs(557, 111.0), DevD9AAction::HOLD_LONG);
    near(held.reward, 111.0 / 110.0 - 1.0, 1e-12,
         "first earned minute occurs after open");
    std::cout << "STEP5_NO_LOOKAHEAD PASS current_close_open_has_no_prior_return=yes\n";
}
void force_exit_test() {
    auto c = config();
    c.permissions.new_entry_cutoff_minutes = 915;
    DevD9BEnvironment env(c);
    (void)env.step(obs(914, 100.0), DevD9AAction::OPEN_LONG);
    const auto legal = env.legalActions(obs(915, 101.0));
    require(legal.size() == 1 && legal.front() == DevD9AAction::FORCE_EXIT,
            "15:15 must force exit");
    const auto end = env.step(obs(915, 101.0), DevD9AAction::FORCE_EXIT);
    require(end.terminal && end.position_after == DevD9APosition::FLAT,
            "mandatory terminal flat");
    near(end.reward, 0.01 - 0.001, 1e-12,
         "terminal exit must include last minute return and cost");
    std::cout << "STEP5_FORCE_EXIT PASS cutoff=15:15 terminal_flat=yes\n";
}
void transition_attribution_test() {
    // With alpha=1, gamma=0, a Q update equals the supplied immediate reward.
    // This tests the learner's contract; the training runner's Pending logic
    // must still be reviewed separately against this contract.
    auto c = config();
    DevD9BSharedQModel q(c);
    const auto s0 = obs(555, 100.0);
    const auto s1 = obs(556, 101.0);
    const std::vector<DevD9AAction> next_legal{DevD9AAction::HOLD_LONG,
                                                DevD9AAction::CLOSE_LONG};
    q.learn(s0, DevD9APosition::FLAT, DevD9AAction::OPEN_LONG,
            -0.001 + 0.01, s1, DevD9APosition::LONG, next_legal, false);
    const std::vector<DevD9AAction> flat_legal{DevD9AAction::HOLD_FLAT,
                                                DevD9AAction::OPEN_LONG};
    const auto selected = q.choose(s0, DevD9APosition::FLAT, flat_legal, false);
    require(selected == DevD9AAction::OPEN_LONG,
            "positive Q value must prefer open over zero-valued hold");
    std::cout << "STEP5_Q_LEARNER_CONTRACT PASS positive_open_reward_selects_open=yes\n";
}
void state_resolution_test() {
    auto c = config();
    DevD9BSharedQModel q(c);
    auto a = obs(555, 100.0, -0.00002);
    auto b = obs(555, 100.0,  0.00002);
    auto next = obs(556, 100.0);
    q.learn(a, DevD9APosition::FLAT, DevD9AAction::OPEN_LONG,
            0.01, next, DevD9APosition::LONG,
            {DevD9AAction::HOLD_LONG}, true);
    const auto choose_b = q.choose(b, DevD9APosition::FLAT,
                                   {DevD9AAction::HOLD_FLAT,
                                    DevD9AAction::OPEN_LONG}, false);
    // Both forecasts are in the 0 bucket with the current 20bps scale.
    require(choose_b == DevD9AAction::OPEN_LONG,
            "expected same Q-state for -0.2bps and +0.2bps forecasts");
    std::cout << "STEP5_FORECAST_BUCKET_ALIAS PASS "
                 "negative_and_positive_small_forecasts_share_state=yes "
                 "status=WARNING_INFORMATION_LOSS\n";
}
void cost_sensitivity_test() {
    for (double cost : {0.0, 5.0, 10.0, 20.0, 30.0}) {
        DevD9BEnvironment env(config(cost));
        const auto enter = env.step(obs(555, 100.0), DevD9AAction::OPEN_LONG);
        const auto exit = env.step(obs(556, 100.0), DevD9AAction::CLOSE_LONG);
        near(enter.reward + exit.reward, -cost / 10000.0, 1e-12,
             "flat-price round-trip cost sensitivity");
        std::cout << "STEP5_COST_SENSITIVITY round_trip_bps=" << cost
                  << " net_fraction=" << enter.reward + exit.reward << '\n';
    }
}
} // namespace
int main() {
    try {
        reward_and_cost_test();
        no_instant_profit_test();
        force_exit_test();
        transition_attribution_test();
        state_resolution_test();
        cost_sensitivity_test();
        std::cout << "STEP5_AUDIT_TESTS_PASS all=6 "
                     "runner_transition_attribution=NOT_YET_VERIFIED "
                     "forecast_horizon=NOT_YET_VERIFIED\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
