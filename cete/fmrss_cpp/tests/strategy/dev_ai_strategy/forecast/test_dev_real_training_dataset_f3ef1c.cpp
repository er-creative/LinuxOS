#include "devai/market/MarketDataLoader.hpp"
#include "devai/strategy/dev_ai_strategy/forecast/DevTrainingDataset_F3EF1C.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace f=devai::strategy::f3ef1c;
namespace b=devai::strategy::f3ef1b;
int main(int argc,char** argv) {
    if(argc!=4) {
        std::cerr<<"Usage: "<<argv[0]<<" <minute_dir> <f1b_labels_dir> <output_dir>\n";
        return 2;
    }
    try {
        const f::fs::path data_dir=argv[1], labels_dir=argv[2], output_dir=argv[3];
        devai::market::MarketDataLoader loader(data_dir);
        auto load=[&](const std::string& symbol) {
            const auto loaded=loader.loadFile(symbol+"_1min.txt");
            if(loaded.invalid_rows || loaded.duplicate_rows) throw std::runtime_error("Invalid source candles");
            std::vector<f::MinuteClose> bars;
            for(const auto& c:loaded.candles) {
                if(c.symbol!=symbol) throw std::runtime_error("Unexpected symbol");
                const auto minute=b::ist_minute_of_day(c.timestamp);
                if(minute>=555 && minute<930) bars.push_back({c.timestamp,c.close,0});
            }
            return bars;
        };
        const auto nifty=f::make_nifty_returns(load("NIFTY%2050"));
        const auto bars=load("RELIANCE");
        const auto labels=f::load_labels(labels_dir/"RELIANCE_forward_returns_f3ef1b.csv","RELIANCE");
        const auto output=output_dir/"RELIANCE_integration_test_f3ef1c.csv";
        const auto counts=f::write_dataset(output,"RELIANCE",bars,labels,nifty);
        if(counts.rows<100 || counts.rows>bars.size()) throw std::runtime_error("Unexpected dataset row count");
        std::cout<<"F3E-F1C REAL INTEGRATION: PASS RELIANCE rows="<<counts.rows
                 <<" warmup_or_gap="<<counts.skipped_warmup
                 <<" missing_nifty="<<counts.skipped_missing_nifty
                 <<" missing_labels="<<counts.skipped_missing_label<<'\n';
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"F3E-F1C REAL INTEGRATION: FAIL "<<e.what()<<'\n';return 1;
    }
}
