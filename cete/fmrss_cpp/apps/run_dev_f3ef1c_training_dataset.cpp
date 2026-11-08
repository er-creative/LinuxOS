#include "devai/market/MarketDataLoader.hpp"
#include "devai/strategy/dev_ai_strategy/forecast/DevTrainingDataset_F3EF1C.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace f=devai::strategy::f3ef1c;
namespace b=devai::strategy::f3ef1b;
int main(int argc,char** argv) {
    if(argc!=5) {
        std::cerr<<"Usage: "<<argv[0]<<" <minute_dir> <symbols_file> <f1b_labels_dir> <output_dir>\n";
        return 2;
    }
    try {
        const f::fs::path minute_dir=argv[1], symbols_file=argv[2], label_dir=argv[3], output_dir=argv[4];
        const auto symbols=b::read_symbols(symbols_file);
        devai::market::MarketDataLoader loader(minute_dir);
        auto load=[&](const std::string& symbol) {
            const auto result=loader.loadFile(symbol+"_1min.txt");
            if(result.invalid_rows || result.duplicate_rows)
                throw std::runtime_error("Unrepaired source data for "+symbol);
            std::vector<f::MinuteClose> bars;
            bars.reserve(result.candles.size());
            for(const auto& c:result.candles) {
                if(c.symbol!=symbol) throw std::runtime_error("Symbol mismatch for "+symbol);
                const int minute=b::ist_minute_of_day(c.timestamp);
                if(minute>=555 && minute<930) bars.push_back({c.timestamp,c.close,0});
            }
            return bars;
        };
        const auto nifty=f::make_nifty_returns(load("NIFTY%2050"));
        std::size_t total=0, failures=0, processed=0;
        f::fs::create_directories(output_dir);
        for(const auto& symbol:symbols) {
            if(symbol=="NIFTY%2050" || symbol=="NIFTY 50") continue;
            ++processed;
            try {
                const auto bars=load(symbol);
                const auto labels=f::load_labels(label_dir/(symbol+"_forward_returns_f3ef1b.csv"),symbol);
                const auto destination=output_dir/(symbol+"_training_f3ef1c.csv");
                const auto count=f::write_dataset(destination,symbol,bars,labels,nifty);
                if(count.rows==0) throw std::runtime_error("No eligible training rows");
                total+=count.rows;
                std::cout<<"PASS "<<symbol<<" rows="<<count.rows
                         <<" warmup_or_gap="<<count.skipped_warmup
                         <<" missing_nifty="<<count.skipped_missing_nifty
                         <<" missing_or_invalid_labels="<<count.skipped_missing_label
                         <<" csv="<<destination<<'\n';
            } catch(const std::exception& e) {
                ++failures;
                std::cerr<<"FAIL "<<symbol<<": "<<e.what()<<'\n';
            }
        }
        std::cout<<"F3E-F1C symbols="<<processed<<" failures="<<failures<<" rows="<<total<<'\n';
        return failures?1:0;
    } catch(const std::exception& e) {
        std::cerr<<"F3E-F1C ERROR: "<<e.what()<<'\n'; return 1;
    }
}
