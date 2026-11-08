#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main() {
    const auto root=std::filesystem::temp_directory_path()/"cete_d7c_1min_fixture";
    std::filesystem::create_directories(root);
    std::vector<DevHistoricalMinuteInput> inputs;
    for(const std::string symbol : {"NIFTY%2050", "RELIANCE", "SBIN"}) {
        const auto file=root/(symbol+".txt");
        std::ofstream out(file);
        for(int i=0;i<90;++i) {
            if(symbol=="SBIN" && i==61)continue;
            const int t=555+i;
            const double price=100.0+i*0.01;
            out<<symbol<<",2025-01-02,"<<(t/60<10?"0":"")<<t/60<<":"
               <<(t%60<10?"0":"")<<t%60<<","<<price<<","<<price+1
               <<","<<price-1<<","<<price<<",100\n";
        }
        inputs.push_back({symbol,file.string()});
    }
    DevHistoricalFeedConfig config;
    config.rolling_window=2;config.minimum_history=2;
    DevRealHistoricalMultiStockFeed feed(config);
    DevD7StrategyConfig strategy_config;
    strategy_config.min_expected_return=-1.0;
    DevManager001StatisticalStrategyD7 strategy(strategy_config,diagnosticPastOnlyForecast);
    DevManager001HistoricalStrategyD7B manager({"DevManager_001",2,1},strategy);
    std::int64_t previous=0;
    std::size_t consecutive=0,frames=0,carried_5m=0,carried_15m=0;
    const auto report=manager.replay(feed,inputs,[&](const DevManager001RankingFrame& f) {
        if(previous && f.decision_key==previous+1)++consecutive;
        assert(f.decision_key>previous);
        previous=f.decision_key;++frames;
        for (const auto& candidate : f.selected) {
            const auto& obs = candidate.observations;
            assert(obs.one_minute.completed_key == f.decision_key);
            assert(obs.five_minute.completed_key <= f.decision_key);
            assert(f.decision_key - obs.five_minute.completed_key < 5);
            assert(obs.fifteen_minute.completed_key <= f.decision_key);
            assert(f.decision_key - obs.fifteen_minute.completed_key < 15);
            if (obs.five_minute.completed_key < f.decision_key) ++carried_5m;
            if (obs.fifteen_minute.completed_key < f.decision_key) ++carried_15m;
        }
    });
    assert(report.feed.validation.passed);
    assert(frames==report.decision_frames);
    assert(frames>5 && consecutive>5);
    assert(report.evaluated==frames*2);
    assert(carried_5m > 0 && carried_15m > 0);
    std::filesystem::remove_all(root);
    std::cout<<"PASS 5C-3I-D7C one-minute decision synchronization\n";
}
