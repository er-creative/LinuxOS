#include "devai/market/MarketDataLoader.hpp"
#include "devai/strategy/dev_ai_strategy/forecast/DevRealMarketForwardReturns_F3EF1B.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace f = devai::strategy::f3ef1b;
int main(int argc,char** argv) {
    if(argc!=4) {
        std::cerr << "Usage: "<<argv[0]<<" <minute_data_dir> <symbols_file> <output_dir>\n";
        return 2;
    }
    try {
        const f::fs::path data_dir=argv[1], symbols_path=argv[2], output_dir=argv[3];
        const auto symbols=f::read_symbols(symbols_path);
        devai::market::MarketDataLoader loader(data_dir);
        std::size_t all_rows=0, valid10=0, failed=0;
        for(const auto& symbol:symbols) {
            try {
                const auto result=loader.loadFile(symbol+"_1min.txt");
                std::vector<f::MinuteClose> bars;
                bars.reserve(result.candles.size());
                for(const auto& candle:result.candles) {
                    if(candle.symbol!=symbol) throw std::runtime_error("Unexpected symbol in "+symbol+" file: "+candle.symbol);
                    const int minute=f::ist_minute_of_day(candle.timestamp);
                    if(minute<555 || minute>=930) continue;
                    bars.push_back({candle.timestamp,candle.close,0});
                }
                const auto rows=f::generate(bars);
                const auto path=output_dir/(symbol+"_forward_returns_f3ef1b.csv");
                f::write_csv(path,symbol,rows);
                std::size_t n10=0;
                for(const auto& row:rows) if(row.horizons[3].valid) ++n10;
                all_rows+=rows.size(); valid10+=n10;
                std::cout<<"PASS "<<symbol<<" candles="<<bars.size()<<" valid_10m="<<n10
                         <<" invalid_rows="<<result.invalid_rows<<" duplicates="<<result.duplicate_rows
                         <<" csv="<<path.string()<<'\n';
            } catch(const std::exception& e) {
                ++failed;
                std::cerr<<"FAIL "<<symbol<<": "<<e.what()<<'\n';
            }
        }
        std::cout<<"F3E-F1B: symbols="<<symbols.size()<<" failed="<<failed
                 <<" total_rows="<<all_rows<<" valid_10m="<<valid10<<'\n';
        return failed?1:0;
    } catch(const std::exception& e) {
        std::cerr<<"F3E-F1B ERROR: "<<e.what()<<'\n'; return 1;
    }
}
