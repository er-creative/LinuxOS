#include "devai/strategy/dev_ai_strategy/DevHistoricalManagerStrategyAdapter.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai;
using namespace devai::strategy;

int main() {
    DevConfigurableManagerSpec spec;
    spec.manager_id = "ExampleManager";
    spec.strategy = DevStrategyKind::MathematicalStatistical;
    spec.universe = {"AAA","BBB","CCC","DDD"};
    spec.top_k = 2;
    spec.model_id = "ExampleFrozenModel";
    spec.interval_seconds = 300;

    int ranking_calls = 0, model_calls = 0;
    DevHistoricalRankingProvider provider = [&](const auto& config, auto time) {
        ++ranking_calls;
        DevUniverseRankingTick tick;
        tick.decision_time = time;
        for (std::size_t i = 0; i < config.universe.size(); ++i) {
            DevUniverseRankedShare row{};
            row.symbol = config.universe[i];
            row.score = static_cast<double>(i);
            tick.universe.push_back(row);
        }
        return tick;
    };
    // TEST-ONLY validator. Production must check actual observation source times.
    DevObservationCausalityValidator test_validator =
        [](const auto&, auto) { return true; };
    DevAgentInferenceCallback model = [&](const auto&, const auto& manager,
        const auto& agent, const auto& stock, auto) {
        ++model_calls;
        DevModelAgentReport r;
        r.manager_id = manager; r.agent_id = agent; r.symbol = stock.symbol;
        r.buy_probability = 0.6; r.sell_probability = 0.2;
        r.exit_probability = 0.2;
        return r;
    };
    DevHistoricalManagerStrategyAdapter adapter(spec,provider,test_validator,model);
    auto frames = adapter.replay({300,600,900});
    assert(frames.size() == 3);
    assert(ranking_calls == 3);
    assert(model_calls == 6);
    assert(frames.front().complete_universe.universe.size() == 4);
    assert(frames.front().candidate_frame.selected.size() == 2);
    assert(frames.front().candidate_frame.selected[0].symbol == "DDD");
    assert(frames.front().candidate_frame.reports.size() == 2);
    bool rejected = false;
    try { (void)adapter.replay({300,300}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    DevHistoricalManagerStrategyAdapter invalid(spec,provider,
        [](const auto&, auto) { return false; },model);
    rejected = false;
    try { (void)invalid.evaluate(300); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    std::cout << "PASS 5C-3I-C historical manager ranking/agent adapter contract\\n";
}
