#pragma once
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace devai::strategy::d9f3a {
using Key=std::int64_t;
struct Observation {double close=0;double predicted_return=0;bool ready=false;};
struct ForwardAudit {
 double close=0;
 std::optional<double> predicted_price,predicted_return,actual_price,actual_return,error_return;
 bool horizon_available=false;
};
// Deliberately keyed by (session, symbol, minute): no overnight forward targets.
class ForwardIndex {
 std::map<std::tuple<Key,std::string,Key>,Observation> data_;
public:
 void insert(Key day,const std::string& symbol,Key key,Observation obs){
  if(!(std::isfinite(obs.close)&&obs.close>0&&std::isfinite(obs.predicted_return)))
   throw std::invalid_argument("invalid forward-audit observation");
  if(!data_.emplace(std::make_tuple(day,symbol,key),obs).second)
   throw std::runtime_error("duplicate forward-audit observation");
 }
 std::optional<Observation> lookup(Key day,const std::string& symbol,Key key)const{
  const auto it=data_.find(std::make_tuple(day,symbol,key));
  if(it==data_.end())return std::nullopt;
  return it->second;
 }
 ForwardAudit evaluate(Key day,const std::string& symbol,Key key,double close,
                       bool forecast_ready,double predicted_return)const{
  if(!(std::isfinite(close)&&close>0))throw std::invalid_argument("invalid close");
  ForwardAudit a;a.close=close;
  if(forecast_ready){
   if(!std::isfinite(predicted_return))throw std::invalid_argument("invalid prediction");
   a.predicted_return=predicted_return;
   a.predicted_price=close*(1.0+predicted_return);
  }
  // A five-bar forward return requires the SAME symbol, SAME session and
  // EXACTLY five subsequent one-minute decisions, not merely a later row.
  const auto later=lookup(day,symbol,key+5);
  if(later){
   a.horizon_available=true;
   a.actual_price=later->close;
   a.actual_return=later->close/close-1.0;
   if(a.predicted_return)a.error_return=*a.predicted_return-*a.actual_return;
  }
  return a;
 }
};
template<class Sessions>
ForwardIndex buildIndex(const Sessions& sessions,Key train,Key validation,Key oos){
 ForwardIndex index;
 for(const auto& [day,rows]:sessions){
  const Key day_key=day*1440;
  if(day_key<train||day_key>=oos)continue;
  for(const auto& r:rows){
   const auto& o=r.obs;
   index.insert(day,o.symbol,o.decision_key,{o.close,o.predicted_forward_return,o.forecast_ready});
  }
 }
 return index;
}
inline void writeOptional(std::ostream& out,const std::optional<double>& v){
 if(v)out<<*v;
}
inline void writeAuditValues(std::ostream& out,const ForwardAudit& a){
 out<<a.close<<',';
 writeOptional(out,a.predicted_price);out<<',';
 writeOptional(out,a.predicted_return);out<<',';
 writeOptional(out,a.actual_price);out<<',';
 writeOptional(out,a.actual_return);out<<',';
 writeOptional(out,a.error_return);out<<','<<int(a.horizon_available);
}
}
