#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace devai::forecast::f3ef2b {
constexpr std::array<int,5> H{1,3,5,10,15};
using V5=std::array<double,5>;
struct Model {
 V5 mu{},sd{}; std::array<V5,5> w{}; V5 intercept{}, baseline{};
 static Model load(const std::string& path){
  std::ifstream f(path); if(!f) throw std::runtime_error("Cannot open model: "+path);
  std::string magic; f>>magic; if(magic!="F3EF2_RIDGE_V1") throw std::runtime_error("Unexpected model version");
  Model m; for(int i=0;i<5;++i){if(!(f>>m.mu[i]>>m.sd[i]) || !(m.sd[i]>0)) throw std::runtime_error("Invalid scaler");}
  for(int i=0;i<5;++i){int horizon; double rawIntercept; if(!(f>>horizon>>rawIntercept>>m.intercept[i])) throw std::runtime_error("Invalid model head");
   if(horizon!=H[i]) throw std::runtime_error("Unexpected horizon ordering");
   for(int j=0;j<5;++j) if(!(f>>m.w[i][j])) throw std::runtime_error("Missing weights");
  } return m;
 }
 V5 predict(const V5& x)const{V5 out{};for(int i=0;i<5;++i){double p=intercept[i];for(int j=0;j<5;++j)p+=w[i][j]*(x[j]-mu[j])/sd[j];out[i]=p;}return out;}
};
inline std::vector<std::string> split(const std::string& s){std::vector<std::string> v;std::size_t p=0;while(true){auto q=s.find(',',p);v.push_back(s.substr(p,q==std::string::npos?q:q-p));if(q==std::string::npos)break;p=q+1;}return v;}
inline double num(const std::string& s){if(s.empty())throw std::runtime_error("Empty numeric field");size_t pos=0;double v=std::stod(s,&pos);if(pos!=s.size()||!std::isfinite(v))throw std::runtime_error("Invalid number: "+s);return v;}
struct Record {std::string symbol; long long t{}; int session{}; double z{};V5 pred{},actual{};std::array<bool,5> valid{};};
inline Record parse(const std::vector<std::string>& v,const Model& m){if(v.size()!=20)throw std::runtime_error("F1C CSV schema mismatch (expected 20 columns)");
 Record r;r.symbol=v[0];r.t=std::stoll(v[2]);r.session=std::stoi(v[3]);double close=num(v[4]),mean=num(v[5]),std=num(v[6]);r.z=num(v[7]);
 if(close<=0||mean<=0||std<0)throw std::runtime_error("Bad price/mean/std");
 V5 x{r.z,std/close,num(v[8]),num(v[9]),close/mean-1.0};r.pred=m.predict(x);
 for(int i=0;i<5;++i){r.valid[i]=(v[10+2*i]=="1");if(r.valid[i])r.actual[i]=num(v[11+2*i]);}
 return r;
}
struct Stats {long long n=0,positive=0,modelBetter=0;double sq=0,baselineSq=0,abs=0,sumActual=0,sumPred=0,net=0;void add(double p,double y,double b,double cost){++n;double e=p-y,eb=b-y;sq+=e*e;baselineSq+=eb*eb;abs+=std::abs(e);positive+=(p>0)==(y>0);modelBetter+=(std::abs(e)<std::abs(eb));sumActual+=y;sumPred+=p;net+=y-cost;}};
inline std::string csvNum(double x){std::ostringstream s;s.precision(12);s<<x;return s.str();}
inline void statsRow(std::ostream& f,const std::string& group,const std::string& key,int horizon,const Stats& s){
 if(!s.n)return;f<<group<<','<<key<<','<<horizon<<','<<s.n<<','<<csvNum(std::sqrt(s.sq/s.n))<<','<<csvNum(std::sqrt(s.baselineSq/s.n))<<','<<csvNum(s.abs/s.n)<<','<<csvNum(double(s.positive)/s.n)<<','<<csvNum(double(s.modelBetter)/s.n)<<','<<csvNum(s.sumPred/s.n)<<','<<csvNum(s.sumActual/s.n)<<','<<csvNum(s.net/s.n)<<'\n';
}
}
