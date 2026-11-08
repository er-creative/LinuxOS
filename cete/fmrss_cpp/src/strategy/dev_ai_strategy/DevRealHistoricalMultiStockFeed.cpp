#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockFeed.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
namespace {
using Key = std::int64_t;
struct SymbolData { std::map<Key,DevHistoricalCandle> m1,m5,m15; };
int digit(char c) { return c >= '0' && c <= '9' ? c-'0' : -1; }
int parsePart(const std::string& s, std::size_t begin, std::size_t count) {
    int v=0;
    for (std::size_t i=begin;i<begin+count;++i) { const int d=digit(s[i]); if(d<0) throw std::runtime_error("Bad date/time digits"); v=v*10+d; }
    return v;
}
bool leap(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
Key dayOrdinal(int y,unsigned m,unsigned d) {
    y-=m<=2; const int era=(y>=0?y:y-399)/400;
    const unsigned yoe=static_cast<unsigned>(y-era*400);
    const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    return static_cast<Key>(era)*146097+doe;
}
Key parseKey(const std::string& date,const std::string& time) {
    if(date.size()!=10 || date[4]!='-' || date[7]!='-' || (time.size()!=5 && time.size()!=8) || time[2]!=':' || (time.size()==8 && time[5]!=':')) throw std::runtime_error("Bad date/time format");
    const int y=parsePart(date,0,4),m=parsePart(date,5,2),d=parsePart(date,8,2);
    const int h=parsePart(time,0,2),minute=parsePart(time,3,2);
    const int sec=time.size()==8?parsePart(time,6,2):0;
    static const int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<1900 || y>2200 || m<1 || m>12 || d<1 || d>days[m]+(m==2&&leap(y)) || h>23 || minute>59 || sec!=0) throw std::runtime_error("Invalid date/time");
    return dayOrdinal(y,static_cast<unsigned>(m),static_cast<unsigned>(d))*1440+h*60+minute;
}
std::vector<std::string> split(const std::string& s) {
    std::stringstream in(s); std::vector<std::string> v; std::string cell;
    while(std::getline(in,cell,',')) {
        auto b=cell.find_first_not_of(" \t\r\n"),e=cell.find_last_not_of(" \t\r\n");
        v.push_back(b==std::string::npos?"":cell.substr(b,e-b+1));
    }
    return v;
}
double num(const std::string& s) {
    std::size_t pos=0; const double x=std::stod(s,&pos);
    if(pos!=s.size() || !std::isfinite(x)) throw std::runtime_error("Invalid numeric candle");
    return x;
}
std::pair<int,int> session(const std::string& date,const DevHistoricalFeedConfig& cfg) {
    const auto it=cfg.sessions.session_overrides.find(date);
    return it==cfg.sessions.session_overrides.end()?std::make_pair(cfg.sessions.session_start_minute,cfg.sessions.session_end_minute):it->second;
}
std::map<Key,DevHistoricalCandle> aggregate(const std::map<Key,DevHistoricalCandle>& raw,
                                           const std::map<Key,std::pair<int,int>>& day_sessions,int width) {
    std::map<Key,DevHistoricalCandle> out;
    for(const auto& ds:day_sessions) {
        const Key day=ds.first; const auto bounds=ds.second;
        for(int start=bounds.first;start+width<=bounds.second;start+=width) {
            DevHistoricalCandle bar; bool complete=true;
            for(int i=0;i<width;++i) {
                const auto it=raw.find(day*1440+start+i);
                if(it==raw.end()) {complete=false;break;}
                const auto& c=it->second;
                if(i==0) {bar.open=c.open;bar.high=c.high;bar.low=c.low;}
                else {bar.high=std::max(bar.high,c.high);bar.low=std::min(bar.low,c.low);}
                bar.close=c.close;bar.volume+=c.volume;
            }
            if(complete) {bar.start_key=day*1440+start;bar.completed_key=bar.start_key+width;out.emplace(bar.completed_key,bar);}
        }
    }
    return out;
}
SymbolData load(const DevHistoricalMinuteInput& input,const DevHistoricalFeedConfig& cfg) {
    std::ifstream file(input.file_path);
    if(!file) throw std::runtime_error("Cannot open "+input.file_path);
    SymbolData data; std::map<Key,std::pair<int,int>> days;
    std::string line;
    while(std::getline(file,line)) {
        if(line.empty())continue;
        auto c=split(line);
        if(!c.empty() && c[0]=="Symbol")continue;
        if(c.size()!=8 || c[0]!=input.symbol) throw std::runtime_error("Malformed row or symbol mismatch in "+input.file_path);
        const Key key=parseKey(c[1],c[2]);
        const auto hours=session(c[1],cfg); const int minute=static_cast<int>(key%1440);
        if(minute<hours.first || minute>=hours.second)throw std::runtime_error("Out-of-session row in "+input.file_path);
        DevHistoricalCandle bar; bar.start_key=key;bar.completed_key=key+1;
        bar.open=num(c[3]);bar.high=num(c[4]);bar.low=num(c[5]);bar.close=num(c[6]);bar.volume=num(c[7]);
        if(bar.open<=0 || bar.high<=0 || bar.low<=0 || bar.close<=0 || bar.volume<0 || bar.high<std::max(bar.open,bar.close) || bar.low>std::min(bar.open,bar.close) || bar.high<bar.low)throw std::runtime_error("Invalid OHLC in "+input.file_path);
        if(!data.m1.emplace(bar.completed_key,bar).second)throw std::runtime_error("Duplicate minute in "+input.file_path);
        days.emplace(key/1440,hours);
    }
    data.m5=aggregate(data.m1,days,5);data.m15=aggregate(data.m1,days,15);
    return data;
}
DevHistoricalStats stats(const std::map<Key,DevHistoricalCandle>& bars,Key decision,
                         std::size_t window,std::size_t minimum) {
    DevHistoricalStats out; auto it=bars.upper_bound(decision);
    double sum=0, sq=0; std::vector<double> closes;
    while(it!=bars.begin() && closes.size()<window) {
        --it;closes.push_back(it->second.close);
    }
    out.count=closes.size(); if(out.count<minimum)return out;
    for(const double v:closes)sum+=v;
    out.mean=sum/static_cast<double>(out.count);
    for(const double v:closes) {const double delta=v-out.mean;sq+=delta*delta;}
    out.standard_deviation=std::sqrt(sq/static_cast<double>(out.count));
    out.zscore=out.standard_deviation>0?(closes.front()-out.mean)/out.standard_deviation:0;
    out.ready=true;
    return out;
}
} // namespace
DevRealHistoricalMultiStockFeed::DevRealHistoricalMultiStockFeed(DevHistoricalFeedConfig config):config_(std::move(config)) {
    if(config_.manager_id.empty() || config_.benchmark_symbol.empty() || config_.rolling_window==0 || config_.minimum_history==0 || config_.minimum_history>config_.rolling_window)throw std::invalid_argument("Invalid D5 configuration");
    config_.sessions.benchmark_symbol=config_.benchmark_symbol;
}
DevHistoricalFeedReport DevRealHistoricalMultiStockFeed::replay(const std::vector<DevHistoricalMinuteInput>& inputs,const DevHistoricalManagerConsumer& consumer) const {
    if(!consumer)throw std::invalid_argument("DevManager_001 consumer is required");
    DevHistoricalFeedReport result;
    result.validation=DevHistoricalMinuteValidator(config_.sessions).validate(inputs);
    if(!result.validation.passed) {result.issues=result.validation.issues;return result;}
    std::map<std::string,SymbolData> data;
    for(const auto& input:inputs)data.emplace(input.symbol,load(input,config_));
    const auto benchmark=data.find(config_.benchmark_symbol);
    if(benchmark==data.end())throw std::invalid_argument("Benchmark missing");
    // Every completed benchmark 1m candle is a potential decision boundary.
    // 5m/15m bars are carried forward ONLY from their last completed boundary,
    // and only within the same exchange session. No future candle is accessed.
    for(const auto& entry:benchmark->second.m1) {
        const Key decision=entry.first;
        const Key day=(decision-1)/1440;
        // Never carry a higher-timeframe candle across a trading day.
        DevHistoricalManagerFrame frame;
        frame.manager_id=config_.manager_id;
        frame.decision_key=decision;
        bool complete=true,ready=true;
        for(const auto& input:inputs) {
            if(input.symbol==config_.benchmark_symbol)continue;
            const auto& s=data.at(input.symbol);
            const auto m1=s.m1.find(decision);
            const auto m5_end=s.m5.upper_bound(decision);
            const auto m15_end=s.m15.upper_bound(decision);
            if(m1==s.m1.end() || m5_end==s.m5.begin() || m15_end==s.m15.begin()) {
                complete=false;break;
            }
            const auto m5=std::prev(m5_end),m15=std::prev(m15_end);
            if((m5->second.start_key/1440)!=day ||
               (m15->second.start_key/1440)!=day ||
               decision-m5->first>=5 || decision-m15->first>=15) {
                complete=false;break;
            }
            DevHistoricalStockFrame stock;stock.symbol=input.symbol;
            stock.one_minute=m1->second;
            stock.five_minute=m5->second;
            stock.fifteen_minute=m15->second;
            stock.close_1m=stats(s.m1,decision,config_.rolling_window,config_.minimum_history);
            stock.close_5m=stats(s.m5,decision,config_.rolling_window,config_.minimum_history);
            stock.close_15m=stats(s.m15,decision,config_.rolling_window,config_.minimum_history);
            if(!stock.close_1m.ready || !stock.close_5m.ready || !stock.close_15m.ready)
                ready=false;
            frame.stocks.push_back(std::move(stock));
        }
        if(!complete || frame.stocks.size()!=inputs.size()-1) {
            ++result.skipped_incomplete;continue;
        }
        if(!ready) {++result.skipped_warmup;continue;}
        consumer(frame);++result.eligible_decisions;
    }
    return result;
}
} // namespace devai::strategy
