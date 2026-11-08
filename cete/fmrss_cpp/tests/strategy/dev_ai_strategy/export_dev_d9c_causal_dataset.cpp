#include "devai/strategy/dev_ai_strategy/DevManager001Top5ForecastD8.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
using namespace devai::strategy;
namespace {
using Key=std::int64_t;
int digits(const std::string&s,int a,int n){int v=0;for(int i=a;i<a+n;++i){if(s.at(i)<'0'||s.at(i)>'9')throw std::invalid_argument("bad date");v=v*10+s[i]-'0';}return v;}
Key ordinal(int y,unsigned m,unsigned d){y-=m<=2;int era=(y>=0?y:y-399)/400;unsigned yoe=y-era*400;unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;unsigned doe=yoe*365+yoe/4-yoe/100+doy;return static_cast<Key>(era)*146097+doe;}
Key dateKey(const std::string&s){if(s.size()!=10||s[4]!='-'||s[7]!='-')throw std::invalid_argument("date YYYY-MM-DD required");int y=digits(s,0,4),m=digits(s,5,2),d=digits(s,8,2);static const int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};if(y<1900||y>2200||m<1||m>12||d<1||d>days[m]+(m==2&&y%4==0&&(y%100!=0||y%400==0)))throw std::invalid_argument("invalid calendar date");return ordinal(y,m,d)*1440;}
}
int main(int argc,char**argv){
 try{
  if(argc!=24)throw std::invalid_argument("Usage: export_dev_d9c_causal_dataset OUTPUT.csv D8_FIT_END_YYYY-MM-DD NIFTY_FILE SYMBOL FILE [x10]");
  std::string output=argv[1];Key train_start=dateKey(argv[2]);
  std::vector<DevHistoricalMinuteInput> inputs{{"NIFTY%2050",argv[3]}};std::set<std::string> universe;
  for(int i=4;i<argc;i+=2){std::string sym=argv[i];if(sym.empty()||sym=="NIFTY%2050"||!universe.insert(sym).second)throw std::invalid_argument("ten distinct stock symbols required");inputs.push_back({sym,argv[i+1]});}
  if(universe.size()!=10)throw std::invalid_argument("exactly ten symbols required");
  DevHistoricalFeedConfig cfg;cfg.manager_id="DevManager_001";cfg.benchmark_symbol="NIFTY%2050";cfg.sessions.session_overrides["2025-10-21"]={13*60+45,14*60+45};
  DevRealHistoricalMultiStockFeed feed(cfg);
  std::vector<DevD8Observation> fitting;
  const auto pre=feed.replay(inputs,[&](const DevHistoricalManagerFrame&f){if(f.decision_key<train_start)for(const auto&s:f.stocks)fitting.push_back(devD8MakeObservation(s,f));});
  if(!pre.validation.passed||fitting.empty())throw std::runtime_error("D8 fitting replay failed");
  DevD8TrainingConfig tc;tc.oos_start_key=train_start;tc.horizon_minutes=5;tc.ridge_lambda=1.0;tc.minimum_training_labels=100;
  DevCausalForwardReturnD8 model(tc);auto fit=model.fitOffline(fitting);fitting.clear();fitting.shrink_to_fit();
  DevManager001HistoricalRanking ranker({"DevManager_001",5},devD8StatisticalRankingPolicy);
  DevManager001Top5ForecastD8 engine(ranker,model,5);
  std::ofstream out(output);if(!out)throw std::runtime_error("cannot create CSV");out.precision(17);
  out<<"symbol,decision_key,close,predicted_forward_return,zscore_1m,zscore_5m,zscore_15m,forecast_ready,shortable,selected_top5\n";
  std::size_t rows=0,frames=0,selected=0;
  auto rep=feed.replay(inputs,[&](const DevHistoricalManagerFrame&f){
   if(f.decision_key<train_start)return;
   auto chosen=engine.evaluate(f);
   if(chosen.evaluated!=10)throw std::runtime_error("not all ten stocks evaluated");
   for(const auto&s:f.stocks){
    const DevD8SelectedForecast* p=nullptr;
    for(const auto&c:chosen.selected)if(c.symbol==s.symbol){p=&c;break;}
    bool ready=p&&p->ready;
    // Nonselected symbols are still emitted so existing positions can be monitored.
    out<<s.symbol<<','<<f.decision_key<<','<<s.one_minute.close<<','<<(ready?p->predicted_forward_return:0.0)
       <<','<<s.close_1m.zscore<<','<<s.close_5m.zscore<<','<<s.close_15m.zscore
       <<','<<(ready?1:0)<<",0,"<<(p?1:0)<<'\n';
    ++rows;if(p)++selected;
   }
   ++frames;
  });
  if(!rep.validation.passed||!out||frames==0||rows!=frames*10)throw std::runtime_error("D9C export validation failed");
  std::cout<<"D8_fit_labels="<<fit.training_labels<<" export_frames="<<frames<<" export_rows="<<rows<<" top5_assignments="<<selected<<"\n";
  std::cout<<"PASS D9C causal ten-stock dataset export (not RL training)\n";
 }catch(const std::exception&e){std::cerr<<"FAIL D9C exporter: "<<e.what()<<'\n';return 1;}
}
