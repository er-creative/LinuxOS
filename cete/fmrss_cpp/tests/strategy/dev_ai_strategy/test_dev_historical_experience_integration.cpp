#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include "Dev5C3HTestFixture.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    const std::vector<std::string> universe={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN"};
    const auto dir=std::filesystem::temp_directory_path()/"cete_4c4_fixture";
    std::filesystem::create_directories(dir);
    DevRealMinuteMarket market;
    for(const auto& sym:universe) {
        std::ofstream f(dir/(sym+"_1min.txt"));
        f<<"Symbol,Date,Time,Open,High,Low,Close,Volume\n";
        for(int m=15;m<=24;++m) {
            const double p=sym=="SBIN"?50.0+(m-15):100.0;
            f<<sym<<",2026-10-09,09:"<<m<<":00,"<<p<<","<<p<<","<<p<<","<<p<<",100\n";
        }
        f.close();
        market.load(sym,dir/(sym+"_1min.txt"));
    }
    const std::int64_t start=1791517500;
    std::vector<DevUniverseRankingTick> ranking;
    for(int i=1;i<=5;++i) {
        DevUniverseRankingTick tick{};tick.decision_time=start+i*60;
        for(std::size_t j=0;j<universe.size();++j) {
            DevUniverseRankedShare s{};s.symbol=universe[j];s.score=double(j);
            s.observation.symbol=s.symbol;s.observation.decision_time=tick.decision_time;
            s.observation.ready=true;s.observation.numerical_integrity=true;
            tick.universe.push_back(s);
        }
        ranking.push_back(tick);
    }
    auto execute=[&] {
        DevSupervisorConfig cfg{};cfg.initial_capital=100000;
        cfg.global_max_exposure_fraction=1.0;cfg.max_open_positions=1;
        cfg.transaction_cost_bps=0;cfg.slippage_bps=0;
        DevinderjeetSupervisor supervisor(cfg);
        supervisor.registerManager({"DevManager_001",{"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"},100000});
        cete_5c3h_test::Fixture fixture(
            std::filesystem::temp_directory_path()/"cete_5c3h_historical_experience_approval",supervisor);
        fixture.add("DevManager_001",universe);
        DevQNetwork agent;
        DevManagerQNetwork manager;
        DevSharedModelMultiManagerRuntime runtime(agent,manager,supervisor,fixture.execution);
        runtime.addManager("DevManager_001",DevSingleManagerTop5Replay::agentIds());
        const auto decision=[&](const DevReplayTick& tick,DevSharedModelMultiManagerRuntime&)->DevRotationIntent {
            if(tick.decision_time==start+60) return {DevRotationAction::BUY,"SBIN","DevAgent_001"};
            return {DevRotationAction::HOLD,{},{}};
        };
        auto r=DevHistoricalExperienceIntegration::run(ranking,universe,runtime,supervisor,market,decision,0.5,true);
        assert(r.experiences.size()==5);
        assert(r.execution.replay.filled==2); // BUY + terminal exit
        assert(r.experiences.front().candidates.size()==5);
        assert(r.experiences.front().next_candidates.size()==5);
        assert(r.experiences.back().terminal);
        assert(r.experiences.back().next_candidates.empty());
        assert(r.experiences[0].intent.symbol=="SBIN");
        double total=0;
        for(const auto& e:r.experiences) total+=e.portfolio_reward;
        assert(std::abs(total-(r.execution.replay.net_pnl/r.execution.replay.initial_equity))<1e-10);
        assert(supervisor.snapshot().open_positions==0);
        return r;
    };
    const auto a=execute(),b=execute();
    assert(a.experiences.size()==b.experiences.size());
    for(std::size_t i=0;i<a.experiences.size();++i)
        assert(a.experiences[i].portfolio_reward==b.experiences[i].portfolio_reward);
    bool rejected=false;
    try {
        auto bad=ranking;bad[0].universe[0].observation.decision_time-=60;
        DevSupervisorConfig cfg{};DevinderjeetSupervisor s(cfg);
        cete_5c3h_test::Fixture fixture(
            std::filesystem::temp_directory_path()/"cete_5c3h_stale_experience_approval",s);
        DevQNetwork ag;DevManagerQNetwork mg;
        DevSharedModelMultiManagerRuntime rt(ag,mg,s,fixture.execution);
        auto dummy=[](const DevReplayTick&,DevSharedModelMultiManagerRuntime&){return DevRotationIntent{};};
        (void)DevHistoricalExperienceIntegration::run(bad,universe,rt,s,market,dummy);
    } catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    std::filesystem::remove_all(dir);
    std::cout<<"PASS historical experience integration: top5, chronological fills, supervisor rewards, deterministic replay, stale rejection\n";
}
