#include "devai/strategy/dev_ai_strategy/DevManager001Top5ForecastD8.hpp"
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;
namespace {
using Key=std::int64_t;
int digits(const std::string& s,int start,int len){int n=0;for(int i=start;i<start+len;++i){if(s[i]<'0'||s[i]>'9')throw std::invalid_argument("Invalid date");n=n*10+s[i]-'0';}return n;}
bool leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
Key dayOrdinal(int y,unsigned m,unsigned d){y-=m<=2;const int era=(y>=0?y:y-399)/400;const unsigned yoe=static_cast<unsigned>(y-era*400);const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;const unsigned doe=yoe*365+yoe/4-yoe/100+doy;return static_cast<Key>(era)*146097+doe;}
Key oosKey(const std::string& s){if(s.size()!=10||s[4]!='-'||s[7]!='-')throw std::invalid_argument("OOS date YYYY-MM-DD required");int y=digits(s,0,4),m=digits(s,5,2),d=digits(s,8,2);static const int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};if(y<1900||y>2200||m<1||m>12||d<1||d>days[m]+(m==2&&leap(y)))throw std::invalid_argument("Invalid date");return dayOrdinal(y,m,d)*1440;}
struct Metrics {std::size_t n=0;double sq=0,baseline_sq=0,price_sq=0;};
}
int main(int argc,char** argv){
 try {
    if(argc!=23){std::cerr<<"Usage: "<<argv[0]<<" OOS_START_YYYY-MM-DD NIFTY_FILE SYMBOL FILE [x10]\n";return 2;}
    const Key split=oosKey(argv[1]);
    std::vector<DevHistoricalMinuteInput> inputs{{"NIFTY%2050",argv[2]}};
    std::set<std::string> symbols;
    for(int i=3;i<argc;i+=2){std::string symbol=argv[i];if(symbol.empty()||symbol=="NIFTY%2050"||!symbols.insert(symbol).second)throw std::invalid_argument("Exactly ten unique stocks required");inputs.push_back({symbol,argv[i+1]});}
    if(symbols.size()!=10)throw std::invalid_argument("Exactly ten stocks required");
    DevHistoricalFeedConfig cfg;cfg.manager_id="DevManager_001";cfg.benchmark_symbol="NIFTY%2050";
    cfg.sessions.session_overrides["2025-10-21"]={13*60+45,14*60+45};
    DevRealHistoricalMultiStockFeed feed(cfg);
    std::vector<DevD8Observation> observations;
    const auto prep=feed.replay(inputs,[&](const DevHistoricalManagerFrame& f){
        for(const auto& s:f.stocks)observations.push_back(devD8MakeObservation(s,f));
    });
    if(!prep.validation.passed)throw std::runtime_error("D4 validation failed");
    DevD8TrainingConfig tc;tc.oos_start_key=split;tc.horizon_minutes=5;
    tc.ridge_lambda=1.0;tc.minimum_training_labels=100;
    DevCausalForwardReturnD8 model(tc);
    const auto fitted=model.fitOffline(observations);
    DevManager001HistoricalRanking ranker({"DevManager_001",5},devD8StatisticalRankingPolicy);
    DevManager001Top5ForecastD8 engine(ranker,model,tc.horizon_minutes);
    std::map<std::string,std::map<Key,double>> closes;
    for(const auto& o:observations)closes[o.symbol].emplace(o.decision_key,o.close);
    Metrics metrics;
    std::size_t frames=0,assignments=0,ready=0,unavailable=0,non5=0,consecutive=0;
    Key prev=0;
    // Fresh replay: D6 selects first, D8 forecasts only the selected Top-5.
    const auto replay=feed.replay(inputs,[&](const DevHistoricalManagerFrame& f){
        if(f.decision_key<split)return;
        if(prev && f.decision_key<=prev)throw std::runtime_error("Nonmonotonic OOS decisions");
        if(prev && f.decision_key==prev+1)++consecutive;
        prev=f.decision_key;++frames;
        if(f.decision_key%5!=0)++non5;
        const auto out=engine.evaluate(f);
        if(out.evaluated!=10 || out.selected.size()>5)throw std::runtime_error("Invalid Top-5 ranking");
        assignments+=out.selected.size();
        for(const auto& p:out.selected){
            if(!p.ready){++unavailable;continue;}
            ++ready;
            if(p.as_of_key!=f.decision_key || p.target_key!=f.decision_key+5 ||
               !std::isfinite(p.predicted_future_price))throw std::runtime_error("Invalid forecast timestamp/price");
            const auto it=closes.at(p.symbol).find(p.target_key);
            if(it==closes.at(p.symbol).end())continue;
            const double actual_return=it->second/p.current_price-1.0;
            const double err=p.predicted_forward_return-actual_return;
            metrics.sq+=err*err;metrics.baseline_sq+=actual_return*actual_return;
            const double pe=p.predicted_future_price-it->second;
            metrics.price_sq+=pe*pe;
            ++metrics.n;
        }
    });
    if(!replay.validation.passed || frames==0 || non5==0 || consecutive==0 ||
       ready+unavailable!=assignments || metrics.n==0)
        throw std::runtime_error("D8 OOS validation failed");
    std::cout<<std::setprecision(9)
        <<"manager=DevManager_001 universe=10 top_k=5 cadence_minutes=1 horizon_minutes=5\n"
        <<"training_labels="<<fitted.training_labels
        <<" excluded_missing_future="<<fitted.excluded_missing_future
        <<" excluded_crossing_split="<<fitted.excluded_crossing_split<<"\n"
        <<"oos_frames="<<frames<<" selected_assignments="<<assignments
        <<" forecast_ready="<<ready<<" forecast_unavailable="<<unavailable
        <<" consecutive_1m_pairs="<<consecutive<<" non_5m_boundaries="<<non5<<"\n"
        <<"selected_oos_labels="<<metrics.n
        <<" selected_return_rmse="<<std::sqrt(metrics.sq/metrics.n)
        <<" selected_zero_return_baseline_rmse="<<std::sqrt(metrics.baseline_sq/metrics.n)
        <<" selected_price_rmse_rupees="<<std::sqrt(metrics.price_sq/metrics.n)<<"\n"
        <<"PASS 5C-3I-D8 Top-5 dual forecast chronological OOS replay (RESEARCH ONLY; NOT TRADING)\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL 5C-3I-D8 Top-5: "<<e.what()<<'\n';return 1;}
}
