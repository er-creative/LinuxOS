#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include "Dev5C3HTestFixture.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai::strategy;
namespace fs = std::filesystem;

namespace {
const std::vector<std::string> universe = {
    "ADANIPORTS", "AXISBANK", "BHARTIARTL", "HCLTECH", "HDFCBANK",
    "ICICIBANK", "ITC", "NTPC", "RELIANCE", "SBIN"
};
constexpr std::int64_t start = 1791517500;

struct TempMarket {
    fs::path dir;
    DevRealMinuteMarket market;

    TempMarket() : dir(fs::temp_directory_path() / "cete_5c3h3_market") {
        fs::create_directories(dir);
        for (const auto& sym : universe) {
            const auto path = dir / (sym + "_1min.txt");
            std::ofstream out(path);
            if (!out) throw std::runtime_error("cannot create test market file");
            out << "Symbol,Date,Time,Open,High,Low,Close,Volume\n";
            for (int minute = 15; minute <= 24; ++minute) {
                const double price = sym == "SBIN"
                    ? 50.0 + (minute - 15) : 100.0;
                out << sym << ",2026-10-09,09:" << minute << ":00,"
                    << price << "," << price << "," << price << ","
                    << price << ",100\n";
            }
            out.close();
            market.load(sym, path);
        }
    }
    ~TempMarket() { std::error_code ec; fs::remove_all(dir, ec); }
};

std::vector<DevUniverseRankingTick> makeRanking() {
    std::vector<DevUniverseRankingTick> result;
    for (int i = 1; i <= 5; ++i) {
        DevUniverseRankingTick tick{};
        tick.decision_time = start + i * 60;
        for (std::size_t j = 0; j < universe.size(); ++j) {
            DevUniverseRankedShare share{};
            share.symbol = universe[j];
            share.score = static_cast<double>(j);
            share.observation.symbol = share.symbol;
            share.observation.decision_time = tick.decision_time;
            share.observation.ready = true;
            share.observation.numerical_integrity = true;
            tick.universe.push_back(share);
        }
        result.push_back(tick);
    }
    return result;
}

enum class Scenario { AllHold, BuyThenHold, InvalidBuy };

DevHistoricalExperienceResult simulate(
    const TempMarket& data, Scenario scenario, const std::string& fixture_name,
    double cost_bps, double slip_bps) {

    DevSupervisorConfig cfg{};
    cfg.initial_capital = 100000.0;
    cfg.global_max_exposure_fraction = 1.0;
    cfg.max_open_positions = 1;
    cfg.transaction_cost_bps = cost_bps;
    cfg.slippage_bps = slip_bps;

    DevinderjeetSupervisor supervisor(cfg);
    supervisor.registerManager({
        "DevManager_001",
        {"DevAgent_001", "DevAgent_002", "DevAgent_003",
         "DevAgent_004", "DevAgent_005"},
        100000.0
    });

    cete_5c3h_test::Fixture fixture(
        fs::temp_directory_path() / fixture_name, supervisor);
    fixture.add("DevManager_001", universe);

    DevQNetwork agent;
    DevManagerQNetwork manager;
    DevSharedModelMultiManagerRuntime runtime(
        agent, manager, supervisor, fixture.execution);
    runtime.addManager(
        "DevManager_001", DevSingleManagerTop5Replay::agentIds());

    const auto policy = [scenario](
        const DevReplayTick& tick,
        DevSharedModelMultiManagerRuntime&) -> DevRotationIntent {

        if (tick.decision_time == start + 60) {
            if (scenario == Scenario::BuyThenHold)
                return {DevRotationAction::BUY, "SBIN", "DevAgent_001"};
            if (scenario == Scenario::InvalidBuy)
                return {DevRotationAction::BUY, "INVALID", "DevAgent_001"};
        }
        return {DevRotationAction::HOLD, {}, {}};
    };

    return DevHistoricalExperienceIntegration::run(
        makeRanking(), universe, runtime, supervisor,
        data.market, policy, 0.5, true);
}

void checkRewardIdentity(const DevHistoricalExperienceResult& result) {
    assert(result.experiences.size() == 5);
    const double initial = result.execution.replay.initial_equity;
    assert(initial > 0.0 && std::isfinite(initial));
    double accumulated = 0.0;
    for (std::size_t i = 0; i < result.experiences.size(); ++i) {
        const auto& e = result.experiences[i];
        assert(std::isfinite(e.portfolio_reward));
        assert(std::isfinite(e.equity_before));
        assert(std::isfinite(e.equity_after));
        assert(std::abs(e.portfolio_reward -
            (e.equity_after - e.equity_before) / initial) < 1e-12);
        assert(e.intent.action == (i == 0
            && result.execution.replay.filled > 0
            ? DevRotationAction::BUY : DevRotationAction::HOLD));
        accumulated += e.portfolio_reward;
    }
    assert(std::abs(accumulated -
        result.execution.replay.net_pnl / initial) < 1e-10);
    assert(result.experiences.back().terminal);
    assert(result.experiences.back().next_candidates.empty());
}

} // namespace

int main() {
    const TempMarket data;

    const auto free_flat = simulate(
        data, Scenario::AllHold, "cete_freeze_flat_zero", 0.0, 0.0);
    const auto costly_flat = simulate(
        data, Scenario::AllHold, "cete_freeze_flat_costly", 25.0, 25.0);

    assert(free_flat.execution.replay.filled == 0);
    assert(costly_flat.execution.replay.filled == 0);
    checkRewardIdentity(free_flat);
    checkRewardIdentity(costly_flat);
    for (const auto& e : costly_flat.experiences) {
        assert(e.intent.action == DevRotationAction::HOLD);
        assert(std::abs(e.portfolio_reward) < 1e-12);
    }
    assert(std::abs(costly_flat.execution.replay.net_pnl) < 1e-8);

    const auto free_trade = simulate(
        data, Scenario::BuyThenHold, "cete_freeze_trade_zero", 0.0, 0.0);
    const auto costly_trade = simulate(
        data, Scenario::BuyThenHold, "cete_freeze_trade_costly", 25.0, 25.0);
    const auto costly_repeat = simulate(
        data, Scenario::BuyThenHold, "cete_freeze_trade_repeat", 25.0, 25.0);

    assert(free_trade.execution.replay.filled == 2);
    assert(costly_trade.execution.replay.filled == 2);
    checkRewardIdentity(free_trade);
    checkRewardIdentity(costly_trade);
    checkRewardIdentity(costly_repeat);

    // Positive fees and slippage must reduce the same strategy's
    // net portfolio result compared with zero-cost replay.
    assert(costly_trade.execution.replay.net_pnl <
           free_trade.execution.replay.net_pnl);

    for (std::size_t i = 0; i < costly_trade.experiences.size(); ++i) {
        const auto& a = costly_trade.experiences[i];
        const auto& b = costly_repeat.experiences[i];
        assert(a.portfolio_reward == b.portfolio_reward);
        assert(a.equity_before == b.equity_before);
        assert(a.equity_after == b.equity_after);
    }

    bool nonzero_hold_reward = false;
    for (std::size_t i = 1; i < costly_trade.experiences.size(); ++i) {
        const auto& e = costly_trade.experiences[i];
        assert(e.intent.action == DevRotationAction::HOLD);
        if (std::abs(e.portfolio_reward) > 1e-12)
            nonzero_hold_reward = true;
    }
    assert(nonzero_hold_reward);

    bool rejected = false;
    try {
        (void)simulate(data, Scenario::InvalidBuy,
                       "cete_freeze_invalid", 25.0, 25.0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "PASS 5C-3H final freeze: nonzero costs/slippage, "
                 "flat HOLD, invested HOLD, reward reconciliation, "
                 "deterministic replay, invalid BUY\n";
}
