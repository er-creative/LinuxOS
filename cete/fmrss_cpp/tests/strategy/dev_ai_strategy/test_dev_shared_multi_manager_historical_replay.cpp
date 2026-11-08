#include "devai/strategy/dev_ai_strategy/DevSharedMultiManagerHistoricalReplay.hpp"
#include "Dev5C3HTestFixture.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
namespace fx = cete_5c3h_test;

static DevSharedCandidate candidate(const std::string& symbol,std::int64_t t){
    DevSharedCandidate c{};c.symbol=symbol;c.slot=0;c.candidate_rank=1;c.candidate_score=0.9;
    c.observation.symbol=symbol;c.observation.decision_time=t;
    c.observation.ready=true;c.observation.numerical_integrity=true;return c;
}
int main(){
    DevQNetwork agent({42U,true});DevManagerQNetwork manager({42U,true});
    DevinderjeetSupervisor supervisor({100000,0.75,5,2,2});
    auto a=fx::ids(1),b=fx::ids(6);
    supervisor.registerManager({"DevManager_001",{a.begin(),a.end()},50000});
    supervisor.registerManager({"DevManager_002",{b.begin(),b.end()},50000});
    fx::Fixture fixture(std::filesystem::temp_directory_path()/"cete_5c3h_historical_replay_test",supervisor);
    fixture.add("DevManager_001",{"RELIANCE"});
    fixture.add("DevManager_002",{"HDFCBANK"});
    DevSharedModelMultiManagerRuntime runtime(agent,manager,supervisor,fixture.execution);
    runtime.addManager("DevManager_001",a);runtime.addManager("DevManager_002",b);
    std::vector<DevReplayTick> ticks;
    for(std::int64_t t: {1000LL,1060LL})
        ticks.push_back({t,{{"DevManager_001",{candidate("RELIANCE",t)}},
                             {"DevManager_002",{candidate("HDFCBANK",t)}}}});
    DevSharedMultiManagerHistoricalReplay replay;
    int calls=0;
    const auto prices=[&](const std::string& symbol,std::int64_t t){
        ++calls;assert(t==1000||t==1060);
        return symbol=="RELIANCE"?2500.0:1500.0;
    };
    const auto result=replay.run(ticks,runtime,supervisor,prices);
    assert(result.equity_curve.size()==3);
    assert(result.initial_equity==100000&&result.final_equity==100000);
    assert(result.filled==0&&result.rejected==0);
    bool bad=false;
    try{(void)replay.run({ticks[1],ticks[0]},runtime,supervisor,prices);}
    catch(const std::invalid_argument&){bad=true;}
    assert(bad);
    std::cout<<"PASS shared multi-manager replay: deterministic HOLD, strict timestamps, equity, terminal settlement path\n";
}
