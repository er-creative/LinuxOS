#include "devai/strategy/dev_ai_strategy/DevSharedAgentTrainer.hpp"

#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;

struct TestStats { std::size_t checks{0}; std::size_t failures{0}; };

void require(bool condition, const std::string& text, TestStats& s)
{
    ++s.checks;
    if (condition) std::cout << "[PASS] " << text << '\n';
    else { ++s.failures; std::cerr << "[FAIL] " << text << '\n'; }
}

template <class F>
void expectThrow(F&& f, const std::string& text, TestStats& s)
{
    bool threw=false;
    try { f(); } catch (const std::exception&) { threw=true; }
    require(threw,text,s);
}

bool sameParameters(const DevQNetworkParameters& a,
                    const DevQNetworkParameters& b)
{
    return a.weights1==b.weights1 && a.biases1==b.biases1 &&
           a.weights2==b.weights2 && a.biases2==b.biases2 &&
           a.weights3==b.weights3 && a.biases3==b.biases3;
}

DevAIObservation makeObservation(
    std::string agent,
    std::string symbol,
    std::int64_t t,
    double shift)
{
    DevAIObservation o{};
    o.agent_name=std::move(agent);
    o.symbol=std::move(symbol);
    o.benchmark_symbol="NIFTY%2050";
    o.decision_time=t;

    for (std::size_t i=0;i<o.market.size();++i)
        o.market[i]=std::fmod(shift + static_cast<double>(i)*0.017,1.8)-0.9;

    o.availability.fill(1.0);
    o.portfolio.position=0.0;
    o.portfolio.unrealized_return=0.0;
    o.portfolio.realized_return=0.0;
    o.portfolio.equity_return=0.0;
    o.portfolio.drawdown=0.0;
    o.portfolio.holding_period=0.0;
    o.numerical_integrity=true;
    o.ready=true;
    return o;
}

DevTransition makeTransition(
    std::size_t index,
    const std::string& agent,
    const std::string& symbol)
{
    const std::int64_t t=1000+static_cast<std::int64_t>(index)*60;
    DevTransition x{};
    x.observation=makeObservation(agent,symbol,t,0.03*static_cast<double>(index));
    x.next_observation=makeObservation(agent,symbol,t+60,0.03*static_cast<double>(index+1));
    x.action=static_cast<DevAction>(index%4);
    x.reward=(static_cast<int>(index%5)-2)*0.001;
    x.terminal=(index==11);
    x.decision_time=t;
    x.next_decision_time=t+60;
    return x;
}

void run(TestStats& s)
{
    DevSharedAgentTrainerConfig config{};
    config.replay_capacity=32;
    config.minimum_replay_size=8;
    config.batch_size=8;
    config.replay_seed=316U;
    config.online_network.random_seed=1603U;
    config.training.mini_batch.bellman.discount_factor=0.99;
    config.training.mini_batch.training.learning_rate=0.001;
    config.training.mini_batch.training.huber_delta=1.0;
    config.training.mini_batch.training.gradient_clip=10.0;
    config.training.target_synchronization.synchronization_interval=2;
    config.exploration.epsilon_start=0.20;
    config.exploration.epsilon_end=0.05;
    config.exploration.decay_steps=1000;
    config.exploration.random_seed=316U;

    DevSharedAgentTrainer trainer(config);

    require(trainer.onlineNetwork().parameterCount()==7652,
            "Shared DevAgent trainer owns frozen 7,652-parameter Q-network",s);
    require(sameParameters(trainer.onlineNetwork().parameters(),
                           trainer.targetNetwork().parameters()),
            "Training initialization hard-synchronizes target from shared online network",s);
    require(!trainer.canTrain(),"Trainer waits for minimum replay experience",s);

    const std::vector<std::string> agents={
        "DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"};
    const std::vector<std::string> symbols={
        "RELIANCE","HDFCBANK","ICICIBANK","SBIN","BHARTIARTL"};

    for (std::size_t i=0;i<12;++i)
        trainer.observe(makeTransition(i,agents[i%agents.size()],symbols[i%symbols.size()]));

    require(trainer.replayBuffer().size()==12,
            "Experiences from all dynamic DevAgent identities enter one shared replay memory",s);
    require(trainer.canTrain(),"Shared trainer becomes trainable after replay warm-up",s);

    const auto before=trainer.onlineNetwork().parameters();
    const auto first=trainer.trainOneUpdate();
    require(first.numerical_integrity,"First shared mini-batch update has numerical integrity",s);
    require(first.batch_size==8,"First shared update uses configured batch size",s);
    require(first.completed_training_updates==1,"First shared update increments global training counter",s);
    require(!first.training.target_synchronized,"Target is not synchronized before configured interval",s);
    require(!sameParameters(before,trainer.onlineNetwork().parameters()),
            "Shared online Q-network parameters change after training",s);

    const auto second=trainer.trainOneUpdate();
    require(second.completed_training_updates==2,"Second shared update increments same global counter",s);
    require(second.training.target_synchronized,"Target synchronizes at configured second update",s);
    require(sameParameters(trainer.onlineNetwork().parameters(),
                           trainer.targetNetwork().parameters()),
            "Target equals shared online network immediately after synchronization",s);

    auto a1=makeObservation("DevAgent_001","RELIANCE",5000,0.10);
    auto a5=makeObservation("DevAgent_005","SBIN",5060,0.20);
    const auto d1=trainer.selectAction(a1,a1.decision_time);
    const auto d5=trainer.selectAction(a5,a5.decision_time);
    require(d1.numerical_integrity && d5.numerical_integrity,
            "Different DevAgent slots use the same shared policy with valid decisions",s);
    require(trainer.completedActionSelections()==2,
            "Action-selection schedule is global across dynamic DevAgent slots",s);

    const auto selections_before_reset=trainer.completedActionSelections();
    trainer.resetEpisode();
    require(trainer.completedActionSelections()==selections_before_reset,
            "Episode reset preserves global epsilon schedule",s);

    const std::filesystem::path model_path=
        std::filesystem::temp_directory_path()/"devai_phase_3_16_shared_agent.devq";
    std::error_code ec;
    std::filesystem::remove(model_path,ec);
    const auto meta=trainer.saveModel(model_path);
    require(meta.numerical_integrity,"Shared trained DevAgent model persists with integrity",s);
    require(meta.parameter_count==7652,"Persisted shared model preserves frozen parameter count",s);
    std::filesystem::remove(model_path,ec);

    trainer.clearExperienceAndResetTraining();
    require(trainer.replayBuffer().empty(),"Explicit training reset clears shared replay memory",s);
    require(trainer.completedTrainingUpdates()==0,"Explicit training reset clears training-update counter",s);
    require(sameParameters(trainer.onlineNetwork().parameters(),trainer.targetNetwork().parameters()),
            "Explicit training reset re-synchronizes target without erasing learned online parameters",s);

    DevSharedAgentTrainerConfig bad=config;
    bad.batch_size=9;
    bad.minimum_replay_size=8;
    expectThrow([&]{ DevSharedAgentTrainer x(bad); },
                "Trainer rejects batch size larger than replay warm-up threshold",s);

    DevSharedAgentTrainer fresh(config);
    expectThrow([&]{ (void)fresh.trainOneUpdate(); },
                "Trainer rejects update before replay warm-up",s);
}
}

int main()
{
    TestStats s{};
    std::cout << "\n======================================================================\n";
    std::cout << "DEVAI PHASE 3.16 — SHARED DEVAGENT TRAINING\n";
    std::cout << "======================================================================\n";

    try { run(s); }
    catch (const std::exception& e)
    {
        ++s.failures;
        std::cerr << "[UNCAUGHT] " << e.what() << '\n';
    }

    std::cout << "\n======================================================================\n";
    std::cout << "PHASE 3.16 SHARED DEVAGENT TRAINING SUMMARY\n";
    std::cout << "======================================================================\n";
    std::cout << "Checks   : " << s.checks << '\n';
    std::cout << "Failures : " << s.failures << '\n';
    std::cout << "======================================================================\n";

    if (s.failures==0)
    {
        std::cout << "DEVAI PHASE 3.16 SHARED DEVAGENT TRAINING: PASSED\n";
        return 0;
    }
    std::cout << "DEVAI PHASE 3.16 SHARED DEVAGENT TRAINING: FAILED\n";
    return 1;
}
