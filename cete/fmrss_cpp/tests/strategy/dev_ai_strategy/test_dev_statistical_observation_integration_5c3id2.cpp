#include "devai/strategy/dev_ai_strategy/DevStatisticalObservationIntegration.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai;
using namespace devai::strategy;
int main() {
    DevConfigurableManagerSpec spec;
    spec.manager_id="TestManager"; spec.model_id="TestModel";
    spec.universe={"AAA","BBB","CCC"}; spec.top_k=2;
    int inference_calls=0;
    DevSnapshotProvider provider=[](const auto& symbols, std::int64_t time) {
        devai::ranking::CrossSectionalUniverseSnapshot snap;
        snap.decision_time=time; snap.benchmark_symbol="NIFTY%2050";
        snap.numerical_integrity=true;
        for (const auto& s : symbols) {
            devai::ranking::CrossSectionalMathematicalState stock;
            stock.symbol=s; stock.benchmark_symbol=snap.benchmark_symbol;
            stock.decision_time=time; stock.numerical_integrity=true;
            auto& rank=stock.five_minute.rankings.return_z_score;
            rank.available=true; rank.centered_percentile=(s=="AAA"?0.8:(s=="BBB"?0.4:-0.2));
            snap.stocks.push_back(stock);
        }
        return snap;
    };
    // Synthetic-data test only: production must validate actual candle completion.
    DevSnapshotCausalityCheck test_only_causal=[](const auto&, auto) { return true; };
    DevStatisticalObservationIntegration integration(spec, provider, test_only_causal,
        [](const auto&, auto) { return DevPortfolioState{}; },
        // Synthetic policy for test only, NOT a recommended trading signal.
        [](const DevAIObservation& obs) {
            return obs.market[marketIndex(DevTimeframe::FIVE_MINUTE,DevMetric::RETURN_Z_SCORE)];
        },
        [&](const auto&, const auto& manager, const auto& agent, const auto& stock, auto) {
            ++inference_calls;
            DevModelAgentReport r;
            r.manager_id=manager; r.agent_id=agent; r.symbol=stock.symbol;
            r.buy_probability=0.5; r.sell_probability=0.25; r.exit_probability=0.25;
            return r;
        });
    auto cycle=integration.evaluate(300);
    assert(cycle.observations.size()==3);
    assert(cycle.candidates.selected.size()==2);
    assert(cycle.candidates.selected[0].symbol=="AAA");
    assert(cycle.candidates.selected[1].symbol=="BBB");
    assert(inference_calls==2);
    auto obs=cycle.observations.at("AAA");
    assert(obs.availability[marketIndex(DevTimeframe::FIVE_MINUTE,DevMetric::RETURN_Z_SCORE)]==1.0);
    assert(obs.availability[marketIndex(DevTimeframe::ONE_MINUTE,DevMetric::RETURN_Z_SCORE)]==0.0);
    bool rejected=false;
    try { (void)integration.evaluate(301); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    std::cout << "PASS 5C-3I-D2 statistical observation integration contract\n";
}
