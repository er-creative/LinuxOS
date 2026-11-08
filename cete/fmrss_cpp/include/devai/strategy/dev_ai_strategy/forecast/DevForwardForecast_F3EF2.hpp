#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace devai::forecast::f3ef2 {
constexpr int H = 5;
constexpr int P = 5;
constexpr std::array<int,H> horizons{1,3,5,10,15};
using Features = std::array<double,P>;
struct Row { std::string symbol; long long epoch=0; int session=0; Features x{}; std::array<int,H> valid{}; std::array<double,H> y{}; };
inline std::vector<std::string> split(const std::string& s, char sep=',') {
 std::vector<std::string> v; std::stringstream ss(s); std::string t; while(std::getline(ss,t,sep)) v.push_back(t); return v;
}
inline int col(const std::map<std::string,int>& m, const std::string& k) { auto it=m.find(k); if(it==m.end()) throw std::runtime_error("Missing CSV column: "+k); return it->second; }
inline double number(const std::vector<std::string>& v,int i) { if(i<0||static_cast<size_t>(i)>=v.size()) throw std::runtime_error("CSV row too short"); size_t used=0; double d=std::stod(v[i],&used); if(used!=v[i].size()||!std::isfinite(d)) throw std::runtime_error("Invalid numeric value"); return d; }
inline long long integer(const std::vector<std::string>& v,int i) { if(i<0||static_cast<size_t>(i)>=v.size()) throw std::runtime_error("CSV row too short"); return std::stoll(v[i]); }
inline void read_csv(const std::string& path,const std::function<void(const Row&)>& fn) {
 std::ifstream f(path); if(!f) throw std::runtime_error("Cannot open "+path);
 std::string line; if(!std::getline(f,line)) throw std::runtime_error("Empty CSV: "+path);
 auto header=split(line); std::map<std::string,int> m; for(size_t i=0;i<header.size();++i) m[header[i]]=static_cast<int>(i);
 const int cs=col(m,"symbol"),ce=col(m,"decision_epoch"),cd=col(m,"session_id"),cc=col(m,"close"),cm=col(m,"mean20"),ct=col(m,"std20"),cz=col(m,"z20"),cr=col(m,"return_1m_past"),cn=col(m,"nifty_return_1m_past");
 std::array<int,H> vi{},yi{}; for(int j=0;j<H;++j){vi[j]=col(m,"valid_"+std::to_string(horizons[j])+"m"); yi[j]=col(m,"target_return_"+std::to_string(horizons[j])+"m");}
 size_t lineno=1; while(std::getline(f,line)) {++lineno; if(line.empty()) continue; auto v=split(line); try {
 Row r; r.symbol=v.at(cs); r.epoch=integer(v,ce); r.session=static_cast<int>(integer(v,cd)); double close=number(v,cc),mean=number(v,cm),std=number(v,ct);
 if(close<=0||std<0) continue;
 r.x={number(v,cz),std/close,number(v,cr),number(v,cn),(close-mean)/close};
 bool good=true; for(double x:r.x) if(!std::isfinite(x)) good=false; if(!good) continue;
 for(int j=0;j<H;++j) {r.valid[j]=static_cast<int>(integer(v,vi[j])); if(r.valid[j]) r.y[j]=number(v,yi[j]);}
 fn(r);
 } catch(const std::exception& e){throw std::runtime_error(path+":"+std::to_string(lineno)+": "+e.what());} }
}
struct Moments {std::array<double,P> sum{},sumsq{},mean{},sd{}; long long n=0;
 void add(const Features& x){++n;for(int k=0;k<P;++k){sum[k]+=x[k];sumsq[k]+=x[k]*x[k];}}
 void finish(){if(n==0)throw std::runtime_error("No training rows");for(int k=0;k<P;++k){mean[k]=sum[k]/n; sd[k]=std::sqrt(std::max(0.,sumsq[k]/n-mean[k]*mean[k]));if(sd[k]<1e-12)sd[k]=1.;}}
 std::array<double,P+1> transform(const Features& x)const{std::array<double,P+1> z{};z[0]=1;for(int k=0;k<P;++k)z[k+1]=(x[k]-mean[k])/sd[k];return z;}
};
constexpr int D=P+1;
struct Accumulator {std::array<std::array<double,D>,D> a{};std::array<double,D> b{};long long n=0;double ysum=0;
 void add(const std::array<double,D>& z,double y){++n;ysum+=y;for(int i=0;i<D;++i){b[i]+=z[i]*y;for(int k=0;k<D;++k)a[i][k]+=z[i]*z[k];}}
};
inline std::array<double,D> solve(Accumulator ac,double lambda){
 for(int i=1;i<D;++i)ac.a[i][i]+=lambda; // regularize features, not intercept
 for(int i=0;i<D;++i){int pivot=i;for(int j=i+1;j<D;++j)if(std::abs(ac.a[j][i])>std::abs(ac.a[pivot][i]))pivot=j;
 if(std::abs(ac.a[pivot][i])<1e-14)throw std::runtime_error("Singular fit; try positive regularization");
 std::swap(ac.a[pivot],ac.a[i]);std::swap(ac.b[pivot],ac.b[i]);double q=ac.a[i][i];for(int k=i;k<D;++k)ac.a[i][k]/=q;ac.b[i]/=q;
 for(int j=0;j<D;++j)if(j!=i){double f=ac.a[j][i];for(int k=i;k<D;++k)ac.a[j][k]-=f*ac.a[i][k];ac.b[j]-=f*ac.b[i];}}
 return ac.b;
}
inline double predict(const std::array<double,D>& w,const std::array<double,D>& z){double s=0;for(int i=0;i<D;++i)s+=w[i]*z[i];return s;}
struct Metrics {long long n=0;double se=0,ae=0,base_se=0,base_ae=0,dir_correct=0,ysum=0;
 void add(double y,double yhat,double baseline){++n;se+=(y-yhat)*(y-yhat);ae+=std::abs(y-yhat);base_se+=(y-baseline)*(y-baseline);base_ae+=std::abs(y-baseline);dir_correct+=(yhat>0&&y>0)||(yhat<0&&y<0);ysum+=y;}
};
struct Model {Moments moments;std::array<std::array<double,D>,H> weights{};std::array<double,H> baselines{};
 void save(const std::string& path)const {std::ofstream f(path);if(!f)throw std::runtime_error("Cannot write model "+path);f<<std::setprecision(17)<<"F3EF2_RIDGE_V1\n";for(int k=0;k<P;++k)f<<moments.mean[k]<<' '<<moments.sd[k]<<'\n';for(int h=0;h<H;++h){f<<horizons[h]<<' '<<baselines[h];for(double w:weights[h])f<<' '<<w;f<<'\n';}}
 static Model load(const std::string& path){std::ifstream f(path);std::string magic;f>>magic;if(magic!="F3EF2_RIDGE_V1")throw std::runtime_error("Invalid model format");Model m;for(int k=0;k<P;++k)f>>m.moments.mean[k]>>m.moments.sd[k];for(int h=0;h<H;++h){int horizon;f>>horizon>>m.baselines[h];if(horizon!=horizons[h])throw std::runtime_error("Model horizon mismatch");for(double& w:m.weights[h])f>>w;}if(!f)throw std::runtime_error("Truncated model");return m;}
 std::array<double,H> forecast(const Features& x)const{auto z=moments.transform(x);std::array<double,H> out{};for(int h=0;h<H;++h)out[h]=predict(weights[h],z);return out;}
};
} // namespace
