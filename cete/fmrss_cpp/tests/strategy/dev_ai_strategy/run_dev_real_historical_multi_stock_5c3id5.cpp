#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockFeed.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
int main(int argc,char** argv) {
    if(argc<6 || (argc-2)%2!=0) {
        std::cerr<<"Usage: "<<argv[0]<<" BENCHMARK_SYMBOL SYMBOL FILE STOCK FILE [STOCK FILE ...]\n";
        return 2;
    }
    try {
        DevHistoricalFeedConfig config;config.benchmark_symbol=argv[1];
        std::vector<DevHistoricalMinuteInput> inputs;
        for(int i=2;i+1<argc;i+=2)inputs.push_back({argv[i],argv[i+1]});
        DevRealHistoricalMultiStockFeed feed(config);
        std::size_t shown = 0;

        auto report = feed.replay(inputs, [&](const DevHistoricalManagerFrame& frame) {
            if (shown++ < 3) {
                std::cout << "DevManager_001 boundary="
                          << frame.decision_key
                          << " stocks=" << frame.stocks.size()
                          << '\n';

                for (const auto& stock : frame.stocks) {
                    std::cout << "  Symbol=" << stock.symbol
                              << " ZScore_1m=" << stock.close_1m.zscore
                              << " ZScore_5m=" << stock.close_5m.zscore
                              << " ZScore_15m=" << stock.close_15m.zscore
                              << '\n';
                }
            }
        });
        for(const auto& s:report.validation.symbols)
            std::cout<<s.symbol<<" rows="<<s.rows<<" invalid="<<s.invalid_rows<<" duplicates="<<s.duplicate_minutes<<" missing="<<s.missing_session_minutes<<"\n";
        std::cout<<"D4_pass="<<(report.validation.passed?"yes":"no")<<" eligible_decisions="<<report.eligible_decisions
                 <<" skipped_incomplete="<<report.skipped_incomplete<<" skipped_warmup="<<report.skipped_warmup<<'\n';
        for(const auto& issue:report.issues)std::cerr<<"ISSUE: "<<issue<<'\n';
        const bool pass=report.validation.passed && report.eligible_decisions>0;
        std::cout<<(pass?"PASS":"FAIL")<<" 5C-3I-D5 real historical multi-stock feed\n";
        return pass?0:1;
    } catch(const std::exception& e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
