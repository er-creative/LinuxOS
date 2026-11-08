#include "devai/strategy/dev_ai_strategy/DevPositionAwareRL_D9B.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace devai::strategy {
namespace {
int minute(std::int64_t k) { return static_cast<int>((k%1440+1440)%1440); }
std::int64_t session(std::int64_t k) { return (k-1)/1440; }
int index(DevD9AAction a) { return static_cast<int>(a); }
int sign(DevD9APosition p) { return p==DevD9APosition::LONG ? 1 : p==DevD9APosition::SHORT ? -1 : 0; }
bool finite(double x) { return std::isfinite(x); }
int bucket(double x, double scale) { return x < -scale ? -2 : x < -0.2*scale ? -1 : x > scale ? 2 : x > 0.2*scale ? 1 : 0; }
}
DevD9BEnvironment::DevD9BEnvironment(DevD9BConfig c) : cfg_(c) {
    if (!cfg_.permissions.intraday_only || cfg_.permissions.allow_overnight ||
        cfg_.permissions.force_exit_minutes != 915 ||
        cfg_.permissions.new_entry_cutoff_minutes > cfg_.permissions.force_exit_minutes ||
        !finite(cfg_.round_trip_cost_bps) || cfg_.round_trip_cost_bps < 0 ||
        !finite(cfg_.max_abs_one_minute_return) || cfg_.max_abs_one_minute_return <= 0 ||
        cfg_.max_abs_one_minute_return >= 1)
        throw std::invalid_argument("D9B invalid intraday/cost configuration");
}
void DevD9BEnvironment::reset() { states_.clear(); }
DevD9BPosition DevD9BEnvironment::position(const std::string& s) const {
    auto it=states_.find(s); return it==states_.end() ? DevD9BPosition{} : it->second.position;
}
std::vector<DevD9AAction> DevD9BEnvironment::legalActions(const DevD9BObservation& o) const {
    if (o.symbol.empty() || o.decision_key<=0 || !finite(o.close) || o.close<=0)
        throw std::invalid_argument("D9B invalid observation");
    const auto p=position(o.symbol).side;
    if (minute(o.decision_key)>=cfg_.permissions.force_exit_minutes)
        return {p==DevD9APosition::FLAT ? DevD9AAction::HOLD_FLAT : DevD9AAction::FORCE_EXIT};
    if (p==DevD9APosition::LONG) return {DevD9AAction::HOLD_LONG,DevD9AAction::CLOSE_LONG};
    if (p==DevD9APosition::SHORT) return {DevD9AAction::HOLD_SHORT,DevD9AAction::CLOSE_SHORT};
    std::vector<DevD9AAction> a{DevD9AAction::HOLD_FLAT};
    if (minute(o.decision_key)<cfg_.permissions.new_entry_cutoff_minutes &&
        (!cfg_.enable_predicted_forward_return || o.forecast_ready)) {
        if (cfg_.permissions.enable_long) a.push_back(DevD9AAction::OPEN_LONG);
        if (cfg_.permissions.enable_short && o.shortable) a.push_back(DevD9AAction::OPEN_SHORT);
    }
    return a;
}
DevD9BStep DevD9BEnvironment::step(const DevD9BObservation& o, DevD9AAction requested) {
    auto legal=legalActions(o);
    if (std::find(legal.begin(),legal.end(),requested)==legal.end())
        throw std::invalid_argument("D9B action prohibited by position/permissions/time");
    if ((cfg_.enable_predicted_forward_return && !finite(o.predicted_forward_return))||!finite(o.zscore_1m)||!finite(o.zscore_5m)||!finite(o.zscore_15m))
        throw std::invalid_argument("D9B non-finite causal feature");
    auto& s=states_[o.symbol];
    if (s.last_key) {
        if (o.decision_key<=s.last_key) throw std::invalid_argument("D9B duplicate/backward minute");
        if (session(o.decision_key)!=session(s.last_key) && s.position.side!=DevD9APosition::FLAT)
            throw std::runtime_error("D9B overnight exposure: missing mandatory exit");
    }
    DevD9BStep out;
    if (s.last_key && session(o.decision_key)==session(s.last_key) && s.position.side!=DevD9APosition::FLAT) {
        if (o.decision_key != s.last_key+1)
            throw std::runtime_error("D9B open position across missing 1m bars; cannot settle safely");
        const double r=o.close/s.last_close-1.0;
        if (!finite(r)||std::abs(r)>cfg_.max_abs_one_minute_return)
            throw std::runtime_error("D9B implausible 1m return");
        out.reward=sign(s.position.side)*r;
    }
    const double half_cost=cfg_.round_trip_cost_bps/20000.0;
    if (requested==DevD9AAction::OPEN_LONG || requested==DevD9AAction::OPEN_SHORT) {
        s.position={requested==DevD9AAction::OPEN_LONG?DevD9APosition::LONG:DevD9APosition::SHORT,o.close,o.decision_key};
        out.total_cost=half_cost;
    } else if (requested==DevD9AAction::CLOSE_LONG || requested==DevD9AAction::CLOSE_SHORT || requested==DevD9AAction::FORCE_EXIT) {
        s.position={}; out.total_cost=half_cost;
    }
    out.reward-=out.total_cost;
    s.last_close=o.close; s.last_key=o.decision_key;
    out.action=requested; out.position_after=s.position.side;
    out.terminal=minute(o.decision_key)>=cfg_.permissions.force_exit_minutes;
    out.reason=out.terminal ? "15:15 mandatory session termination" : "causal close-to-close mark-to-market";
    return out;
}
DevD9BSharedQModel::DevD9BSharedQModel(DevD9BConfig c):cfg_(c),rng_state_(c.seed?c.seed:17) {
    if (c.enable_predicted_forward_return &&
        (!finite(c.forecast_inner_abs)||!finite(c.forecast_outer_abs)||
         c.forecast_inner_abs<=0||c.forecast_outer_abs<=c.forecast_inner_abs))
        throw std::invalid_argument("D9D forecast mode requires training-calibrated thresholds");
    if (!finite(c.q_alpha)||c.q_alpha<=0||c.q_alpha>1||!finite(c.q_gamma)||c.q_gamma<0||c.q_gamma>1||
        !finite(c.epsilon)||c.epsilon<0||c.epsilon>1) throw std::invalid_argument("D9B invalid Q hyperparameters");
}
double DevD9BSharedQModel::random01() {
    rng_state_^=rng_state_<<13; rng_state_^=rng_state_>>7; rng_state_^=rng_state_<<17;
    return (rng_state_>>11)*(1.0/9007199254740992.0);
}
std::string DevD9BSharedQModel::stateKey(const DevD9BObservation& o, DevD9APosition p) const {
    return key(o,p);
}
std::string DevD9BSharedQModel::key(const DevD9BObservation& o, DevD9APosition p) const {
    std::ostringstream ss;
    ss<<static_cast<int>(p)<<':';
    if (cfg_.enable_predicted_forward_return) {
        if (!o.forecast_ready || !finite(o.predicted_forward_return))
            ss<<"NA";
        else {
            const double x=o.predicted_forward_return;
            // Seven signed bins: zero is never conflated with positive or negative.
            const int b=x < -cfg_.forecast_outer_abs ? -3 :
                        x < -cfg_.forecast_inner_abs ? -2 :
                        x < 0 ? -1 : x == 0 ? 0 :
                        x <= cfg_.forecast_inner_abs ? 1 :
                        x <= cfg_.forecast_outer_abs ? 2 : 3;
            ss<<b;
        }
        ss<<':';
    }
    ss<<bucket(o.zscore_1m,1.0)<<':'
      <<bucket(o.zscore_5m,1.0)<<':'
      <<bucket(o.zscore_15m,1.0)<<':'<<(minute(o.decision_key)>=900?1:0);
    return ss.str();
}
double DevD9BSharedQModel::value(const std::string& k, DevD9AAction a) const {
    auto it=q_.find(k); return it==q_.end()?0.0:it->second.at(index(a));
}
DevD9AAction DevD9BSharedQModel::choose(const DevD9BObservation& o, DevD9APosition p,
    const std::vector<DevD9AAction>& legal, bool explore) {
    if (legal.empty()) throw std::invalid_argument("D9B empty action mask");
    if (explore && random01()<cfg_.epsilon) return legal[static_cast<std::size_t>(random01()*legal.size())%legal.size()];
    const auto k=key(o,p);
    DevD9AAction best=legal.front(); double score=value(k,best);
    for (auto a:legal) if (value(k,a)>score) {best=a;score=value(k,a);}
    return best;
}
void DevD9BSharedQModel::learn(const DevD9BObservation& o, DevD9APosition p,
    DevD9AAction a, double reward, const DevD9BObservation& next, DevD9APosition np,
    const std::vector<DevD9AAction>& next_legal, bool terminal) {
    if (!finite(reward)) throw std::invalid_argument("D9B non-finite reward");
    const auto k=key(o,p);
    double future=0;
    if (!terminal) {
        if (next_legal.empty()) throw std::invalid_argument("D9B empty next action mask");
        const auto nk=key(next,np);
        future=value(nk,next_legal.front());
        for (auto na:next_legal) future=std::max(future,value(nk,na));
    }
    auto& q=q_[k][index(a)]; q+=cfg_.q_alpha*(reward+cfg_.q_gamma*future-q);
}
void DevD9BSharedQModel::save(const std::string& path) const {
    std::ofstream f(path); if (!f) throw std::runtime_error("D9D cannot save model");
    f<<"CETE_D9D_Q_V2\n"<<std::setprecision(17)
     <<(cfg_.enable_predicted_forward_return?1:0)<<' '
     <<cfg_.forecast_inner_abs<<' '<<cfg_.forecast_outer_abs<<'\n'
     <<q_.size()<<'\n';
    for(const auto& kv:q_) {f<<kv.first;for(double x:kv.second)f<<' '<<x;f<<'\n';}
    if(!f)throw std::runtime_error("D9D model write failed");
}
void DevD9BSharedQModel::load(const std::string& path) {
    std::ifstream f(path);std::string magic;int enabled=-1;double inner=0,outer=0;
    std::size_t count=0;
    if(!(f>>magic)||magic!="CETE_D9D_Q_V2" ||
       !(f>>enabled>>inner>>outer)||!(f>>count)||count>10000000||
       (enabled!=0&&enabled!=1)||!finite(inner)||!finite(outer))
        throw std::runtime_error("D9D invalid or incompatible model header (V1 rejected)");
    if ((enabled==1)!=cfg_.enable_predicted_forward_return ||
        inner!=cfg_.forecast_inner_abs || outer!=cfg_.forecast_outer_abs)
        throw std::runtime_error("D9D model observation mode/threshold mismatch");
    std::unordered_map<std::string,Values> tmp;
    for(std::size_t i=0;i<count;++i){
        std::string k;Values a{};
        if(!(f>>k))throw std::runtime_error("D9D truncated model");
        for(double& x:a)if(!(f>>x)||!finite(x))throw std::runtime_error("D9D invalid Q value");
        if(!tmp.emplace(k,a).second)throw std::runtime_error("D9D duplicate model key");
    }
    std::string trailing;
    if(f>>trailing)throw std::runtime_error("D9D unexpected trailing model data");
    q_=std::move(tmp);
}
} // namespace devai::strategy
