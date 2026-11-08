#include "devai/strategy/dev_ai_strategy/reward/DevHistoricalReplayIntegration_D9F4C.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai::strategy;
using namespace devai::strategy::d9f4c;
namespace {
void require(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
template <class F> void mustReject(F&& fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "expected rejection");
}
ReplayDecision decision(std::int64_t t, std::size_t action,
                        const std::string& symbol, bool terminal = false) {
    ReplayDecision d{};
    d.decision_time = t;
    d.manager_action = action;
    d.desired_symbol = symbol;
    d.terminal = terminal;
    d.legal_actions = {true, true, true, true, true, true, true};
    return d;
}
}
int main() {
    constexpr std::int64_t day = 20000LL * 86400 - 19800;
    const auto open = day + 9 * 3600 + 30 * 60;
    const auto hold = open + 60;
    const auto rotate = hold + 60;
    const auto close = day + 15 * 3600 + 15 * 60;
    DevOOSLedgerConfig config{};
    config.initial_capital = 7000.0;
    config.max_allocation_fraction = 0.75;
    config.brokerage_rate = 0.001;
    config.slippage_rate = 0.001;
    config.other_cost_rate = 0.0005;
    config.allow_intraday_short = false;
    const DevOOSPriceProvider prices = [=](const std::string& symbol, std::int64_t t) {
        if (symbol == "AAA") return t >= rotate ? 102.0 : 100.0;
        if (symbol == "BBB") return t >= close ? 202.0 : 200.0;
        throw std::invalid_argument("unexpected symbol");
    };
    const std::vector<ReplayDecision> decisions = {
        decision(open, 2, "AAA"),
        decision(hold, 1, "AAA"),
        decision(rotate, 3, "BBB"),
        decision(close, 0, "", true)
    };
    const auto replay = DevRealLedgerReplay::run(decisions, prices, config);
    const auto audit = DevHistoricalReplayIntegration::validate(replay, config.initial_capital);
    require(audit.events == 4 && audit.entries == 1 && audit.holds == 1 &&
            audit.switches == 1 && audit.exits == 1 && audit.terminal_flat,
            "incorrect episode classification");
    require(std::abs(audit.ending_equity - replay.final_portfolio.equity) < 1e-8,
            "ending equity mismatch");
    auto corrupted = replay;
    corrupted.steps[2].execution.net_pnl += 1.0;
    mustReject([&] { (void)DevHistoricalReplayIntegration::validate(corrupted); });
    corrupted = replay;
    corrupted.steps[1].execution.equity_before += 1.0;
    mustReject([&] { (void)DevHistoricalReplayIntegration::validate(corrupted); });
    corrupted = replay;
    corrupted.steps.back().decision.terminal = false;
    mustReject([&] { (void)DevHistoricalReplayIntegration::validate(corrupted); });
    mustReject([&] { (void)DevHistoricalReplayIntegration::validate({}, 7000.0); });
    mustReject([&] { (void)DevHistoricalReplayIntegration::validate(replay, 0.0); });
    std::cout << "D9F4C_HISTORICAL_REPLAY_INTEGRATION_PASS events=4 entry=1 hold=1 switch=1 exit=1 reconciliation=1 terminal=1 model_trained=0 oos_evaluated=0\n";
}
