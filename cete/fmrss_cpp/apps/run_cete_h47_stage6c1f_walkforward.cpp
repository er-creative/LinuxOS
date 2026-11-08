// CETE Stage 5E.2K.6C.1F: purged rolling walk-forward retraining and cost-aware ranking.
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
#include <limits>
#include <sstream>
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
// Kite candle timestamps mark candle START. Use latest candle whose END <= decision time.
const Candle* completed(const Series& s,std::int64_t t,int tf,std::size_t* idx=nullptr){
    const std::int64_t span=60LL*tf;
    auto it=s.index.upper_bound(t-span);
    if(it==s.index.begin())return nullptr;
    --it;
    const auto start=it->first;
    // Do not carry yesterday's 5m/15m candle into today's session.
    if(day(start)!=day(t) || start+span>t || t-(start+span)>=span)return nullptr;
    if(idx)*idx=it->second;
    return &s.a[it->second];
}
// Allow rolling history to span previous sessions, but never silently bridge a missing
// candle within the same session. Reject nonmonotonic timestamps and implausible gaps.
bool history(const Series& s,std::size_t i,int tf,int bars){
    if(i+1<static_cast<std::size_t>(bars))return false;
    const std::size_t first=i+1-bars;
    for(std::size_t j=first+1;j<=i;++j){
        const auto prev=s.a[j-1].timestamp,cur=s.a[j].timestamp;
        if(cur<=prev)return false;
        if(day(cur)==day(prev)){
            if(cur-prev!=60LL*tf)return false;
        }else{
            if(day(cur)-day(prev)>7)return false;
        }
    }
    return true;
}
struct Raw {double ret=0,mean=0,sd=0,z=0,volume_z=0,range=0,bench_ret=0,relative=0,volatility=0,logvolume=0;};
bool raw(const Series& s,const Series& benchmark,std::int64_t t,int tf,Raw& o){std::size_t i=0,bi=0;const Candle* c=completed(s,t,tf,&i);const Candle* b=completed(benchmark,t,tf,&bi);if(!c||!b||c->timestamp!=b->timestamp||!history(s,i,tf,11)||!history(benchmark,bi,tf,11))return false;
std::array<double,10> r{},br{},v{};for(int k=0;k<10;++k){const auto j=i-9+k,bj=bi-9+k;require(s.a[j-1].close>0&&benchmark.a[bj-1].close>0,"Invalid historical close");r[k]=s.a[j].close/s.a[j-1].close-1;br[k]=benchmark.a[bj].close/benchmark.a[bj-1].close-1;v[k]=std::log1p(static_cast<double>(s.a[j].volume));}
auto mean=[](const auto& a){return std::accumulate(a.begin(),a.end(),0.)/a.size();};const double m=mean(r),vm=mean(v);double var=0,vvar=0;for(int k=0;k<10;++k){var+=(r[k]-m)*(r[k]-m);vvar+=(v[k]-vm)*(v[k]-vm);}const double sd=std::sqrt(var/10.),vsd=std::sqrt(vvar/10.);o.ret=r.back();o.mean=m;o.sd=sd;o.z=sd>1e-12?(r.back()-m)/sd:0;o.volume_z=vsd>1e-12?(v.back()-vm)/vsd:0;o.range=(c->high-c->low)/c->close;o.bench_ret=br.back();o.relative=o.ret-o.bench_ret;o.volatility=sd;o.logvolume=v.back();return std::isfinite(o.z)&&std::isfinite(o.range)&&std::isfinite(o.relative);}
V features(const Store& store,const std::string& symbol,std::int64_t t){V x{};x[0]=1;const auto& a=store.at(symbol);const auto& b=store.at("NIFTY 50");std::array<Raw,3> rr{};for(int k=0;k<3;++k){require(raw(a[k],b[k],t,mins[k],rr[k]),"Unavailable causal 1m/5m/15m window");const auto& z=rr[k];const std::array<double,10> q{z.ret,z.mean,z.sd,z.z,z.volume_z,z.range,z.bench_ret,z.relative,z.volatility,z.logvolume};for(int j=0;j<10;++j)x[1+k*10+j]=q[j];}x[31]=rr[0].ret*rr[1].ret;x[32]=rr[0].ret*rr[2].ret;x[33]=rr[1].ret*rr[2].ret;return x;}
struct Sample{std::int64_t t,end;std::string symbol;int rank,h;bool eligible;double y;V x;};
struct Model{V w{};double lambda=0;std::size_t n=0;};
Model fit(const std::vector<Sample>& s,int h,std::int64_t boundary,double lambda,bool nonlinear){Mat a{};V b{};std::size_t n=0;for(const auto& z:s){if(z.h!=h||z.t>=boundary||z.end>=boundary)continue;++n;for(int i=0;i<F;++i){if(!nonlinear&&i>=31)continue;b[i]+=z.x[i]*z.y;for(int j=0;j<=i;++j){if(!nonlinear&&j>=31)continue;a[i][j]+=z.x[i]*z.x[j];}}}require(n>=100,"Too few purged training labels");for(int i=0;i<F;++i){b[i]/=n;for(int j=0;j<=i;++j){a[i][j]/=n;a[j][i]=a[i][j];}}for(int i=0;i<F;++i){a[i][i]+=i==0?1e-12:lambda;if(!nonlinear&&i>=31){a[i][i]=1;b[i]=0;}}for(int i=0;i<F;++i){int p=i;for(int j=i+1;j<F;++j)if(std::abs(a[j][i])>std::abs(a[p][i]))p=j;require(std::abs(a[p][i])>1e-15,"Singular regression system");std::swap(a[i],a[p]);std::swap(b[i],b[p]);const double pivot=a[i][i];for(int j=i;j<F;++j)a[i][j]/=pivot;b[i]/=pivot;for(int j=i+1;j<F;++j){const double f=a[j][i];for(int k=i;k<F;++k)a[j][k]-=f*a[i][k];b[j]-=f*b[i];}}Model m;m.lambda=lambda;m.n=n;for(int i=F-1;i>=0;--i){double v=b[i];for(int j=i+1;j<F;++j)v-=a[i][j]*m.w[j];m.w[i]=v;}return m;}
double predict(const Model& m,const V& x){return std::inner_product(m.w.begin(),m.w.end(),x.begin(),0.);}
struct Score {size_t n=0,positive=0;double squared=0,zero=0,sp=0,sy=0,sp2=0,sy2=0,spy=0;void add(double p,double y){++n;positive+=(p>0);squared+=(p-y)*(p-y);zero+=y*y;sp+=p;sy+=y;sp2+=p*p;sy2+=y*y;spy+=p*y;}double rmse()const{return n?std::sqrt(squared/n):0;}double corr()const{if(n<2)return 0;double a=sp2-sp*sp/n,b=sy2-sy*sy/n;return a>1e-20&&b>1e-20?(spy-sp*sy/n)/std::sqrt(a*b):0;}};
// Fit sufficient statistics once per boundary and horizon, not once per lambda.
struct Stats {Mat a{};V b{};size_t n=0;};
Stats stats(const std::vector<Sample>& rows,int h,std::int64_t boundary){Stats s;for(const auto& z:rows){if(z.h!=h||z.t>=boundary||z.end>=boundary)continue;++s.n;for(int i=0;i<F;++i){s.b[i]+=z.x[i]*z.y;for(int j=0;j<=i;++j)s.a[i][j]+=z.x[i]*z.x[j];}}require(s.n>=100,"Insufficient purged training samples");return s;}
Model solve(const Stats& s,double lambda,bool nonlinear){Mat a=s.a;V b=s.b;for(int i=0;i<F;++i){b[i]/=s.n;for(int j=0;j<=i;++j){a[i][j]/=s.n;a[j][i]=a[i][j];}}for(int i=0;i<F;++i){a[i][i]+=i==0?1e-12:lambda;if(!nonlinear&&i>=31){a[i][i]=1;b[i]=0;}}for(int i=0;i<F;++i){int p=i;for(int j=i+1;j<F;++j)if(std::abs(a[j][i])>std::abs(a[p][i]))p=j;require(std::abs(a[p][i])>1e-15,"Singular regression matrix");std::swap(a[i],a[p]);std::swap(b[i],b[p]);double q=a[i][i];for(int j=i;j<F;++j)a[i][j]/=q;b[i]/=q;for(int j=i+1;j<F;++j){double f=a[j][i];for(int k=i;k<F;++k)a[j][k]-=f*a[i][k];b[j]-=f*b[i];}}Model m;m.lambda=lambda;m.n=s.n;for(int i=F-1;i>=0;--i){double q=b[i];for(int j=i+1;j<F;++j)q-=a[i][j]*m.w[j];m.w[i]=q;}return m;}
// Editable hypothetical NSE cash intraday assumptions; validate against current broker contract notes.
struct Charges{double brokerage_rate=.0003,brokerage_cap=20,stt_sell=.00025,exchange_each=.0000297,sebi_each=.000001,stamp_buy=.00003,gst=.18,slippage_each=.0001;};
double roundtrip(double notional,const Charges& c){double brokerage=2*std::min(c.brokerage_cap,notional*c.brokerage_rate);double exchange=2*notional*c.exchange_each,sebi=2*notional*c.sebi_each;return brokerage+notional*(c.stt_sell+c.stamp_buy+2*c.slippage_each)+exchange+sebi+c.gst*(brokerage+exchange+sebi);}
struct Prediction{std::int64_t t,end;std::string symbol;int rank,h,fold;bool eligible;double p,y;};
struct Rank{size_t groups=0,pairs=0,wins=0,netpositive=0;double gross=0,net=0;};
}
int main(int argc,char** argv){try{
require(argc==7||argc==8,"Usage: run_cete_h47_stage6c1f_walkforward <enriched.cache> <kite_root> <out_dir> <position_INR> <min_sessions> <slippage_each_fraction> [brokerage_rate]");
const fs::path root=argv[2],out=argv[3];const double position=std::stod(argv[4]);const int minimum=std::stoi(argv[5]);Charges charges;charges.slippage_each=std::stod(argv[6]);if(argc==8)charges.brokerage_rate=std::stod(argv[7]);require(position>0&&std::isfinite(position)&&minimum>=20&&charges.slippage_each>=0&&charges.slippage_each<.1&&charges.brokerage_rate>=0&&charges.brokerage_rate<.1,"Invalid parameters");fs::create_directories(out);
auto data=ds::DevHistoricalTrainingCache::load(argv[1]);ds::DevHistoricalTrainingDatasetValidator::validate(data);std::set<std::string> symbols{"NIFTY 50"};for(const auto& r:data.records)for(const auto& c:r.candidates)symbols.insert(c.symbol);
Store store;for(const auto& symbol:symbols){std::array<Series,3> ss;for(int k=0;k<3;++k){ss[k]=load(pathFor(root,symbol,mins[k]));std::cout<<"[6C1F][DATA] "<<symbol<<' '<<mins[k]<<"m="<<ss[k].a.size()<<'\n';}store.emplace(symbol,std::move(ss));}
std::set<std::int64_t> daySet;for(const auto& r:data.records)daySet.insert(day(r.decision_time));std::vector<std::int64_t> days(daySet.begin(),daySet.end());require(int(days.size())>=minimum,"Insufficient sessions");
std::vector<Sample> rows;size_t missing=0,invalid=0;for(const auto& r:data.records)for(const auto& c:r.candidates){if(!r.numerical_integrity||!c.numerical_integrity||!c.observation.ready||!c.observation.numerical_integrity)continue;V x{};try{x=features(store,c.symbol,r.decision_time);}catch(const std::exception&){++missing;continue;}bool eligible=true;for(auto tf:{ds::DevTimeframe::ONE_MINUTE,ds::DevTimeframe::FIVE_MINUTE,ds::DevTimeframe::FIFTEEN_MINUTE}){const auto idx=ds::marketIndex(tf,ds::DevMetric::RETURN_VALUE);if(c.observation.availability[idx]!=1||c.observation.market[idx]<=0)eligible=false;}for(const auto& o:c.outcomes){if(o.horizon_bars!=5&&o.horizon_bars!=10&&o.horizon_bars!=15)continue;if(!o.available||!o.numerical_integrity||!std::isfinite(o.forward_return)||o.outcome_time<=r.decision_time){++invalid;continue;}rows.push_back({r.decision_time,o.outcome_time,c.symbol,int(c.candidate_rank),int(o.horizon_bars),eligible,o.forward_return,x});}}
require(!rows.empty(),"No samples");
std::ofstream folds(out/"walkforward_folds.csv"),pred(out/"walkforward_predictions.csv"),rank(out/"ranking_metrics.csv"),summary(out/"run_summary.csv"),costs(out/"cost_assumptions.csv"),elig(out/"eligibility_audit.csv");require(bool(folds)&&bool(pred)&&bool(rank)&&bool(summary)&&bool(costs)&&bool(elig),"Cannot open output CSV");for(auto* f:{&folds,&pred,&rank,&summary,&costs,&elig})*f<<std::setprecision(15);
folds<<"fold,horizon,train_start_day,validation_start_day,test_start_day,test_end_day,model,lambda,train_fit_rows,test_fit_rows,validation_rmse,test_count,test_rmse,test_zero_rmse,test_correlation\n";
pred<<"fold,decision_time,outcome_time,symbol,rank,horizon_bars,math_eligible,predicted_forward_return,realized_forward_return,hypothetical_cost_fraction,realized_net_fraction\n";
rank<<"fold,horizon,decision_groups_with_two_or_more,comparisons,pairwise_accuracy,top1_mean_gross_fraction,top1_mean_net_fraction,top1_positive_net_fraction,top1_mean_net_INR\n";
elig<<"fold,horizon,all_candidate_rows,math_eligible_rows,positive_forecast_rows,math_eligible_negative_forecast_rows,decision_groups_with_two_or_more\n";
costs<<"component,assumed_value,unit\n"<<"brokerage_rate_each,"<<charges.brokerage_rate<<",fraction\n"<<"brokerage_cap_each,"<<charges.brokerage_cap<<",INR\n"<<"STT_sell,"<<charges.stt_sell<<",fraction\n"<<"exchange_each,"<<charges.exchange_each<<",fraction\n"<<"SEBI_each,"<<charges.sebi_each<<",fraction\n"<<"stamp_buy,"<<charges.stamp_buy<<",fraction\n"<<"GST_on_brokerage_exchange_SEBI,"<<charges.gst<<",fraction\n"<<"slippage_each,"<<charges.slippage_each<<",fraction\n";
const double charge=roundtrip(position,charges),costFraction=charge/position;
// Expanding train 12 sessions, validation 4 sessions, unseen test 4 sessions; roll by 4.
const int trainMin=12,valLen=4,testLen=4,step=4;int foldCount=0;size_t totalTest=0;for(int testIdx=trainMin+valLen;testIdx+testLen<=int(days.size());testIdx+=step){++foldCount;const int valIdx=testIdx-valLen;const std::int64_t valBoundary=days[valIdx]*86400-19800,testBoundary=days[testIdx]*86400-19800,testEnd=(days[testIdx+testLen-1]+1)*86400-19800;
for(int h:{5,10,15}){const Stats training=stats(rows,h,valBoundary);double best=std::numeric_limits<double>::infinity(),lambdaChosen=0;bool nonlinearChosen=false;for(bool nonlinear:{false,true})for(double lambda:{.0001,.01,1.,10.,100.}){Model m=solve(training,lambda,nonlinear);Score sc;for(const auto& z:rows)if(z.h==h&&z.t>=valBoundary&&z.t<testBoundary&&z.end<testBoundary)sc.add(predict(m,z.x),z.y);require(sc.n>0,"No purged validation labels");if(sc.rmse()<best){best=sc.rmse();lambdaChosen=lambda;nonlinearChosen=nonlinear;}}
const Stats testTraining=stats(rows,h,testBoundary);Model m=solve(testTraining,lambdaChosen,nonlinearChosen);Score test;std::map<std::int64_t,std::vector<Prediction>> groups;size_t all=0,eligible=0,pos=0,eligibleNeg=0;for(const auto& z:rows){if(z.h!=h||z.t<testBoundary||z.t>=testEnd||z.end>=testEnd)continue;const double p=predict(m,z.x);test.add(p,z.y);++all;pos+=(p>0);eligible+=z.eligible;eligibleNeg+=(z.eligible&&p<=0);pred<<foldCount<<','<<z.t<<','<<z.end<<','<<z.symbol<<','<<z.rank<<','<<h<<','<<z.eligible<<','<<p<<','<<z.y<<','<<costFraction<<','<<z.y-costFraction<<'\n';if(z.eligible)groups[z.t].push_back({z.t,z.end,z.symbol,z.rank,h,foldCount,z.eligible,p,z.y});}
Rank rr;for(auto& [time,v]:groups){if(v.size()<2)continue;++rr.groups;std::sort(v.begin(),v.end(),[](const Prediction&a,const Prediction&b){return a.p>b.p||(a.p==b.p&&a.symbol<b.symbol);});rr.gross+=v.front().y;rr.net+=v.front().y-costFraction;rr.netpositive+=(v.front().y>costFraction);for(size_t i=0;i<v.size();++i)for(size_t j=i+1;j<v.size();++j){if(v[i].y==v[j].y)continue;++rr.pairs;rr.wins+=(v[i].y>v[j].y);}}
folds<<foldCount<<','<<h<<','<<days.front()<<','<<days[valIdx]<<','<<days[testIdx]<<','<<days[testIdx+testLen-1]<<','<<(nonlinearChosen?"nonlinear":"linear")<<','<<lambdaChosen<<','<<training.n<<','<<testTraining.n<<','<<best<<','<<test.n<<','<<test.rmse()<<','<<std::sqrt(test.zero/test.n)<<','<<test.corr()<<'\n';rank<<foldCount<<','<<h<<','<<rr.groups<<','<<rr.pairs<<','<<(rr.pairs?double(rr.wins)/rr.pairs:0)<<','<<(rr.groups?rr.gross/rr.groups:0)<<','<<(rr.groups?rr.net/rr.groups:0)<<','<<(rr.groups?double(rr.netpositive)/rr.groups:0)<<','<<(rr.groups?rr.net/rr.groups*position:0)<<'\n';elig<<foldCount<<','<<h<<','<<all<<','<<eligible<<','<<pos<<','<<eligibleNeg<<','<<rr.groups<<'\n';totalTest+=test.n;std::cout<<"[6C1F] fold="<<foldCount<<" h="<<h<<" model="<<(nonlinearChosen?"nonlinear":"linear")<<" lambda="<<lambdaChosen<<" test_rows="<<test.n<<" RMSE="<<test.rmse()<<" pairs="<<rr.pairs<<'\n';}}
require(foldCount>=1,"No complete walk-forward folds");summary<<"metric,value\n"<<"sessions,"<<days.size()<<"\n"<<"walkforward_folds,"<<foldCount<<"\n"<<"test_predictions_all_horizons,"<<totalTest<<"\n"<<"raw_feature_samples,"<<rows.size()<<"\n"<<"missing_windows,"<<missing<<"\n"<<"invalid_outcomes,"<<invalid<<"\n"<<"position_INR,"<<position<<"\n"<<"estimated_roundtrip_cost_INR,"<<charge<<"\n"<<"estimated_roundtrip_cost_fraction,"<<costFraction<<"\n"<<"note,Train and refit purge outcomes at validation and test boundaries\n"<<"note_2,Mathematical eligibility is independent of forecast sign\n"<<"note_3,Research counterfactual only; no executable position ledger or capital switching\n"<<"note_4,Cost rates are editable assumptions and require broker confirmation\n"<<"note_5,Overlapping test horizons and folds are not independent trades\n";
std::cout<<"[CETE][6C1F] WALK-FORWARD COMPLETE folds="<<foldCount<<" predictions="<<totalTest<<" output="<<out<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"[CETE][6C1F] FAIL: "<<e.what()<<'\n';return 1;}}
