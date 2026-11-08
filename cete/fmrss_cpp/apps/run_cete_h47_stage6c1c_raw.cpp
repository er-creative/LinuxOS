// CETE Stage 5E.2K.6C.1C: read-only causal raw OHLCV forecasting benchmark.
// Targets come ONLY from offline cache outcomes; all features use completed candles.
#include "devai/market/MarketDataLoader.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace fs=std::filesystem;
namespace ds=devai::strategy;
using Candle=devai::market::Candle;
namespace {
constexpr int F=34; // 1 intercept + 3*10 raw features + 3 cross-horizon interactions
using V=std::array<double,F>;using Mat=std::array<V,F>;
void require(bool x,const std::string& s){if(!x)throw std::runtime_error(s);}
std::int64_t day(std::int64_t t){auto x=t+19800;return x>=0?x/86400:(x-86399)/86400;}
struct Series {std::vector<Candle> a;std::map<std::int64_t,std::size_t> index;};
using Store=std::map<std::string,std::array<Series,3>>;
const std::array<int,3> mins{1,5,15};
std::string fileName(const std::string& symbol,int tf){return symbol=="NIFTY 50"?"NIFTY%2050_"+(tf==1?std::string("1min.txt"):tf==5?std::string("5mins.txt"):std::string("15mins.txt")):symbol+"_"+(tf==1?std::string("1min.txt"):tf==5?std::string("5mins.txt"):std::string("15mins.txt"));}
fs::path pathFor(const fs::path& root,const std::string& symbol,int tf){return root/(tf==1?"minute":tf==5?"5minutes":"15minutes")/fileName(symbol,tf);}
Series load(const fs::path& path){require(fs::exists(path),"Missing OHLCV file: "+path.string());devai::market::MarketDataLoader loader(path.parent_path());auto result=loader.loadPath(path);require(!result.candles.empty(),"No candles in "+path.string());Series s;s.a=std::move(result.candles);std::sort(s.a.begin(),s.a.end(),[](const auto& a,const auto& b){return a.timestamp<b.timestamp;});for(std::size_t i=0;i<s.a.size();++i)require(s.index.emplace(s.a[i].timestamp,i).second,"Duplicate candle timestamp: "+path.string());return s;}
// Exact close boundary: candle [timestamp,timestamp+tf*60) is available only at end.
const Candle* completed(const Series& s,std::int64_t t,int tf,std::size_t* idx=nullptr){auto it=s.index.find(t-60LL*tf);if(it==s.index.end())return nullptr;if(idx)*idx=it->second;return &s.a[it->second];}
// For rolling features require contiguous timestamps AND same IST session.
bool history(const Series& s,std::size_t i,int tf,int bars){if(i+1<static_cast<std::size_t>(bars))return false;const std::size_t first=i+1-bars;for(std::size_t j=first;j<=i;++j){if(day(s.a[j].timestamp)!=day(s.a[i].timestamp))return false;if(j>first&&s.a[j].timestamp-s.a[j-1].timestamp!=60LL*tf)return false;}return true;}
struct Raw {double ret=0,mean=0,sd=0,z=0,volume_z=0,range=0,bench_ret=0,relative=0,volatility=0,logvolume=0;};
bool raw(const Series& s,const Series& benchmark,std::int64_t t,int tf,Raw& o){std::size_t i=0,bi=0;const Candle* c=completed(s,t,tf,&i);const Candle* b=completed(benchmark,t,tf,&bi);if(!c||!b||!history(s,i,tf,11)||!history(benchmark,bi,tf,11))return false;
std::array<double,10> r{},br{},v{};for(int k=0;k<10;++k){const auto j=i-9+k,bj=bi-9+k;require(s.a[j-1].close>0&&benchmark.a[bj-1].close>0,"Invalid historical close");r[k]=s.a[j].close/s.a[j-1].close-1;br[k]=benchmark.a[bj].close/benchmark.a[bj-1].close-1;v[k]=std::log1p(static_cast<double>(s.a[j].volume));}
auto mean=[](const auto& a){return std::accumulate(a.begin(),a.end(),0.)/a.size();};const double m=mean(r),vm=mean(v);double var=0,vvar=0;for(int k=0;k<10;++k){var+=(r[k]-m)*(r[k]-m);vvar+=(v[k]-vm)*(v[k]-vm);}const double sd=std::sqrt(var/10.),vsd=std::sqrt(vvar/10.);o.ret=r.back();o.mean=m;o.sd=sd;o.z=sd>1e-12?(r.back()-m)/sd:0;o.volume_z=vsd>1e-12?(v.back()-vm)/vsd:0;o.range=(c->high-c->low)/c->close;o.bench_ret=br.back();o.relative=o.ret-o.bench_ret;o.volatility=sd;o.logvolume=v.back();return std::isfinite(o.z)&&std::isfinite(o.range)&&std::isfinite(o.relative);}
V features(const Store& store,const std::string& symbol,std::int64_t t){V x{};x[0]=1;const auto& a=store.at(symbol);const auto& b=store.at("NIFTY 50");std::array<Raw,3> rr{};for(int k=0;k<3;++k){require(raw(a[k],b[k],t,mins[k],rr[k]),"Unavailable causal 1m/5m/15m window");const auto& z=rr[k];const std::array<double,10> q{z.ret,z.mean,z.sd,z.z,z.volume_z,z.range,z.bench_ret,z.relative,z.volatility,z.logvolume};for(int j=0;j<10;++j)x[1+k*10+j]=q[j];}x[31]=rr[0].ret*rr[1].ret;x[32]=rr[0].ret*rr[2].ret;x[33]=rr[1].ret*rr[2].ret;return x;}
struct Sample{std::int64_t t,end;std::string symbol;int rank,h;bool eligible;double y;V x;};
struct Model{V w{};double lambda=0;std::size_t n=0;};
Model fit(const std::vector<Sample>& s,int h,std::int64_t boundary,double lambda,bool nonlinear){Mat a{};V b{};std::size_t n=0;for(const auto& z:s){if(z.h!=h||z.t>=boundary||z.end>=boundary)continue;++n;for(int i=0;i<F;++i){if(!nonlinear&&i>=31)continue;b[i]+=z.x[i]*z.y;for(int j=0;j<=i;++j){if(!nonlinear&&j>=31)continue;a[i][j]+=z.x[i]*z.x[j];}}}require(n>=100,"Too few purged training labels");for(int i=0;i<F;++i){b[i]/=n;for(int j=0;j<=i;++j){a[i][j]/=n;a[j][i]=a[i][j];}}for(int i=0;i<F;++i){a[i][i]+=i==0?1e-12:lambda;if(!nonlinear&&i>=31){a[i][i]=1;b[i]=0;}}for(int i=0;i<F;++i){int p=i;for(int j=i+1;j<F;++j)if(std::abs(a[j][i])>std::abs(a[p][i]))p=j;require(std::abs(a[p][i])>1e-15,"Singular regression system");std::swap(a[i],a[p]);std::swap(b[i],b[p]);const double pivot=a[i][i];for(int j=i;j<F;++j)a[i][j]/=pivot;b[i]/=pivot;for(int j=i+1;j<F;++j){const double f=a[j][i];for(int k=i;k<F;++k)a[j][k]-=f*a[i][k];b[j]-=f*b[i];}}Model m;m.lambda=lambda;m.n=n;for(int i=F-1;i>=0;--i){double v=b[i];for(int j=i+1;j<F;++j)v-=a[i][j]*m.w[j];m.w[i]=v;}return m;}
double predict(const Model& m,const V& x){return std::inner_product(m.w.begin(),m.w.end(),x.begin(),0.);}
struct Score{std::size_t n=0,pos=0,predpos=0;double sq=0,zero=0,sy=0,sp=0,syp=0,sy2=0,sp2=0;void add(double p,double y){++n;pos+=y>0;predpos+=p>0;sq+=(p-y)*(p-y);zero+=y*y;sy+=y;sp+=p;syp+=y*p;sy2+=y*y;sp2+=p*p;}double rmse()const{return n?std::sqrt(sq/n):0;}double corr()const{if(!n)return 0;const double c=syp/n-(sy/n)*(sp/n),v1=sy2/n-(sy/n)*(sy/n),v2=sp2/n-(sp/n)*(sp/n);return v1>1e-20&&v2>1e-20?c/std::sqrt(v1*v2):0;}};
}
int main(int argc,char** argv){try{
require(argc==7,"Usage: run_cete_h47_stage6c1c_raw <enriched.cache> <kite_downloads_root> <out_dir> <position_INR> <roundtrip_cost_fraction> <min_sessions>");
const fs::path root=argv[2],out=argv[3];const double capital=std::stod(argv[4]),cost=std::stod(argv[5]);const int minSessions=std::stoi(argv[6]);require(capital>0&&std::isfinite(capital)&&cost>=0&&cost<1&&minSessions>=10,"Invalid parameters");fs::create_directories(out);
auto data=ds::DevHistoricalTrainingCache::load(argv[1]);ds::DevHistoricalTrainingDatasetValidator::validate(data);std::set<std::string> symbols{"NIFTY 50"};for(const auto& r:data.records)for(const auto& c:r.candidates)symbols.insert(c.symbol);
Store store;for(const auto& symbol:symbols){std::array<Series,3> ss;for(int k=0;k<3;++k){ss[k]=load(pathFor(root,symbol,mins[k]));std::cout<<"[RAW] "<<symbol<<" "<<mins[k]<<"m candles="<<ss[k].a.size()<<'\n';}store.emplace(symbol,std::move(ss));}
std::set<std::int64_t> days;for(const auto& r:data.records)days.insert(day(r.decision_time));std::vector<std::int64_t> d(days.begin(),days.end());require(static_cast<int>(d.size())>=minSessions,"Insufficient sessions");const std::size_t ntrain=d.size()*6/10,nval=d.size()*2/10;require(ntrain>=2&&nval>=1&&ntrain+nval<d.size(),"Invalid splits");const std::int64_t valStart=d[ntrain]*86400-19800,testStart=d[ntrain+nval]*86400-19800;
std::vector<Sample> samples;std::size_t missing=0,invalid=0;for(const auto& r:data.records)for(const auto& c:r.candidates){if(!r.numerical_integrity||!c.numerical_integrity||!c.observation.ready||!c.observation.numerical_integrity)continue;V x{};try{x=features(store,c.symbol,r.decision_time);}catch(const std::exception&){++missing;continue;}bool eligible=true;for(auto tf:{ds::DevTimeframe::ONE_MINUTE,ds::DevTimeframe::FIVE_MINUTE,ds::DevTimeframe::FIFTEEN_MINUTE}){const auto idx=ds::marketIndex(tf,ds::DevMetric::RETURN_VALUE);if(c.observation.availability[idx]!=1||c.observation.market[idx]<=0)eligible=false;}for(const auto& o:c.outcomes){if(o.horizon_bars!=5&&o.horizon_bars!=10&&o.horizon_bars!=15)continue;if(!o.available||!o.numerical_integrity||!std::isfinite(o.forward_return)||o.outcome_time<=r.decision_time){++invalid;continue;}samples.push_back({r.decision_time,o.outcome_time,c.symbol,static_cast<int>(c.candidate_rank),static_cast<int>(o.horizon_bars),eligible,o.forward_return,x});}}
require(!samples.empty(),"No raw-feature samples; inspect missing windows and data paths");std::ofstream metrics(out/"raw_metrics.csv"),preds(out/"raw_predictions.csv"),sel(out/"model_selection.csv"),summary(out/"run_summary.csv");require(bool(metrics)&&bool(preds)&&bool(sel)&&bool(summary),"Cannot create output files");for(auto* f:{&metrics,&preds,&sel,&summary})*f<<std::setprecision(15);
metrics<<"split,horizon_bars,group,model,count,rmse,rmse_zero,pearson,positive_actual_fraction,positive_prediction_fraction,mean_prediction,mean_realized\n";
preds<<"split,decision_time,outcome_time,symbol,rank,horizon_bars,exact_signal_eligible,model,predicted_return_fraction,realized_return_fraction,predicted_return_percent,predicted_gross_INR,realized_gross_INR,assumed_cost_INR,predicted_net_INR\n";
sel<<"horizon_bars,model,lambda,validation_rmse,selected,purged_test_fit_labels\n";
for(int h:{5,10,15}){double best=1e100,chosenLambda=0;bool chosenNonlinear=false;std::vector<std::tuple<bool,double,double>> trials;for(bool nonlinear:{false,true})for(double lambda:{0.0001,0.001,0.01,0.1,1.,10.,100.}){auto m=fit(samples,h,valStart,lambda,nonlinear);Score sc;for(const auto& z:samples)if(z.h==h&&z.t>=valStart&&z.t<testStart)sc.add(predict(m,z.x),z.y);require(sc.n>0,"No validation rows");const double v=sc.rmse();trials.emplace_back(nonlinear,lambda,v);if(v<best){best=v;chosenLambda=lambda;chosenNonlinear=nonlinear;}}const auto vm=fit(samples,h,valStart,chosenLambda,chosenNonlinear),tm=fit(samples,h,testStart,chosenLambda,chosenNonlinear);for(const auto& tr:trials)sel<<h<<','<<(std::get<0>(tr)?"nonlinear":"linear")<<','<<std::get<1>(tr)<<','<<std::get<2>(tr)<<','<<(std::get<0>(tr)==chosenNonlinear&&std::get<1>(tr)==chosenLambda)<<','<<tm.n<<'\n';std::map<std::tuple<std::string,std::string>,Score> agg;for(const auto& z:samples){if(z.h!=h||z.t<valStart)continue;const bool isTest=z.t>=testStart;const std::string split=isTest?"test":"validation";const double p=predict(isTest?tm:vm,z.x);for(const auto& group:{std::string("all"),z.eligible?std::string("eligible"):std::string("ineligible")})agg[{split,group}].add(p,z.y);preds<<split<<','<<z.t<<','<<z.end<<','<<z.symbol<<','<<z.rank<<','<<h<<','<<z.eligible<<','<<(chosenNonlinear?"nonlinear":"linear")<<','<<p<<','<<z.y<<','<<p*100<<','<<p*capital<<','<<z.y*capital<<','<<cost*capital<<','<<(p-cost)*capital<<'\n';}for(const auto& [key,sc]:agg){metrics<<std::get<0>(key)<<','<<h<<','<<std::get<1>(key)<<','<<(chosenNonlinear?"nonlinear":"linear")<<','<<sc.n<<','<<sc.rmse()<<','<<std::sqrt(sc.zero/sc.n)<<','<<sc.corr()<<','<<double(sc.pos)/sc.n<<','<<double(sc.predpos)/sc.n<<','<<sc.sp/sc.n<<','<<sc.sy/sc.n<<'\n';}std::cout<<"[6C1C] h="<<h<<" selected="<<(chosenNonlinear?"nonlinear":"linear")<<" lambda="<<chosenLambda<<" validation_RMSE="<<best<<" test_RMSE="<<agg.at({"test","all"}).rmse()<<'\n';}
summary<<"metric,value\n"<<"sessions,"<<d.size()<<"\ntrain_sessions,"<<ntrain<<"\nvalidation_sessions,"<<nval<<"\ntest_sessions,"<<d.size()-ntrain-nval<<"\nraw_feature_label_samples,"<<samples.size()<<"\nmissing_raw_feature_windows,"<<missing<<"\ninvalid_outcomes,"<<invalid<<"\nposition_INR,"<<capital<<"\nroundtrip_cost_fraction,"<<cost<<"\n"<<"note,Offline-only; labels are never included in input features\n"<<"note_2,Exact completed-candle timestamp alignment required; missing windows are excluded\n"<<"note_3,Internal chronological holdout only; no independent external OOS or execution simulation\n"<<"note_4,Model selection on validation only; test fit purges labels ending on or after test start\n";
std::cout<<"[CETE][6C1C] RAW FORECAST BENCHMARK COMPLETE samples="<<samples.size()<<" missing_windows="<<missing<<" output="<<out<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"[CETE][6C1C] FAIL: "<<e.what()<<'\n';return 1;}}
