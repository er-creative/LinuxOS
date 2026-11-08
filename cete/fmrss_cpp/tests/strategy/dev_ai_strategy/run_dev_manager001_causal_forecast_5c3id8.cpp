#include "devai/strategy/dev_ai_strategy/DevCausalForwardReturnD8.hpp"
#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <cmath>
#include <cstdint>
#include <exception>
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
struct Eval {std::size_t n=0;double sq=0,abs=0,base_sq=0,sign_ok=0;};
}
int main(int argc,char** argv){
    try {
        if(argc!=23){std::cerr<<"Usage: "<<argv[0]<<" OOS_START_YYYY-MM-DD NIFTY_FILE SYMBOL FILE [x10]\n";return 2;}
        const Key split=oosKey(argv[1]);
        std::vector<DevHistoricalMinuteInput> inputs{{"NIFTY%2050",argv[2]}};
        std::set<std::string> names;
        for(int i=3;i<argc;i+=2){std::string symbol=argv[i];if(symbol.empty()||symbol=="NIFTY%2050"||!names.insert(symbol).second)throw std::invalid_argument("Exactly 10 unique tradable stocks required");inputs.push_back({symbol,argv[i+1]});}
        if(names.size()!=10)throw std::invalid_argument("10 stocks required");
        DevHistoricalFeedConfig cfg;cfg.manager_id="DevManager_001";cfg.benchmark_symbol="NIFTY%2050";
        cfg.sessions.session_overrides["2025-10-21"]={13*60+45,14*60+45};
        DevRealHistoricalMultiStockFeed feed(cfg);
        std::vector<DevD8Observation> observations;
        // OFFLINE data preparation. Future closes only enter label creation and post-hoc evaluation.
        const auto feed_report=feed.replay(inputs,[&](const DevHistoricalManagerFrame& f){
            for(const auto& s:f.stocks) observations.push_back(devD8MakeObservation(s,f));
        });
        if(!feed_report.validation.passed)throw std::runtime_error("D4 validation failed");
        DevD8TrainingConfig training;training.oos_start_key=split;training.horizon_minutes=5;
        training.ridge_lambda=1.0;training.minimum_training_labels=100;
        DevCausalForwardReturnD8 model(training);
        const auto fit=model.fitOffline(observations);
        // Evaluate all OOS forecasts, including negative forecasts, independently of ranking gates.
        std::map<std::string,std::map<Key,const DevD8Observation*>> lookup;
        for(const auto& o:observations)lookup[o.symbol].emplace(o.decision_key,&o);
        Eval metrics;
        for(const auto& o:observations){
            if(o.decision_key<split)continue;
            const Key target=o.decision_key+training.horizon_minutes;
            if(!devD8SameSession(o.decision_key,target))continue;
            auto it=lookup[o.symbol].find(target);
            if(it==lookup[o.symbol].end())continue;
            const double actual=it->second->close/o.close-1.0;
            // Use only the frozen fitted coefficients and current observation features.
            double prediction=0;const auto& b=model.coefficients();
            for(int i=0;i<4;++i)prediction+=b[i]*o.x[i];
            metrics.sq+=(prediction-actual)*(prediction-actual);
            metrics.abs+=std::abs(prediction-actual);
            metrics.base_sq+=actual*actual;
            metrics.sign_ok+=(prediction>=0)==(actual>=0)?1.0:0.0;
            ++metrics.n;
        }
        if(metrics.n==0)throw std::runtime_error("No OOS labels available for evaluation");
        DevD7StrategyConfig strategy_config;strategy_config.manager_id="DevManager_001";
        strategy_config.min_expected_return=0.0;
        DevManager001StatisticalStrategyD7 strategy(strategy_config,
            [&model](const DevHistoricalStockFrame& s,const DevHistoricalManagerFrame& f){return model.predict(s,f);});
        DevManager001HistoricalStrategyD7B manager({"DevManager_001",5,1},strategy);
        std::size_t oos_decisions=0,non5=0,consecutive=0,selected=0;Key previous=0;
        const auto replay=manager.replay(feed,inputs,[&](const DevManager001RankingFrame& frame){
            if(frame.decision_key<split)return;
            if(previous && frame.decision_key<=previous)throw std::runtime_error("D8 non-monotonic decision keys");
            if(previous && frame.decision_key==previous+1)++consecutive;
            previous=frame.decision_key;++oos_decisions;
            if(frame.decision_key%5!=0)++non5;
            if(frame.evaluated!=10||frame.selected.size()>5)throw std::runtime_error("D8 manager universe/Top5 invalid");
            selected+=frame.selected.size();
        });
        if(!replay.feed.validation.passed||oos_decisions==0||consecutive==0||non5==0)
            throw std::runtime_error("D8 one-minute OOS replay validation failed");
        std::cout<<"manager=DevManager_001 universe=10 top_k=5 cadence_minutes=1 horizon_minutes=5\n"
                 <<"training_labels="<<fit.training_labels<<" excluded_missing_future="<<fit.excluded_missing_future
                 <<" excluded_crossing_split="<<fit.excluded_crossing_split<<"\n"
                 <<"oos_decisions="<<oos_decisions<<" consecutive_1m_pairs="<<consecutive
                 <<" non_5m_boundaries="<<non5<<" selected_assignments="<<selected<<"\n"
                 <<"oos_labels="<<metrics.n<<" oos_mae="<<metrics.abs/metrics.n
                 <<" oos_rmse="<<std::sqrt(metrics.sq/metrics.n)
                 <<" zero_baseline_rmse="<<std::sqrt(metrics.base_sq/metrics.n)
                 <<" sign_accuracy="<<metrics.sign_ok/metrics.n<<"\n"
                 <<"PASS 5C-3I-D8 causal frozen forward-return forecast replay (RESEARCH ONLY; NOT TRADING)\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL 5C-3I-D8: "<<e.what()<<'\n';return 1;}
}
