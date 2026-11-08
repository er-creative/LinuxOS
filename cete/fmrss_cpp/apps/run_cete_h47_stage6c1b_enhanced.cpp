// CETE H4.7 Stage 5E.2K.6C.1B: read-only, purged enhanced forecast experiment.
// Uses only contemporaneously available observation percentiles; outcomes are labels ONLY.
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
namespace fs=std::filesystem;
namespace ds=devai::strategy;
namespace {
// 1 intercept + 39 available market percentiles + 39 availability masks
// + 6 pairwise interactions of 1m/5m/15m return values + 2 time features + rank = 88.
constexpr int D=88;
using V=std::array<double,D>;
using M=std::array<V,D>;
struct Sample {std::int64_t t,end;int h,rank;std::string symbol;bool eligible;V x;double y;};
struct Model {V w{};double lambda=0;std::size_t n=0;};
struct Score {std::size_t n=0,correct=0,positive=0,predpositive=0;double sq=0,zeroSq=0,abs=0,sy=0,sp=0,spy=0,sp2=0,sy2=0;void add(double p,double y){++n;sq+=(p-y)*(p-y);zeroSq+=y*y;abs+=std::abs(p-y);correct+=((p>0)==(y>0));positive+=(y>0);predpositive+=(p>0);sy+=y;sp+=p;spy+=p*y;sp2+=p*p;sy2+=y*y;}};
void check(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
std::int64_t dayIST(std::int64_t t){const auto a=t+19800;return (a>=0?a/86400:(a-86399)/86400);}
int minuteIST(std::int64_t t){auto a=(t+19800)%86400;if(a<0)a+=86400;return static_cast<int>(a/60);}
bool eligible(const ds::DevAIObservation& o){if(!o.ready||!o.numerical_integrity)return false;for(auto tf:{ds::DevTimeframe::ONE_MINUTE,ds::DevTimeframe::FIVE_MINUTE,ds::DevTimeframe::FIFTEEN_MINUTE}){const auto i=ds::marketIndex(tf,ds::DevMetric::RETURN_VALUE);if(o.availability[i]!=1.||!std::isfinite(o.market[i])||o.market[i]<=0.)return false;}return true;}
V features(const ds::DevAIObservation& o,int rank,std::int64_t t){V x{};x[0]=1.;for(int i=0;i<39;++i){const bool avail=o.availability[static_cast<std::size_t>(i)]==1.&&std::isfinite(o.market[static_cast<std::size_t>(i)]);x[1+i]=avail?o.market[static_cast<std::size_t>(i)]:0.;x[40+i]=avail?1.:0.;}
const auto a=x[1],b=x[14],c=x[27];x[79]=a*a;x[80]=b*b;x[81]=c*c;x[82]=a*b;x[83]=a*c;x[84]=b*c;
const double angle=6.283185307179586*(minuteIST(t)-555)/375.;x[85]=std::sin(angle);x[86]=std::cos(angle);x[87]=(static_cast<double>(rank)-3.)/2.;return x;}
// Solve ridge normal equations via pivoted Gaussian elimination.
V solve(M a,V b){for(int i=0;i<D;++i){int p=i;for(int j=i+1;j<D;++j)if(std::abs(a[j][i])>std::abs(a[p][i]))p=j;check(std::abs(a[p][i])>1e-14,"Ridge system singular");std::swap(a[i],a[p]);std::swap(b[i],b[p]);const double diag=a[i][i];for(int k=i;k<D;++k)a[i][k]/=diag;b[i]/=diag;for(int j=i+1;j<D;++j){const double f=a[j][i];if(f==0)continue;for(int k=i;k<D;++k)a[j][k]-=f*a[i][k];b[j]-=f*b[i];}}V w{};for(int i=D-1;i>=0;--i){double z=b[i];for(int j=i+1;j<D;++j)z-=a[i][j]*w[j];w[i]=z;}return w;}
Model fit(const std::vector<Sample>& samples,int h,std::int64_t boundary,double lambda){
struct Stats {M a{};V b{};std::size_t n=0;};
// Reuse normal-equation sufficient statistics across validation lambda trials.
static std::map<std::pair<int,std::int64_t>,Stats> cache;
const auto key=std::make_pair(h,boundary);
auto it=cache.find(key);
if(it==cache.end()){
Stats st;
for(const auto& s:samples){if(s.h!=h||s.t>=boundary||s.end>=boundary)continue;++st.n;
for(int i=0;i<D;++i){st.b[i]+=s.x[i]*s.y;for(int j=0;j<=i;++j)st.a[i][j]+=s.x[i]*s.x[j];}}
check(st.n>=100,"Insufficient purged training samples");
const double inv=1./static_cast<double>(st.n);
for(int i=0;i<D;++i){st.b[i]*=inv;for(int j=0;j<=i;++j){st.a[i][j]*=inv;st.a[j][i]=st.a[i][j];}}
it=cache.emplace(key,std::move(st)).first;
}
M a=it->second.a;V b=it->second.b;
for(int i=1;i<D;++i)a[i][i]+=lambda;
a[0][0]+=1e-12;
Model m;m.w=solve(a,b);m.lambda=lambda;m.n=it->second.n;return m;
}
double predict(const Model& m,const V& x){return std::inner_product(m.w.begin(),m.w.end(),x.begin(),0.);}
std::vector<std::string> csv(const std::string& s){std::vector<std::string> v;std::string cell;bool q=false;for(std::size_t i=0;i<s.size();++i){const char c=s[i];if(c=='"'){if(q&&i+1<s.size()&&s[i+1]=='"'){cell+='"';++i;}else q=!q;}else if(c==','&&!q){v.push_back(cell);cell.clear();}else cell+=c;}check(!q,"Unclosed CSV quote");v.push_back(cell);return v;}
using Key=std::tuple<std::string,std::int64_t,std::string,int>;
std::map<Key,double> loadBaseline(const fs::path& path){std::ifstream in(path);check(bool(in),"Cannot open original Stage 6C.1 predictions");std::string line;check(bool(std::getline(in,line)),"Empty original predictions");const auto h=csv(line);std::map<std::string,std::size_t> ix;for(std::size_t i=0;i<h.size();++i)ix[h[i]]=i;for(const auto& s:{"split","decision_time","symbol","horizon_bars","predicted_forward_return"})check(ix.count(s),std::string("Missing baseline column ")+s);std::map<Key,double> result;while(std::getline(in,line)){if(line.empty())continue;auto c=csv(line);check(c.size()==h.size(),"Baseline CSV column count mismatch");Key k{c[ix.at("split")],std::stoll(c[ix.at("decision_time")]),c[ix.at("symbol")],std::stoi(c[ix.at("horizon_bars")])};check(result.emplace(k,std::stod(c[ix.at("predicted_forward_return")])).second,"Duplicate baseline prediction key");}return result;}
void metric(std::ofstream& f,const std::string& split,int h,const std::string& group,const std::string& model,const Score& s){if(!s.n)return;const double n=static_cast<double>(s.n);const double cov=s.spy/n-(s.sp/n)*(s.sy/n);const double vx=s.sp2/n-(s.sp/n)*(s.sp/n),vy=s.sy2/n-(s.sy/n)*(s.sy/n);const double corr=(vx>1e-25&&vy>1e-25)?cov/std::sqrt(vx*vy):0.;f<<split<<','<<h<<','<<group<<','<<model<<','<<s.n<<','<<std::sqrt(s.sq/n)<<','<<std::sqrt(s.zeroSq/n)<<','<<s.abs/n<<','<<static_cast<double>(s.correct)/n<<','<<static_cast<double>(s.positive)/n<<','<<static_cast<double>(s.predpositive)/n<<','<<corr<<','<<s.sp/n<<','<<s.sy/n<<'\n';}
}
int main(int argc,char**argv){try{
check(argc==5,"Usage: run_cete_h47_stage6c1b_enhanced <enriched.cache> <stage6c1/forecast_predictions.csv> <output_dir> <hypothetical_position_INR>");
const fs::path out=argv[3];fs::create_directories(out);const double position=std::stod(argv[4]);check(std::isfinite(position)&&position>0,"Invalid position amount");
const auto old=loadBaseline(argv[2]);auto data=ds::DevHistoricalTrainingCache::load(argv[1]);ds::DevHistoricalTrainingDatasetValidator::validate(data);
std::set<std::int64_t> days;std::vector<Sample> samples;std::size_t candidates=0,eligibleCount=0,skipped=0;
for(const auto& r:data.records){days.insert(dayIST(r.decision_time));for(const auto& c:r.candidates){++candidates;const bool ok=eligible(c.observation);eligibleCount+=ok;const bool usable=r.numerical_integrity&&c.numerical_integrity&&c.observation.ready&&c.observation.numerical_integrity;const V x=features(c.observation,static_cast<int>(c.candidate_rank),r.decision_time);for(const auto& o:c.outcomes){if(o.horizon_bars!=5&&o.horizon_bars!=10&&o.horizon_bars!=15)continue;if(!usable||!o.available||!o.numerical_integrity||!std::isfinite(o.forward_return)||o.outcome_time<=r.decision_time){++skipped;continue;}samples.push_back({r.decision_time,o.outcome_time,static_cast<int>(o.horizon_bars),static_cast<int>(c.candidate_rank),c.symbol,ok,x,o.forward_return});}}}
check(days.size()>=10,"Insufficient historical sessions");std::vector<std::int64_t> d(days.begin(),days.end());const std::size_t ntrain=d.size()*6/10,nval=d.size()*2/10;check(ntrain>=2&&nval>=1&&ntrain+nval<d.size(),"Invalid chronological split");const auto valStart=d[ntrain]*86400-19800,testStart=d[ntrain+nval]*86400-19800;
std::ofstream metrics(out/"enhanced_metrics.csv"),preds(out/"enhanced_predictions.csv"),selection(out/"model_selection.csv"),summary(out/"run_summary.csv");check(bool(metrics)&&bool(preds)&&bool(selection)&&bool(summary),"Cannot write reports");for(auto* f:{&metrics,&preds,&selection,&summary})*f<<std::setprecision(15);
metrics<<"split,horizon_bars,group,model,count,rmse,rmse_zero,mae,directional_accuracy,positive_actual_fraction,positive_prediction_fraction,pearson,mean_predicted,mean_realized\n";
preds<<"split,decision_time,outcome_time,symbol,rank,horizon_bars,exact_signal_eligible,enhanced_predicted_return,baseline_predicted_return,realized_return,enhanced_predicted_percent,enhanced_predicted_gross_INR,realized_gross_INR\n";
selection<<"horizon_bars,lambda,validation_rmse,validation_samples,selected,test_fit_purged_labels\n";
std::size_t matched=0;bool improvedAll=true;
for(int h:{5,10,15}){
const std::array<double,5> lambdas{0.0001,0.001,0.01,0.1,1.};std::vector<std::tuple<double,double,std::size_t>> scores;
for(double lambda:lambdas){const auto m=fit(samples,h,valStart,lambda);Score sc;for(const auto& s:samples)if(s.h==h&&s.t>=valStart&&s.t<testStart)sc.add(predict(m,s.x),s.y);check(sc.n>0,"Empty validation period");scores.emplace_back(lambda,std::sqrt(sc.sq/sc.n),sc.n);}
const auto best=std::min_element(scores.begin(),scores.end(),[](const auto& a,const auto& b){return std::get<1>(a)<std::get<1>(b);});const double selected=std::get<0>(*best);
const auto valModel=fit(samples,h,valStart,selected),testModel=fit(samples,h,testStart,selected);
for(const auto& z:scores)selection<<h<<','<<std::get<0>(z)<<','<<std::get<1>(z)<<','<<std::get<2>(z)<<','<<(std::get<0>(z)==selected?1:0)<<','<<testModel.n<<'\n';
std::map<std::tuple<std::string,std::string,std::string>,Score> agg;
for(const auto& s:samples){if(s.h!=h||s.t<valStart)continue;const bool test=s.t>=testStart;const std::string split=test?"test":"validation",group=s.eligible?"eligible":"ineligible";const double p=predict(test?testModel:valModel,s.x);
const Key k{split,s.t,s.symbol,h};const auto it=old.find(k);check(it!=old.end(),"Missing matching original prediction");const double baseline=it->second;++matched;
preds<<split<<','<<s.t<<','<<s.end<<','<<s.symbol<<','<<s.rank<<','<<h<<','<<(s.eligible?1:0)<<','<<p<<','<<baseline<<','<<s.y<<','<<p*100<<','<<p*position<<','<<s.y*position<<'\n';
for(const auto& g:{std::string("all"),group}){agg[{split,g,"enhanced"}].add(p,s.y);agg[{split,g,"original_ridge"}].add(baseline,s.y);}}
for(const auto& [key,sc]:agg)metric(metrics,std::get<0>(key),h,std::get<1>(key),std::get<2>(key),sc);
const auto& e=agg.at({"test","all","enhanced"});const auto& b=agg.at({"test","all","original_ridge"});const bool improved=e.sq<b.sq;improvedAll&=improved;
std::cout<<"[6C1B] h="<<h<<" lambda="<<selected<<" test enhanced_RMSE="<<std::sqrt(e.sq/e.n)<<" original_RMSE="<<std::sqrt(b.sq/b.n)<<" improvement="<<(improved?"YES":"NO")<<'\n';
}
summary<<"metric,value\n"<<"sessions,"<<d.size()<<"\ntrain_sessions,"<<ntrain<<"\nvalidation_sessions,"<<nval<<"\ntest_sessions,"<<d.size()-ntrain-nval<<"\n"<<"candidate_observations,"<<candidates<<"\nexact_signal_eligible,"<<eligibleCount<<"\nusable_labels,"<<samples.size()<<"\nskipped_labels,"<<skipped<<"\nmatched_baseline_predictions,"<<matched<<"\nhypothetical_position_INR,"<<position<<"\nall_horizons_improved_test_rmse,"<<(improvedAll?1:0)<<"\n"<<"note,Feature inputs are centered cross-sectional percentiles and availability masks; not raw mean or standard deviation\n"<<"note_2,Test is an internal holdout from the existing validation cache; not independent external OOS\n"<<"note_3,Repeated stock and overlapping horizon samples are correlated\n";
std::cout<<"[CETE][H47][5E2K6C1B] OFFLINE ENHANCED FORECAST AUDIT COMPLETE; baseline matches="<<matched<<"; reports="<<out<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"[CETE][H47][5E2K6C1B] FAIL: "<<e.what()<<'\n';return 1;}}
