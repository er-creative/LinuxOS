#include "devai/strategy/dev_ai_strategy/DevCausalForwardReturnD8.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
namespace devai::strategy {
namespace {
constexpr int N=4;
// Key is exchange-local day-ordinal*1440 + minute-of-day; completion at 00:00 belongs to prior day.
std::int64_t sessionDay(std::int64_t key) {return (key-1)/1440;}
double clipped(double z) {return std::clamp(z,-5.0,5.0);}
bool finite(const DevD8Features& x) {for(double v:x) if(!std::isfinite(v))return false;return true;}
std::array<double,N> solve(double a[N][N+1]) {
    for(int c=0;c<N;++c) {
        int p=c;
        for(int r=c+1;r<N;++r) if(std::abs(a[r][c])>std::abs(a[p][c]))p=r;
        if(std::abs(a[p][c])<1e-12) throw std::runtime_error("D8 singular training matrix");
        for(int j=c;j<=N;++j)std::swap(a[p][j],a[c][j]);
        const double pivot=a[c][c];
        for(int j=c;j<=N;++j)a[c][j]/=pivot;
        for(int r=0;r<N;++r)if(r!=c){const double v=a[r][c];for(int j=c;j<=N;++j)a[r][j]-=v*a[c][j];}
    }
    std::array<double,N> result{};
    for(int i=0;i<N;++i)result[i]=a[i][N];
    return result;
}
}
bool devD8SameSession(std::int64_t a,std::int64_t b) {
    return a>0 && b>0 && sessionDay(a)==sessionDay(b);
}
DevD8Observation devD8MakeObservation(const DevHistoricalStockFrame& s,
                                      const DevHistoricalManagerFrame& f) {
    const auto key=f.decision_key;
    const auto k5=s.five_minute.completed_key,k15=s.fifteen_minute.completed_key;
    if(f.manager_id!="DevManager_001" || key<=0 || s.symbol.empty() ||
       s.one_minute.completed_key!=key || k5<=0 || k15<=0 ||
       k5>key || k15>key || key-k5>=5 || key-k15>=15 ||
       !devD8SameSession(key,k5) || !devD8SameSession(key,k15) ||
       !s.close_1m.ready || !s.close_5m.ready || !s.close_15m.ready ||
       !(s.one_minute.close>0.0) || !std::isfinite(s.one_minute.close))
        throw std::invalid_argument("D8 invalid causal observation");
    DevD8Observation o{s.symbol,key,s.one_minute.close,
        {1.0,clipped(s.close_1m.zscore),clipped(s.close_5m.zscore),clipped(s.close_15m.zscore)}};
    if(!finite(o.x))throw std::invalid_argument("D8 nonfinite feature");
    return o;
}
DevCausalForwardReturnD8::DevCausalForwardReturnD8(DevD8TrainingConfig c):config_(c) {
    if(c.oos_start_key<=0 || c.horizon_minutes<=0 || c.horizon_minutes>=1440 ||
       !(c.ridge_lambda>0) || !std::isfinite(c.ridge_lambda) || c.minimum_training_labels==0)
        throw std::invalid_argument("D8 invalid training config");
}
DevD8FitReport DevCausalForwardReturnD8::fitOffline(const std::vector<DevD8Observation>& rows) {
    fitted_=false;beta_={};
    std::map<std::string,std::map<std::int64_t,const DevD8Observation*>> by_symbol;
    for(const auto& r:rows) {
        if(r.symbol.empty() || r.decision_key<=0 || !(r.close>0) || !std::isfinite(r.close) ||
           !finite(r.x) || r.x[0]!=1.0) throw std::invalid_argument("D8 invalid training row");
        if(!by_symbol[r.symbol].emplace(r.decision_key,&r).second)
            throw std::invalid_argument("D8 duplicate stock decision key");
    }
    double normal[N][N+1]{};
    DevD8FitReport report;
    for(const auto& [symbol,history]:by_symbol) {
        (void)symbol;
        for(const auto& [key,ptr]:history) {
            if(key>=config_.oos_start_key) continue;
            const auto target_key=key+config_.horizon_minutes;
            if(target_key>=config_.oos_start_key) {++report.excluded_crossing_split;continue;}
            if(!devD8SameSession(key,target_key)) {++report.excluded_missing_future;continue;}
            auto future=history.find(target_key);
            if(future==history.end()) {++report.excluded_missing_future;continue;}
            const double y=future->second->close/ptr->close-1.0;
            if(!std::isfinite(y))throw std::invalid_argument("D8 nonfinite label");
            for(int i=0;i<N;++i){
                for(int j=0;j<N;++j)normal[i][j]+=ptr->x[i]*ptr->x[j];
                normal[i][N]+=ptr->x[i]*y;
            }
            ++report.training_labels;
        }
    }
    if(report.training_labels<config_.minimum_training_labels)
        throw std::runtime_error("D8 insufficient pre-OOS training labels");
    // Ridge regularizes slopes, not intercept. Normalization by N keeps lambda scale stable.
    for(int i=0;i<N;++i){
        for(int j=0;j<=N;++j)normal[i][j]/=static_cast<double>(report.training_labels);
        if(i>0)normal[i][i]+=config_.ridge_lambda;
    }
    beta_=solve(normal);
    for(double v:beta_)if(!std::isfinite(v))throw std::runtime_error("D8 invalid fit");
    fitted_=true;
    return report;
}
DevD7Forecast DevCausalForwardReturnD8::predict(const DevHistoricalStockFrame& stock,
                                                  const DevHistoricalManagerFrame& frame) const {
    if(!fitted_)return {};
    const auto o=devD8MakeObservation(stock,frame);
    double y=0;
    for(int i=0;i<N;++i)y+=beta_[i]*o.x[i];
    if(!std::isfinite(y))throw std::runtime_error("D8 nonfinite forecast");
    return {y,frame.decision_key,true};
}
} // namespace devai::strategy
