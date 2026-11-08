#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main(int argc,char** argv) {
    try {
        // Explicit demonstration policy required: no production score is invented.
        if(argc<8 || std::string(argv[1])!="--demo-mean-reversion" || (argc-4)%2!=0) {
            std::cerr << "Usage: " << argv[0]
                      << " --demo-mean-reversion TOP_K BENCHMARK BENCHMARK_SYMBOL BENCHMARK_FILE STOCK FILE [STOCK FILE ...]\n"
                      << "Demo ranks by negative 15m close Z-score; NOT a BUY signal, trained model, or production strategy.\n";
            return 2;
        }
        const auto top_k=static_cast<std::size_t>(std::stoul(argv[2]));
        const std::string benchmark=argv[3];
        std::vector<DevHistoricalMinuteInput> inputs;
        for(int i=4;i<argc;i+=2) inputs.push_back({argv[i],argv[i+1]});
        DevHistoricalFeedConfig config;
        config.manager_id="DevManager_001";
        config.benchmark_symbol=benchmark;
        config.sessions.session_overrides["2025-10-21"]={13*60+45,14*60+45};
        DevRealHistoricalMultiStockFeed feed(config);
        DevManager001HistoricalRanking manager({config.manager_id,top_k},
            [](const DevHistoricalStockFrame& stock,const DevHistoricalManagerFrame&) {
                // Explicit demonstration policy: most negative 15m Z-score first.
                // Replace with the established DevManager_001 strategy policy before real decisions.
                return DevManager001Assessment{true,-stock.close_15m.zscore,"DEMO ONLY"};
            });
        std::size_t shown=0;
        const auto report = replayDevManager001Ranking(
            feed,
            inputs,
            manager,
            [&](const DevManager001RankingFrame& frame) {
                if (shown++ >= 3)
                    return;

                std::cout
                    << "DevManager_001 boundary=" << frame.decision_key
                    << " evaluated=" << frame.evaluated
                    << " selected=" << frame.selected.size()
                    << '\n';

                for (const auto& s : frame.selected) {
                    std::cout
                        << "  Symbol=" << s.symbol
                        << " DemoScore=" << s.ranking_score
                        << " ZScore_1m=" << s.observations.close_1m.zscore
                        << " ZScore_5m=" << s.observations.close_5m.zscore
                        << " ZScore_15m=" << s.observations.close_15m.zscore
                        << '\n';
                }
            }
        );
        for(const auto& symbol:report.feed.validation.symbols)
            std::cout << symbol.symbol << " rows=" << symbol.rows
                      << " missing=" << symbol.missing_session_minutes << '\n';
        std::cout << "D4_pass=" << (report.feed.validation.passed?"yes":"no")
                  << " D5_eligible=" << report.feed.eligible_decisions
                  << " manager_frames=" << report.manager_frames
                  << " frames_with_candidates=" << report.frames_with_candidates
                  << " candidate_assignments=" << report.candidate_assignments << '\n';
        if(!report.feed.validation.passed || report.manager_frames==0) {
            std::cout << "FAIL 5C-3I-D6 demo historical manager ranking\n";
            return 1;
        }
        std::cout << "PASS 5C-3I-D6 demo historical manager ranking (NOT production strategy)\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
