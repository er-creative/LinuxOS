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
    const TempMarket& data, Scenario scenario, const std::string& fixture_name) {

    DevSupervisorConfig cfg{};
    cfg.initial_capital = 100000.0;
    cfg.global_max_exposure_fraction = 1.0;
    cfg.max_open_positions = 1;
    cfg.transaction_cost_bps = 0;
    cfg.slippage_bps = 0;

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

    // Flat HOLD: no trades, no trading fees and no execution rejection penalty.
    const auto flat = simulate(data, Scenario::AllHold, "cete_5c3h3_flat");
    assert(flat.execution.replay.filled == 0);
    checkRewardIdentity(flat);
    for (const auto& e : flat.experiences) {
        assert(e.intent.action == DevRotationAction::HOLD);
        assert(std::abs(e.portfolio_reward) < 1e-12);
        assert(std::abs(e.equity_after - e.equity_before) < 1e-8);
    }

    // BUY followed by HOLD: mark-to-market can change reward without an
    // additional order. Terminal settlement is handled by the replay engine.
    const auto invested = simulate(
        data, Scenario::BuyThenHold, "cete_5c3h3_invested");
    assert(invested.execution.replay.filled == 2);
    checkRewardIdentity(invested);
    bool saw_hold_reward = false;
    for (std::size_t i = 1; i < invested.experiences.size(); ++i) {
        const auto& e = invested.experiences[i];
        assert(e.intent.action == DevRotationAction::HOLD);
        if (std::abs(e.portfolio_reward) > 1e-12)
            saw_hold_reward = true;
    }
    assert(saw_hold_reward);

    // Same historical inputs and policy must yield identical rewards.
    const auto repeated = simulate(
        data, Scenario::BuyThenHold, "cete_5c3h3_repeat");
    assert(repeated.experiences.size() == invested.experiences.size());
    for (std::size_t i = 0; i < invested.experiences.size(); ++i)
        assert(repeated.experiences[i].portfolio_reward ==
               invested.experiences[i].portfolio_reward);

    // A BUY target outside the ranked top five must be rejected rather
    // than becoming a fabricated execution or a training reward.
    bool invalid_rejected = false;
    try {
        (void)simulate(data, Scenario::InvalidBuy, "cete_5c3h3_invalid");
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    assert(invalid_rejected);

    std::cout
        << "PASS 5C-3H-3 reward integrity: flat HOLD, invested HOLD, "
        << "equity identity, deterministic replay, invalid BUY rejection\n";
}
