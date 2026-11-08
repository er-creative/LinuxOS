#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main(int argc,char** argv) {
    try {
        // The baseline is explicitly opt-in and NOT a forward-return predictor.
        if(argc<8 || (argc-4)%2!=0 ||
           std::string(argv[1])!="--diagnostic-past-only") {
            std::cerr<<"Usage: "<<argv[0]
              <<" --diagnostic-past-only TOP_K BENCHMARK BENCHMARK_SYMBOL BENCHMARK_FILE STOCK FILE [STOCK FILE ...]\n"
              <<"WARNING: past 5m return is used as a diagnostic proxy, NOT a trained forecast or BUY signal.\n";
            return 2;
        }
        const auto top_k=static_cast<std::size_t>(std::stoul(argv[2]));
        std::vector<DevHistoricalMinuteInput> inputs;
        for(int i=4;i<argc;i+=2) inputs.push_back({argv[i],argv[i+1]});
        DevHistoricalFeedConfig feed_config;
        feed_config.manager_id="DevManager_001";
        feed_config.benchmark_symbol=argv[3];
        feed_config.sessions.session_overrides["2025-10-21"]={13*60+45,14*60+45};
        DevRealHistoricalMultiStockFeed feed(feed_config);
        DevD7StrategyConfig strategy_config;
        strategy_config.manager_id=feed_config.manager_id;
        // Zero threshold means diagnostic forecast>=0 is eligible.
        strategy_config.min_expected_return=0.0;
        DevManager001StatisticalStrategyD7 strategy(
            strategy_config, diagnosticPastOnlyForecast);
        DevManager001HistoricalStrategyD7B manager(
            {feed_config.manager_id,top_k,1},strategy);
        std::size_t shown=0;
        const auto report=manager.replay(feed,inputs,[&](const DevManager001RankingFrame& frame) {
            if(shown++>=3) return;
            std::cout<<"DevManager_001 decision_key="<<frame.decision_key
                     <<" evaluated="<<frame.evaluated<<" rejected="<<frame.rejected
                     <<" selected="<<frame.selected.size()<<'\n';
            for(const auto& s:frame.selected) {
                const auto& o=s.observations;
                std::cout<<"  "<<s.symbol<<" diagnostic_past_return="<<s.ranking_score
                         <<" z1="<<o.close_1m.zscore
                         <<" z5="<<o.close_5m.zscore
                         <<" z15="<<o.close_15m.zscore<<'\n';
            }
        });
        for(const auto& symbol:report.feed.validation.symbols)
            std::cout<<symbol.symbol<<" rows="<<symbol.rows
                     <<" missing="<<symbol.missing_session_minutes<<'\n';
        std::cout<<"D4_pass="<<(report.feed.validation.passed?"yes":"no")
                 <<" D5_eligible="<<report.feed.eligible_decisions
                 <<" observed_frames="<<report.observed_frames
                 <<" decision_frames="<<report.decision_frames
                 <<" evaluated="<<report.evaluated
                 <<" rejected="<<report.rejected
                 <<" selected="<<report.selected<<'\n';
        if(!report.feed.validation.passed || report.decision_frames==0) return 1;
        std::cout<<"PASS 5C-3I-D7B real historical diagnostic strategy replay"
                    " (NOT production forecast or trading)\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<'\n'; return 1;
    }
}
