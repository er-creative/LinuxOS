#include "devai/market/MarketDataLoader.hpp"
#include "devai/strategy/dev_ai_strategy/forecast/DevMultiTimeframeFeatures_F3EF2C2.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace mt=devai::forecast::f3ef2c2;
namespace f1=devai::strategy::f3ef1b;
static std::vector<std::string> csv(const std::string&s){
    std::vector<std::string> out;std::istringstream in(s);std::string x;
    while(std::getline(in,x,','))out.push_back(x);
    return out;
}
int main(int argc,char**argv){
    if(argc!=5){std::cerr<<"Usage: "<<argv[0]<<" <minute_dir> <symbols_file> <f1c_dir> <output_dir>\n";return 2;}
    try{
        fs::path minute=argv[1],f1c=argv[3],outdir=argv[4];
        fs::create_directories(outdir);
        devai::market::MarketDataLoader loader(minute);
        auto load=[&](const std::string&sym){
            const auto r=loader.loadFile(sym+"_1min.txt");
            if(r.invalid_rows||r.duplicate_rows)throw std::runtime_error("unrepaired source data: "+sym);
            std::vector<mt::MinuteClose> bars;bars.reserve(r.candles.size());
            for(const auto&c:r.candles){
                if(c.symbol!=sym)throw std::runtime_error("symbol mismatch: "+sym);
                int m=f1::ist_minute_of_day(c.timestamp);
                if(m>=555&&m<930)bars.push_back({c.timestamp,c.close,0});
            }
            return bars;
        };
        const auto nifty=load("NIFTY%2050");
        const auto symbols=f1::read_symbols(argv[2]);
        std::size_t total=0,fail=0,processed=0;
        for(const auto&sym:symbols){
            if(sym=="NIFTY%2050"||sym=="NIFTY 50")continue;
            ++processed;
            try{
                const auto context=mt::build_context(load(sym),nifty);
                std::ifstream in(f1c/(sym+"_training_f3ef1c.csv"));
                if(!in)throw std::runtime_error("missing F1C input");
                const auto target=outdir/(sym+"_training_f3ef2c2.csv");
                std::ofstream out(target);
                if(!out)throw std::runtime_error("cannot create output");
                std::string line;
                if(!std::getline(in,line))throw std::runtime_error("empty input");
                const auto header=csv(line);
                auto find=[&](const std::string&name){
                    auto it=std::find(header.begin(),header.end(),name);
                    if(it==header.end())throw std::runtime_error("missing column: "+name);
                    return static_cast<std::size_t>(it-header.begin());
                };
                const auto col=find("decision_epoch"),sess=find("session_id"),symbol=find("symbol");
                out<<line<<",ret_5m_past,ret_15m_past,mean5_4,std5_4,z5_4,mean15_2,std15_2,z15_2,volatility20,mean60,std60,z60,nifty_ret_5m_past,nifty_ret_15m_past,relative_ret_5m,relative_ret_15m,age5_minutes,age15_minutes\n";
                out<<std::setprecision(17);
                std::size_t written=0,skipped=0;
                while(std::getline(in,line)){
                    if(line.empty())continue;
                    const auto fields=csv(line);
                    if(fields.size()<=std::max({col,sess,symbol}))throw std::runtime_error("bad input CSV row");
                    if(fields[symbol]!=sym)throw std::runtime_error("input symbol mismatch");
                    auto decision=std::stoll(fields[col]);
                    auto it=context.find(decision);
                    if(it==context.end()){++skipped;continue;}
                    if(f1::ist_session_id(decision-60)!=std::stoi(fields[sess]))throw std::runtime_error("session mismatch");
                    const auto&c=it->second;
                    out<<line<<','<<c.ret5<<','<<c.ret15<<','<<c.mean5_4<<','<<c.std5_4<<','<<c.z5_4
                       <<','<<c.mean15_2<<','<<c.std15_2<<','<<c.z15_2<<','<<c.volatility20
                       <<','<<c.mean60<<','<<c.std60<<','<<c.z60<<','<<c.nifty_ret5<<','<<c.nifty_ret15
                       <<','<<c.relative_ret5<<','<<c.relative_ret15<<','<<c.age5<<','<<c.age15<<'\n';
                    ++written;
                }
                if(!out||!written)throw std::runtime_error("empty output or write failure");
                total+=written;
                std::cout<<"PASS "<<sym<<" rows="<<written<<" missing_or_warmup="<<skipped<<" csv="<<target<<'\n';
            }catch(const std::exception&e){++fail;std::cerr<<"FAIL "<<sym<<": "<<e.what()<<'\n';}
        }
        std::cout<<"F3E-F2C.2 symbols="<<processed<<" failures="<<fail<<" rows="<<total<<'\n';
        return fail?1:0;
    }catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
