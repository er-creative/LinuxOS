#include "devai/strategy/dev_ai_strategy/DevManagerHistoricalStatisticalIntegration.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai;
using namespace devai::strategy;

int main() {
    DevConfigurableManagerSpec spec;
    spec.manager_id = "DevManager_001";
    spec.strategy = DevStrategyKind::MathematicalStatistical;
    spec.universe = {"AAA", "BBB", "CCC"};
    spec.top_k = 2;
    spec.model_id = "shared_frozen_model";
    spec.interval_seconds = 300;
    int provider_calls = 0, model_calls = 0;
    auto provider = [&](const std::vector<std::string>& symbols, std::int64_t time) {
        ++provider_calls;
        devai::ranking::CrossSectionalUniverseSnapshot snap;
        snap.decision_time = time;
        snap.benchmark_symbol = "NIFTY%2050";
        snap.numerical_integrity = true;
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            devai::ranking::CrossSectionalMathematicalState s;
            s.symbol = symbols[i];
            s.benchmark_symbol = snap.benchmark_symbol;
            s.decision_time = time;
            s.numerical_integrity = true;
            auto& rank = s.five_minute.rankings.return_z_score;
            rank.available = true;
            rank.centered_percentile = static_cast<double>(i) / 2.0;
            snap.stocks.push_back(s);
        }
        return snap;
    };
    // Test fixture only. Production validator MUST inspect actual source-candle
    // completion times and exclude future labels.
    auto validator = [](const devai::ranking::CrossSectionalUniverseSnapshot& snap,
                        std::int64_t time) { return snap.decision_time == time; };
    auto score = [](const DevAIObservation& obs) {
        return obs.market[marketIndex(DevTimeframe::FIVE_MINUTE, DevMetric::RETURN_Z_SCORE)];
    }; // TEST ORDERING ONLY; NOT a production BUY/SELL or scoring policy.
    auto inference = [&](const std::string& model, const std::string& manager,
                         const std::string& agent, const DevAIObservation& obs) {
        assert(model == "shared_frozen_model");
        assert(obs.ready);
        ++model_calls;
        DevModelAgentReport report;
        report.manager_id = manager;
        report.agent_id = agent;
        report.symbol = obs.symbol;
        report.buy_probability = 0.5;
        report.sell_probability = 0.25;
        report.exit_probability = 0.25;
        return report;
    };
    DevManagerHistoricalStatisticalIntegration driver(spec, provider, validator, score, inference);
    auto frames = driver.replay({300, 600});
    assert(frames.size() == 2 && provider_calls == 2 && model_calls == 4);
    for (const auto& frame : frames) {
        assert(frame.manager_id == "DevManager_001");
        assert(frame.selected.size() == 2 && frame.reports.size() == 2);
        assert(frame.selected[0].symbol == "CCC");
        assert(frame.selected[1].symbol == "BBB");
        assert(frame.reports[0].agent_id == "DevAgent_001");
    }
    bool rejected = false;
    try { (void)driver.replay({600, 300}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    std::cout << "PASS 5C-3I-D3 manager-driven historical statistical integration contract\n";
}
