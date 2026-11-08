#include "devai/strategy/dev_ai_strategy/DevSingleManagerTop5Replay.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace devai::strategy {
std::array<std::string, DEV_AGENT_SLOT_COUNT> DevSingleManagerTop5Replay::agentIds() {
    return {"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"};
}
std::vector<DevReplayTick> DevSingleManagerTop5Replay::prepare(
    const std::vector<DevUniverseRankingTick>& ranking_ticks,
    const std::vector<std::string>& expected_universe) {
    if (ranking_ticks.empty() || expected_universe.size()!=10)
        throw std::invalid_argument("top5 replay requires ticks and exactly 10 universe symbols");
    const std::set<std::string> expected(expected_universe.begin(),expected_universe.end());
    if(expected.size()!=10 || expected.count("")!=0)
        throw std::invalid_argument("universe must contain 10 distinct nonempty symbols");
    std::vector<DevReplayTick> result;
    std::int64_t previous=0;
    for(const auto& tick:ranking_ticks) {
        if(tick.decision_time<=previous || tick.universe.size()!=10)
            throw std::invalid_argument("nonchronological tick or incomplete 10-share ranking");
        previous=tick.decision_time;
        std::set<std::string> observed;
        auto ranked=tick.universe;
        for(const auto& share:ranked) {
            if(!expected.count(share.symbol) || !observed.insert(share.symbol).second ||
               !std::isfinite(share.score) || !share.observation.ready ||
               !share.observation.numerical_integrity ||
               share.observation.decision_time!=tick.decision_time ||
               share.observation.symbol!=share.symbol)
                throw std::invalid_argument("invalid universe score, identity or causal observation");
        }
        std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){
            if(a.score!=b.score) return a.score>b.score;
            return a.symbol<b.symbol;
        });
        DevReplayManagerInput manager{};
        manager.manager_id=managerId;
        for(std::size_t i=0;i<DEV_AGENT_SLOT_COUNT;++i) {
            DevSharedCandidate c{};
            c.slot=i;c.symbol=ranked[i].symbol;c.candidate_score=ranked[i].score;
            c.candidate_rank=i+1;c.observation=ranked[i].observation;
            manager.candidates.push_back(std::move(c));
        }
        DevReplayTick output{};
        output.decision_time=tick.decision_time;
        output.managers.push_back(std::move(manager));
        result.push_back(std::move(output));
    }
    return result;
}
DevRealReplayResult DevSingleManagerTop5Replay::run(
    const std::vector<DevReplayTick>& ticks,DevSharedModelMultiManagerRuntime& runtime,
    DevinderjeetSupervisor& supervisor,const DevRealMinuteMarket& market,double fraction) {
    if(ticks.empty()) throw std::invalid_argument("empty replay");
    const auto account=supervisor.managerAccount(managerId);
    const auto ids=agentIds();
    if(account.registration.agents!=std::vector<std::string>(ids.begin(),ids.end()))
        throw std::invalid_argument("DevManager_001 must own exactly DevAgent_001..005");
    for(const auto& tick:ticks)
        if(tick.managers.size()!=1 || tick.managers.front().manager_id!=managerId ||
           tick.managers.front().candidates.size()!=DEV_AGENT_SLOT_COUNT)
            throw std::invalid_argument("single manager with five ranked candidates required");
    return DevRealMarketMultiManagerReplay{}.run(ticks,runtime,supervisor,market,fraction);
}
}
