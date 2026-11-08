#include "devai/strategy/dev_ai_strategy/DevRealHistoricalTrainingDriver.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace devai::strategy {
namespace {
struct Sample { DevSharedInput input{}; std::size_t action{0}; double reward{0.0}; };
void ensure(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error("3.19D-3B: " + message);
}
double squared(const DevSharedNeuralNetwork& net, const Sample& s) {
    const auto q = net.predict(s.input);
    return 0.5 * (q[s.action] - s.reward) * (q[s.action] - s.reward);
}
// Manager's 62 causal inputs: agent Q summaries, candidate availability,
// position, equity, drawdown. No forward returns or future market prices.
DevManagerEncodedState managerState(const DevHistoricalDecisionRecord& record,
                                     const DevOOSLedgerState& ledger,
                                     const DevSharedNeuralNetwork& net) {
    DevManagerEncodedState m{};
    m.decision_time = record.decision_time;
    m.valid_actions[0] = true; // FLAT/EXIT
    m.valid_actions[1] = ledger.quantity != 0; // KEEP
    for (const auto& c : record.candidates) {
        ensure(c.slot_index < 5, "candidate slot outside 0..4");
        const auto q = net.predictAgent(c.observation);
        const auto base = c.slot_index * 8;
        for (std::size_t a=0; a<4; ++a)
            m.values[base+a] = std::clamp(q.values[a], -10.0, 10.0);
        m.values[base+4] = std::clamp(c.candidate_score / 100.0, -1.0, 1.0);
        m.values[base+5] = 1.0;
        m.values[base+6] = static_cast<double>(c.slot_index)/4.0;
        m.values[base+7] = 1.0;
        // Rank 10 is assessed by DevAgent_005, but is not executable in
        // the default LONG-only training configuration.
        m.valid_actions[2+c.slot_index] = c.slot_index < 4;
    }
    m.values[40] = ledger.quantity>0 ? 1.0 : ledger.quantity<0 ? -1.0 : 0.0;
    m.values[41] = (ledger.equity - 7000.0)/7000.0;
    m.values[42] = ledger.peak_equity>0 ?
        (ledger.peak_equity-ledger.equity)/ledger.peak_equity : 0.0;
    m.values[43] = static_cast<double>(record.candidates.size())/5.0;
    m.numerical_integrity = true;
    return m;
}
}
DevRealHistoricalTrainingDriver::DevRealHistoricalTrainingDriver(
    DevRealHistoricalTrainingConfig config):config_(std::move(config)) {}
DevRealHistoricalTrainingResult DevRealHistoricalTrainingDriver::run() const {
    const auto& cfg = config_;
    ensure(cfg.epochs>0 && cfg.epochs<=100, "epochs must be 1..100");
    ensure(std::isfinite(cfg.learning_rate) && cfg.learning_rate>0 && cfg.learning_rate<=0.01,
           "invalid learning rate");
    ensure(cfg.train_fraction>0.5 && cfg.train_fraction<1.0, "train_fraction must be (0.5,1)");
    auto data=DevHistoricalTrainingCache::load(cfg.cache_file);
    DevHistoricalTrainingDatasetValidator::validate(data);
    ensure(data.records.size()>=10, "need at least 10 historical cache records");
    std::set<std::string> symbols;
    for (const auto& r:data.records)
        for (const auto& c:r.candidates) symbols.insert(c.symbol);
    ensure(!symbols.empty(), "no historical candidate symbols");
    DevHistoricalCloseStore prices;
    for (const auto& symbol:symbols)
        prices.loadMinuteFile(cfg.minute_folder/(symbol+"_1min.txt"), symbol);
    DevSharedNeuralNetwork net(cfg.seed);
    DevOOSLedgerConfig ledger_config{};
    ledger_config.initial_capital=cfg.initial_capital;
    ledger_config.max_allocation_fraction=cfg.allocation_fraction;
    ledger_config.brokerage_rate=cfg.brokerage_rate;
    ledger_config.slippage_rate=cfg.slippage_rate;
    ledger_config.other_cost_rate=cfg.other_cost_rate;
    DevRealHistoricalMultiStockOOS ledger(ledger_config);
    const std::size_t boundary=static_cast<std::size_t>(data.records.size()*cfg.train_fraction);
    ensure(boundary>=2 && boundary+2<data.records.size(), "invalid chronological split");
    std::vector<Sample> agent_samples, manager_samples;
    std::size_t processed=0;
    // Training data is strictly before the split. Model is frozen while collecting
    // transitions, so reward from t+120 cannot influence the decision at t+60.
    for (std::size_t i=0;i+2<boundary;++i) {
        const auto& r=data.records[i];
        if (data.records[i+1].decision_time != r.decision_time+60 ||
            data.records[i+2].decision_time != r.decision_time+120) continue;
        const auto manager=managerState(r,ledger.state(),net);
        const auto manager_input=net.encodeManager(manager);
        // Deterministic behavior policy for coverage, NOT learned optimal policy.
        // Cycles through FLAT and eligible LONG candidates only.
        // SHORT candidates remain observable, but cannot be executed
        // while the ledger is configured with SHORT disabled.
        std::string desired;
        std::size_t chosen=0;
        std::vector<std::size_t> eligible_long;
        for (std::size_t j=0; j<r.candidates.size(); ++j) {
            if (r.candidates[j].slot_index < 4)
                eligible_long.push_back(j);
        }
        if (!eligible_long.empty() && (processed % 3) != 0) {
            const auto index = eligible_long[processed % eligible_long.size()];
            chosen = 2 + r.candidates[index].slot_index;
            desired = r.candidates[index].symbol;
        }
        const auto fill_time=r.decision_time+60;
        const auto mark_time=r.decision_time+120;
        const auto provider=[&](const std::string& symbol,std::int64_t t){
            return prices.completedClose(symbol,t);
        };
        // Verify market data before advancing the ledger; missing/stale candles
        // fail closed instead of silently generating fictitious rewards.
        if (!desired.empty()) {
            const auto& symbol=desired;
            (void)prices.completedClose(symbol,fill_time);
            (void)prices.completedClose(symbol,mark_time);
        }
        if (!ledger.state().held_symbol.empty()) {
            (void)prices.completedClose(ledger.state().held_symbol,fill_time);
        }
        const auto before=ledger.state().equity;
        const auto execution=ledger.execute(fill_time,desired,provider);
        ensure(execution.numerical_integrity,"ledger execution failed integrity");
        double marked=ledger.state().cash;
        if (ledger.state().quantity!=0)
            marked+=ledger.state().quantity*prices.completedClose(ledger.state().held_symbol,mark_time);
        const double reward=(marked-before)/cfg.initial_capital*100.0;
        ensure(std::isfinite(reward),"non-finite portfolio reward");
        manager_samples.push_back({manager_input,chosen,std::clamp(reward,-10.0,10.0)});
        // Candidate action targets use the NEXT completed minute prices only as
        // offline labels, never as an observation or input to policy inference.
        for (const auto& c:r.candidates) {
            const auto p0=prices.completedClose(c.symbol,fill_time);
            const auto p1=prices.completedClose(c.symbol,mark_time);
            const auto ret=(p1/p0-1.0)*100.0;
            const auto input=net.encodeAgent(c.observation);
            agent_samples.push_back({input,1,std::clamp(ret,-10.0,10.0)});
            // No SHORT action targets in default LONG-only training.
            // Dedicated directional training must opt in separately.
            agent_samples.push_back({input,0,0.0});
            agent_samples.push_back({input,3,0.0});
        }
        ++processed;
    }
    ensure(processed>=5 && !agent_samples.empty(),"too few consecutive 1-minute training decisions");
    double total_loss=0.0;
    for (std::size_t epoch=0;epoch<cfg.epochs;++epoch) {
        for (const auto& s:agent_samples)
            total_loss+=net.train(s.input,s.action,s.reward,cfg.learning_rate);
        for (const auto& s:manager_samples)
            total_loss+=net.train(s.input,s.action,s.reward,cfg.learning_rate);
    }
    // Holdout is strictly excluded from weight updates. Report a descriptive
    // prediction loss only; this is NOT an executable OOS profitability result.
    double validation_loss=0.0;
    std::size_t validation_count=0;
    for (std::size_t i=boundary;i+2<data.records.size();++i) {
        const auto& r=data.records[i];
        if (data.records[i+1].decision_time!=r.decision_time+60 ||
            data.records[i+2].decision_time!=r.decision_time+120) continue;
        for (const auto& c:r.candidates) {
            const auto p0=prices.completedClose(c.symbol,r.decision_time+60);
            const auto p1=prices.completedClose(c.symbol,r.decision_time+120);
            const auto input=net.encodeAgent(c.observation);
            validation_loss+=squared(net,{input,1,std::clamp((p1/p0-1.0)*100.0,-10.0,10.0)});
            ++validation_count;
        }
    }
    ensure(validation_count>0,"no valid chronological holdout labels");
    if (!cfg.checkpoint_file.parent_path().empty())
        std::filesystem::create_directories(cfg.checkpoint_file.parent_path());
    net.save(cfg.checkpoint_file);
    DevRealHistoricalTrainingResult out{};
    out.records=data.records.size();out.train_records=processed;
    out.agent_samples=agent_samples.size();out.manager_samples=manager_samples.size();
    out.training_loss=total_loss/(cfg.epochs*(agent_samples.size()+manager_samples.size()));
    out.validation_loss=validation_loss/validation_count;
    out.behavior_final_equity=ledger.state().equity;
    return out;
}
}
