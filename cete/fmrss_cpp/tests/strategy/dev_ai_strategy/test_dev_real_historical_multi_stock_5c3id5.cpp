#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockFeed.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main() {
    const auto root=std::filesystem::temp_directory_path()/"cete_5c3id5_fixture";
    std::filesystem::create_directories(root);
    std::vector<DevHistoricalMinuteInput> inputs;
    for(const auto& sym:std::vector<std::string>{"NIFTY%2050","AAA","BBB"}) {
        const auto path=root/(sym+".txt");std::ofstream out(path);
        for(int i=0;i<75;++i) {
            // A gap in AAA inside a 15-minute interval must not be imputed.
            if(sym=="AAA" && i==40)continue;
            const int minute=9*60+15+i;
            const double price=100.0+i*0.1+(sym=="BBB"?10.0:0.0);
            out<<sym<<",2025-01-02,"<< (minute/60<10?"0":"") <<minute/60<<":"<<(minute%60<10?"0":"")<<minute%60
               <<","<<price<<","<<price+1<<","<<price-1<<","<<price<<",0\n";
        }
        out.close();inputs.push_back({sym,path.string()});
    }
    DevHistoricalFeedConfig config;config.rolling_window=2;config.minimum_history=2;
    DevRealHistoricalMultiStockFeed feed(config);
    std::size_t seen=0;std::int64_t last=0;
    const auto report=feed.replay(inputs,[&](const DevHistoricalManagerFrame& frame){
        assert(frame.manager_id=="DevManager_001");assert(frame.stocks.size()==2);
        assert(frame.decision_key>last);last=frame.decision_key;
        for(const auto& s:frame.stocks) {
            assert(s.one_minute.completed_key==frame.decision_key);
            assert(s.five_minute.completed_key<=frame.decision_key);
            assert(frame.decision_key-s.five_minute.completed_key<5);
            assert(s.fifteen_minute.completed_key<=frame.decision_key);
            assert(frame.decision_key-s.fifteen_minute.completed_key<15);
            assert(s.close_1m.ready && s.close_5m.ready && s.close_15m.ready);
            assert(std::isfinite(s.close_15m.zscore));
        }
        ++seen;
    });
    assert(report.validation.passed);
    assert(report.validation.symbols.size()==3);
    assert(report.eligible_decisions==seen);
    assert(seen>2); // One-minute boundaries, with missing-data exclusions
    assert(report.skipped_incomplete>=1);
    std::filesystem::remove_all(root);
    std::cout<<"PASS 5C-3I-D5 real historical multi-stock causal feed contract\n";
}
