#include "devai/benchmark/DevHistoricalDecisionRunner.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
namespace devai::benchmark {
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
// UTC epoch minutes converted to IST (UTC+05:30); no system locale dependency.
int minuteIST(std::int64_t epoch){auto m=(epoch/60+330)%1440;return int(m<0?m+1440:m);}
std::int64_t dayIST(std::int64_t epoch){auto n=epoch/60+330;return n>=0?n/1440:(n-1439)/1440;}
void finite(double x){check(std::isfinite(x),"non-finite numeric input");}
}
DevHistoricalDecisionRunner::DevHistoricalDecisionRunner(H43Config c):config_(std::move(c)){
 check(config_.universe.size()==H43_STOCKS,"exactly ten tradable symbols required");
 std::set<std::string> unique(config_.universe.begin(),config_.universe.end());
 check(unique.size()==H43_STOCKS && !unique.contains("NIFTY%2050"),"invalid/duplicate universe");
 for(const auto& s:unique)check(!s.empty(),"empty symbol");
 check(config_.start_minute_ist>=555 && config_.start_minute_ist<config_.last_entry_minute_ist &&
 config_.last_entry_minute_ist<=config_.flat_minute_ist && config_.flat_minute_ist<=930,
 "invalid session limits");
}
std::vector<H43Decision> DevHistoricalDecisionRunner::run(const std::vector<H43Candle>& candles,
 const H43Ranker& ranker,const H43Policy& policy) const {
 check(bool(ranker)&&bool(policy),"missing ranker/model adapter");
 check(!candles.empty(),"no historical candles");
 std::set<std::string> universe(config_.universe.begin(),config_.universe.end());
 std::map<std::int64_t,std::vector<H43Candle>> byTime;
 std::set<std::pair<std::string,std::int64_t>> seen;
 for(const auto& c:candles){
  check(universe.contains(c.symbol),"candle symbol outside universe");
  check(c.start_epoch<c.end_epoch && c.end_epoch-c.start_epoch==60,"expected completed 1-minute candles");
  check(seen.emplace(c.symbol,c.end_epoch).second,"duplicate candle");
  for(double v:{c.open,c.high,c.low,c.close,c.volume})finite(v);
  check(c.open>0&&c.high>0&&c.low>0&&c.close>0&&c.volume>=0&&
    c.high>=std::max(c.open,c.close)&&c.low<=std::min(c.open,c.close)&&c.high>=c.low,
    "invalid OHLCV");
  byTime[c.end_epoch].push_back(c);
 }
 std::vector<H43Decision> result;
 // Per-manager position state; never share portfolio positions.
 std::array<std::string,2> held{};
 std::int64_t previousDay=-1;
 for(const auto& [now,batch]:byTime){
  const auto day=dayIST(now); const int minute=minuteIST(now);
  if(previousDay!=-1 && day!=previousDay)
   check(held[0].empty()&&held[1].empty(),"overnight position: missing 15:15 candle");
  previousDay=day;
  if(minute<config_.start_minute_ist || minute>config_.flat_minute_ist)continue;
  // No partial cross-sectional ranking: all ten symbols must have a completed candle.
  check(batch.size()==H43_STOCKS,"missing symbol candle at decision timestamp");
  std::map<std::string,H43Candle> snapshot;
  for(const auto& c:batch)snapshot.emplace(c.symbol,c);
  for(int si=0;si<2;++si){
   const H43Side side=si==0?H43Side::Long:H43Side::Short;
   auto candidates=ranker(side,now,batch);
   check(candidates.size()==H43_TOPK,"ranker must return five candidates");
   std::set<std::string> names;
   for(const auto& c:candidates){
    check(universe.contains(c.symbol)&&names.insert(c.symbol).second,"invalid top-five ranking");
    finite(c.score);for(double x:c.observation)finite(x);
   }
   for(std::size_t i=1;i<candidates.size();++i)
    check(candidates[i-1].score>=candidates[i].score,"top-five not ranked descending");
   std::array<std::string,H43_TOPK> top{};
   for(std::size_t i=0;i<H43_TOPK;++i)top[i]=candidates[i].symbol;
   auto emit=[&](H43Action a,const std::string& symbol,double score){
    result.push_back({now,side,a,symbol,snapshot.at(symbol).close,score,top});
   };
   if(minute==config_.flat_minute_ist){
    if(!held[si].empty()){emit(H43Action::Exit,held[si],0);held[si].clear();}
    continue;
   }
   if(!held[si].empty()){
    // Evaluate held stock even if it drops outside top five; use a candidate from the
    // current ranker only if present. Otherwise fail closed by exiting.
    auto it=std::find_if(candidates.begin(),candidates.end(),[&](const auto& x){return x.symbol==held[si];});
    if(it==candidates.end()){emit(H43Action::Exit,held[si],0);held[si].clear();continue;}
    const auto action=policy(side,now,*it,true);
    check(action==H43Action::Hold||action==H43Action::Exit||action==H43Action::Wait,
      "OPEN forbidden when already holding");
    if(action==H43Action::Exit){emit(action,held[si],it->score);held[si].clear();}
    else if(action==H43Action::Hold)emit(action,held[si],it->score);
    continue;
   }
   if(minute>config_.last_entry_minute_ist)continue;
   // Manager selection policy: scan ranked candidates; at most one OPEN per manager.
   for(const auto& candidate:candidates){
    const auto action=policy(side,now,candidate,false);
    check(action==H43Action::Open||action==H43Action::Wait,
       "invalid flat-portfolio policy action");
    if(action==H43Action::Open){held[si]=candidate.symbol;emit(action,candidate.symbol,candidate.score);break;}
   }
  }
 }
 check(held[0].empty()&&held[1].empty(),"unclosed positions at replay end");
 return result;
}
void DevHistoricalDecisionRunner::writeCsv(const std::string& path,const std::vector<H43Decision>& rows){
 std::ofstream out(path);check(bool(out),"cannot open output CSV");
 out<<"epoch,manager,action,symbol,observed_close,score,top1,top2,top3,top4,top5\n";
 for(const auto& r:rows){
  const char* action=r.action==H43Action::Open?"OPEN":r.action==H43Action::Hold?"HOLD":r.action==H43Action::Exit?"EXIT":"WAIT";
  out<<r.timestamp<<','<<(r.side==H43Side::Long?"DevManager_001":"DevManager_002")<<','
   <<action<<','<<r.symbol<<','<<r.observed_price<<','<<r.score;
  for(const auto& x:r.top5)out<<','<<x;
  out<<'\n';
 }
 check(bool(out),"CSV write failure");
}
}
