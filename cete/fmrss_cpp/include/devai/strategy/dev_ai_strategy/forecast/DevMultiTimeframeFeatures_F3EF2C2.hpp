#pragma once
#include "devai/strategy/dev_ai_strategy/forecast/DevRealMarketForwardReturns_F3EF1B.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>
#include <unordered_set>

namespace devai::forecast::f3ef2c2 {
using MinuteClose=devai::strategy::f3ef1::MinuteClose;
namespace clock=devai::strategy::f3ef1b;
struct Bar {std::int64_t start=0,completed=0; int session=0; double close=0;};
struct Context {
    std::int64_t decision=0;
    double ret5=0,ret15=0,mean5_4=0,std5_4=0,z5_4=0;
    double mean15_2=0,std15_2=0,z15_2=0;
    double volatility20=0,mean60=0,std60=0,z60=0;
    double nifty_ret5=0,nifty_ret15=0,relative_ret5=0,relative_ret15=0;
    int age5=0,age15=0;
};
inline bool good(double x){return std::isfinite(x)&&x>0;}
inline std::pair<double,double> stats(const std::deque<double>& w){
    if(w.empty())throw std::runtime_error("empty rolling window");
    double m=0,v=0;for(double x:w)m+=x;m/=w.size();for(double x:w)v+=(x-m)*(x-m);
    return {m,std::sqrt(v/w.size())};
}
inline std::vector<Bar> completed_bars(const std::vector<MinuteClose>& raw,int minutes){
    if(minutes!=5&&minutes!=15)throw std::invalid_argument("timeframe must be 5 or 15");
    const auto bars=clock::adapt_candles(raw);
    std::vector<Bar> out;int count=0,session=-1;std::int64_t expected=0,start=0;double close=0;
    for(const auto& b:bars){
        if(!good(b.close))throw std::runtime_error("invalid candle close");
        const int md=clock::ist_minute_of_day(b.epoch_seconds);
        const int offset=md-555;
        if(b.session_id!=session){session=b.session_id;count=0;expected=0;}
        // Every block begins at 09:15 + n*timeframe. A gap invalidates the block.
        if(offset%minutes==0){count=0;start=b.epoch_seconds;}
        if(count==0 && offset%minutes!=0)continue;
        if(count>0 && b.epoch_seconds!=expected){count=0;continue;}
        ++count;expected=b.epoch_seconds+60;close=b.close;
        if(count==minutes){out.push_back({start,b.epoch_seconds+60,session,close});count=0;}
    }
    return out;
}
struct Partial {double ret=0,mean=0,sd=0,z=0;bool valid=false;};
inline std::map<std::int64_t,Partial> build_tf(const std::vector<MinuteClose>& raw,int tf,int rolling){
    const auto bars=completed_bars(raw,tf);
    std::map<std::int64_t,Partial> result;std::deque<double> w;int session=-1;std::int64_t previous_end=0;double previous_close=0;
    for(const auto& b:bars){
        if(session!=b.session || (previous_end && b.completed-previous_end!=tf*60)){
            w.clear();previous_close=0;
        }
        Partial p;p.ret=previous_close>0?b.close/previous_close-1:0;
        w.push_back(b.close);if(static_cast<int>(w.size())>rolling)w.pop_front();
        if(static_cast<int>(w.size())==rolling && previous_close>0){
            auto [m,s]=stats(w);p.mean=m;p.sd=s;p.z=s>1e-12?(b.close-m)/s:0;p.valid=true;
        }
        result.emplace(b.completed,p);session=b.session;previous_end=b.completed;previous_close=b.close;
    }
    return result;
}
// Uses only bars whose COMPLETION timestamp is <= decision timestamp.
// Returns nullopt until the 5m and 15m rolling contexts are both available.
inline std::map<std::int64_t,Context> build_context(const std::vector<MinuteClose>& stock,
                                                   const std::vector<MinuteClose>& nifty){
    const auto bars=clock::adapt_candles(stock);
    const auto benchmark=clock::adapt_candles(nifty);
    std::unordered_set<std::int64_t> nifty_epochs;
    for(const auto& b:benchmark)nifty_epochs.insert(b.epoch_seconds);
    const auto s5=build_tf(stock,5,4),s15=build_tf(stock,15,2);
    const auto n5=build_tf(nifty,5,4),n15=build_tf(nifty,15,2);
    std::map<std::int64_t,Context> result;
    auto a5=s5.begin(),a15=s15.begin(),b5=n5.begin(),b15=n15.begin();
    std::optional<std::pair<std::int64_t,Partial>> x5,x15,y5,y15;
    std::deque<double> w60,returns20;int session=-1;std::int64_t prev=0;double prev_close=0;
    for(const auto& b:bars){
        if(session!=b.session_id || (prev && b.epoch_seconds-prev!=60)){
            w60.clear();returns20.clear();prev_close=0;
        }
        if(prev_close>0)returns20.push_back(b.close/prev_close-1);
        if(returns20.size()>20)returns20.pop_front();
        w60.push_back(b.close);if(w60.size()>60)w60.pop_front();
        const auto decision=b.epoch_seconds+60;
        while(a5!=s5.end()&&a5->first<=decision){x5=*a5++;}
        while(a15!=s15.end()&&a15->first<=decision){x15=*a15++;}
        while(b5!=n5.end()&&b5->first<=decision){y5=*b5++;}
        while(b15!=n15.end()&&b15->first<=decision){y15=*b15++;}
        if(nifty_epochs.contains(b.epoch_seconds)&&w60.size()==60&&returns20.size()==20&&x5&&x15&&y5&&y15&&
           x5->second.valid&&x15->second.valid&&y5->second.valid&&y15->second.valid&&
           // Do not carry completed bars across the session or a large staleness gap.
           decision-x5->first>=0&&decision-x5->first<300&&
           decision-x15->first>=0&&decision-x15->first<900&&
           clock::ist_session_id(x5->first-60)==b.session_id&&
           clock::ist_session_id(x15->first-60)==b.session_id&&
           clock::ist_session_id(y5->first-60)==b.session_id&&
           clock::ist_session_id(y15->first-60)==b.session_id){
            auto [m60,sd60]=stats(w60);auto [_,vol]=stats(returns20);
            Context c;c.decision=decision;c.ret5=x5->second.ret;c.ret15=x15->second.ret;
            c.mean5_4=x5->second.mean;c.std5_4=x5->second.sd;c.z5_4=x5->second.z;
            c.mean15_2=x15->second.mean;c.std15_2=x15->second.sd;c.z15_2=x15->second.z;
            c.volatility20=vol;c.mean60=m60;c.std60=sd60;c.z60=sd60>1e-12?(b.close-m60)/sd60:0;
            c.nifty_ret5=y5->second.ret;c.nifty_ret15=y15->second.ret;
            c.relative_ret5=c.ret5-c.nifty_ret5;c.relative_ret15=c.ret15-c.nifty_ret15;
            c.age5=static_cast<int>((decision-x5->first)/60);c.age15=static_cast<int>((decision-x15->first)/60);
            result.emplace(decision,c);
        }
        session=b.session_id;prev=b.epoch_seconds;prev_close=b.close;
    }
    return result;
}
} // namespace devai::forecast::f3ef2c2
