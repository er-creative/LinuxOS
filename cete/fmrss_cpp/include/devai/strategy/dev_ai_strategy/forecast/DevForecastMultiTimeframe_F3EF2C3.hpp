#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace devai::forecast::f3ef2c3 {
constexpr int K=24;
constexpr std::array<int,5> horizons{1,3,5,10,15};
using Vec=std::array<double,K>;
using Mat=std::array<std::array<double,K+1>,K+1>;
inline std::vector<std::string> split(const std::string&s){std::vector<std::string> v;std::stringstream ss(s);std::string t;while(std::getline(ss,t,','))v.push_back(t);return v;}
inline std::map<std::string,size_t> header(const std::string&s){std::map<std::string,size_t> m;auto v=split(s);for(size_t i=0;i<v.size();++i)m[v[i]]=i;return m;}
inline std::string get(const std::vector<std::string>&v,const std::map<std::string,size_t>&m,const std::string&k){auto it=m.find(k);if(it==m.end()||it->second>=v.size())throw std::runtime_error("missing CSV column "+k);return v[it->second];}
inline double num(const std::vector<std::string>&v,const std::map<std::string,size_t>&m,const std::string&k){return std::stod(get(v,m,k));}
struct Row{std::string symbol;long long epoch=0;int session=0;double close=0;Vec x{};std::array<double,5> y{};std::array<bool,5> valid{};};
inline Row parse(const std::string&line,const std::map<std::string,size_t>&m){
 auto v=split(line);Row r;r.symbol=get(v,m,"symbol");r.epoch=std::stoll(get(v,m,"decision_epoch"));r.session=std::stoi(get(v,m,"session_id"));r.close=num(v,m,"close");
 double mean=num(v,m,"mean20"),sd=num(v,m,"std20"),z=num(v,m,"z20"),ret=num(v,m,"return_1m_past"),market=num(v,m,"nifty_return_1m_past");
 if(mean<=0)throw std::runtime_error("invalid mean20");
 double rel=(r.close-mean)/mean,vol=sd/mean;
 r.x={z,ret,market,ret-market,vol,rel,std::abs(z),z*z,z*ret,z*market,std::abs(ret),ret*market,
 num(v,m,"ret_5m_past"),num(v,m,"ret_15m_past"),num(v,m,"z5_4"),num(v,m,"z15_2"),
 num(v,m,"volatility20"),num(v,m,"z60"),num(v,m,"nifty_ret_5m_past"),num(v,m,"nifty_ret_15m_past"),
 num(v,m,"relative_ret_5m"),num(v,m,"relative_ret_15m"),num(v,m,"age5_minutes"),num(v,m,"age15_minutes")};
 if(r.x[22]<0||r.x[22]>4||r.x[23]<0||r.x[23]>14)throw std::runtime_error("stale/inconsistent timeframe age");
 for(size_t h=0;h<5;++h){auto key=std::to_string(horizons[h])+"m";r.valid[h]=get(v,m,"valid_"+key)=="1";if(r.valid[h])r.y[h]=num(v,m,"target_return_"+key);}return r;
}
inline bool finite(const Row&r){if(!(std::isfinite(r.close)&&r.close>0))return false;for(double x:r.x)if(!std::isfinite(x))return false;return true;}
struct Cost{double brokerage=0,stt=0,exchange=0,sebi=0,gst=0,stamp=0,total=0;};
inline Cost zerodha_nse(double buy,double sell,int qty,bool round_stt=true){if(buy<=0||sell<=0||qty<=0)throw std::invalid_argument("invalid order");double b=buy*qty,s=sell*qty,t=b+s;Cost c;c.brokerage=std::min(20.,b*.0003)+std::min(20.,s*.0003);c.stt=s*.00025;if(round_stt)c.stt=std::floor(c.stt+.5);c.exchange=t*.0000307;c.sebi=t*.000001;c.stamp=b*.00003;c.gst=.18*(c.brokerage+c.exchange+c.sebi);c.total=c.brokerage+c.stt+c.exchange+c.sebi+c.stamp+c.gst;return c;}
struct Stats{std::array<double,K> sum{},sq{};uint64_t n=0;void add(const Vec&x){++n;for(int j=0;j<K;++j){sum[j]+=x[j];sq[j]+=x[j]*x[j];}}};
struct Scale{Vec mean{},sd{};void fit(const Stats&s){if(!s.n)throw std::runtime_error("empty train");for(int j=0;j<K;++j){mean[j]=s.sum[j]/s.n;sd[j]=std::sqrt(std::max(0.,s.sq[j]/s.n-mean[j]*mean[j]));if(sd[j]<1e-12)sd[j]=1;}}Vec apply(const Vec&x)const{Vec z{};for(int j=0;j<K;++j)z[j]=std::clamp((x[j]-mean[j])/sd[j],-8.,8.);return z;}};
struct Normal{Mat a{};std::array<double,K+1> b{};uint64_t n=0;void add(const Vec&x,double y){std::array<double,K+1> z{};z[0]=1;for(int j=0;j<K;++j)z[j+1]=x[j];++n;for(int i=0;i<=K;++i){b[i]+=z[i]*y;for(int j=i;j<=K;++j)a[i][j]+=z[i]*z[j];}}};
inline std::array<double,K+1> solve(Normal n,double lambda){for(int i=0;i<=K;++i)for(int j=0;j<i;++j)n.a[i][j]=n.a[j][i];if(!n.n)throw std::runtime_error("empty target");for(int i=1;i<=K;++i)n.a[i][i]+=lambda*n.n;for(int i=0;i<=K;++i){int p=i;for(int j=i+1;j<=K;++j)if(std::abs(n.a[j][i])>std::abs(n.a[p][i]))p=j;std::swap(n.a[i],n.a[p]);std::swap(n.b[i],n.b[p]);if(std::abs(n.a[i][i])<1e-15)throw std::runtime_error("singular regression");double d=n.a[i][i];for(int j=i;j<=K;++j)n.a[i][j]/=d;n.b[i]/=d;for(int k=0;k<=K;++k)if(k!=i){double f=n.a[k][i];for(int j=i;j<=K;++j)n.a[k][j]-=f*n.a[i][j];n.b[k]-=f*n.b[i];}}return n.b;}
inline double predict(const std::array<double,K+1>&w,const Vec&x){double p=w[0];for(int j=0;j<K;++j)p+=w[j+1]*x[j];return p;}
struct Metric{uint64_t n=0;double se=0,ae=0,base_se=0;void add(double p,double y,double base){++n;se+=(p-y)*(p-y);ae+=std::abs(p-y);base_se+=(base-y)*(base-y);}double rmse()const{return n?std::sqrt(se/n):0;}double baseline()const{return n?std::sqrt(base_se/n):0;}};
struct Model{Scale scale;std::array<std::array<double,K+1>,5> w{};std::array<double,5> lambda{};std::array<double,5> base{};std::array<double,5> predict(const Row&r)const{auto x=scale.apply(r.x);std::array<double,5> out{};for(int h=0;h<5;++h)out[h]=f3ef2c3::predict(w[h],x);return out;}void save(const std::string&path)const{std::ofstream o(path);if(!o)throw std::runtime_error("cannot save model");o<<std::setprecision(17)<<"F3EF2C3_RIDGE_V1\n";for(double x:scale.mean)o<<x<<' ';o<<'\n';for(double x:scale.sd)o<<x<<' ';o<<'\n';for(int h=0;h<5;++h){o<<horizons[h]<<' '<<lambda[h]<<' '<<base[h];for(double x:w[h])o<<' '<<x;o<<'\n';}}  static Model load(const std::string&path){std::ifstream in(path);if(!in)throw std::runtime_error("cannot load model: "+path);std::string magic;in>>magic;if(magic!="F3EF2C3_RIDGE_V1")throw std::runtime_error("unexpected model version");Model m;for(auto &x:m.scale.mean)in>>x;for(auto &x:m.scale.sd)in>>x;for(int h=0;h<5;++h){int hz=0;in>>hz>>m.lambda[h]>>m.base[h];if(hz!=horizons[h])throw std::runtime_error("model horizon mismatch");for(auto &x:m.w[h])in>>x;}if(!in)throw std::runtime_error("truncated model");return m;}
};
} // namespace
