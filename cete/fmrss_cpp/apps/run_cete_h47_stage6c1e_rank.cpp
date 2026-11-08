// CETE H4.7 Stage 5E.2K.6C.1E: read-only cost-aware forecast ranking audit.
// This program evaluates PRECOMPUTED forecasts. It does not retrain models or claim
// that rolling evaluation of a fixed checkpoint is true walk-forward validation.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace fs=std::filesystem;
namespace {
void check(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
std::vector<std::string> split(const std::string& line){std::vector<std::string> v;std::stringstream ss(line);std::string s;while(std::getline(ss,s,','))v.push_back(s);return v;}
struct Row{std::string split,symbol,model;std::int64_t time=0,end=0;int rank=0,h=0;bool eligible=false;double pred=0,actual=0;};
using Key=std::tuple<std::string,std::int64_t,std::string,int>;
std::map<Key,Row> read(const fs::path& path,bool raw){
 std::ifstream f(path);check(bool(f),"Cannot open "+path.string());std::string line;check(bool(std::getline(f,line)),"Empty CSV: "+path.string());
 const std::string expected=raw?"split,decision_time,outcome_time,symbol,rank,horizon_bars,exact_signal_eligible,model,predicted_return_fraction,realized_return_fraction,predicted_return_percent,predicted_gross_INR,realized_gross_INR,assumed_cost_INR,predicted_net_INR":"split,decision_time,outcome_time,symbol,rank,horizon_bars,exact_signal_eligible,predicted_forward_return,realized_forward_return";
 check(line==expected,"Unexpected CSV header: "+path.string());
 std::map<Key,Row> out;std::size_t lineno=1;
 while(std::getline(f,line)){++lineno;if(line.empty())continue;auto a=split(line);check(a.size()==(raw?15u:9u),"Malformed row "+std::to_string(lineno));Row r;r.split=a[0];r.time=std::stoll(a[1]);r.end=std::stoll(a[2]);r.symbol=a[3];r.rank=std::stoi(a[4]);r.h=std::stoi(a[5]);r.eligible=a[6]=="1";r.model=raw?a[7]:"original_ridge";r.pred=std::stod(a[raw?8:7]);r.actual=std::stod(a[raw?9:8]);check((r.split=="test"||r.split=="validation")&&(r.h==5||r.h==10||r.h==15)&&r.end>r.time&&std::isfinite(r.pred)&&std::isfinite(r.actual),"Invalid row "+std::to_string(lineno));
 check(out.emplace(Key{r.split,r.time,r.symbol,r.h},r).second,"Duplicate row key in "+path.string());}
 return out;
}
struct Agg{int n=0,positive=0,netPositive=0;double gross=0,net=0,pred=0,actual=0;void add(const Row& r,double cost){++n;gross+=r.actual;net+=r.actual-cost;pred+=r.pred;actual+=r.actual;positive+=r.actual>0;netPositive+=r.actual>cost;}};
struct PairAgg{int n=0,concordant=0,discordant=0,tied=0;void add(const Row& a,const Row& b){const double p=a.pred-b.pred,y=a.actual-b.actual;++n;if(std::abs(p)<1e-15||std::abs(y)<1e-15)++tied;else if(p*y>0)++concordant;else ++discordant;}double accuracy()const{return n>tied?double(concordant)/(n-tied):0;}};
std::int64_t day(std::int64_t t){return (t+19800)/86400;}
struct Selected{Row top,bottom;int group=0;double spread=0;};
}
int main(int argc,char**argv){try{
 check(argc==7,"Usage: run_cete_h47_stage6c1e_rank <raw_predictions.csv> <original_forecast_predictions.csv> <output_dir> <position_INR> <cost_fraction> <fold_count>");
 const auto raw=read(argv[1],true),base=read(argv[2],false);const fs::path out=argv[3];fs::create_directories(out);const double capital=std::stod(argv[4]),cost=std::stod(argv[5]);const int folds=std::stoi(argv[6]);check(capital>0&&std::isfinite(capital)&&cost>=0&&cost<1&&folds>=2&&folds<=12,"Invalid capital/cost/folds");
 using GKey=std::tuple<std::string,int,std::int64_t,std::string>;
 std::map<GKey,std::vector<Row>> groups;std::size_t mismatched=0,unmatched=0;
 for(const auto& [k,r]:raw){auto it=base.find(k);if(it==base.end()){++unmatched;continue;}const Row& b=it->second;
 if(r.end!=b.end||r.rank!=b.rank||r.eligible!=b.eligible||std::abs(r.actual-b.actual)>1e-10){++mismatched;continue;}
 if(!r.eligible)continue;
 groups[GKey{r.split,r.h,r.time,"raw"}].push_back(r);
 groups[GKey{r.split,r.h,r.time,"original_ridge"}].push_back(b);
 }
 check(mismatched==0,"Raw and baseline labels/eligibility differ. Aborting invalid comparison.");
 check(!groups.empty(),"No matched eligible observations");
 std::ofstream rank(out/"ranking_metrics.csv"),events(out/"ranking_events.csv"),fold(out/"chronological_folds.csv"),costs(out/"cost_sensitivity.csv"),summary(out/"run_summary.csv");
 check(bool(rank)&&bool(events)&&bool(fold)&&bool(costs)&&bool(summary),"Cannot create output reports");
 for(auto* f:{&rank,&events,&fold,&costs,&summary})*f<<std::setprecision(15);
 rank<<"split,horizon_bars,model,decision_groups,observations,pairwise_comparisons,pairwise_accuracy,top1_mean_gross_fraction,top1_mean_net_fraction,top1_positive_net_fraction,top1_mean_gross_INR,top1_mean_net_INR,top_minus_bottom_mean_fraction\n";
 events<<"split,horizon_bars,model,decision_time,eligible_candidates,top_symbol,top_predicted_return,top_realized_return,bottom_symbol,bottom_predicted_return,bottom_realized_return,top_minus_bottom_realized,top_net_after_cost_fraction,top_net_after_cost_INR,oracle_top_realized_return,oracle_regret_fraction\n";
 fold<<"split,horizon_bars,model,fold,first_decision_time,last_decision_time,groups,mean_top_net_fraction,mean_top_net_INR,positive_net_fraction,pairwise_accuracy,selection_rule\n";
 costs<<"split,horizon_bars,model,cost_fraction,cost_INR,groups,mean_top_net_fraction,mean_top_net_INR,positive_net_fraction\n";
 std::map<std::tuple<std::string,int,std::string>,std::vector<Selected>> selected;
 std::map<std::tuple<std::string,int,std::string>,PairAgg> pairs;
 std::map<std::tuple<std::string,int,std::string>,int> obs;
 std::size_t small=0;
 for(auto& [key,rows]:groups){const auto& [sp,h,t,model]=key;const auto groupkey=std::make_tuple(sp,h,model);
 std::sort(rows.begin(),rows.end(),[](const Row&a,const Row&b){return a.symbol<b.symbol;});
 if(rows.size()<2){++small;continue;}
 for(std::size_t i=0;i<rows.size();++i)for(std::size_t j=i+1;j<rows.size();++j)pairs[groupkey].add(rows[i],rows[j]);
 obs[groupkey]+=int(rows.size());
 const auto cmp=[](const Row&a,const Row&b){if(a.pred!=b.pred)return a.pred<b.pred;return a.symbol>b.symbol;};
 const Row& top=*std::max_element(rows.begin(),rows.end(),cmp);const Row& bottom=*std::min_element(rows.begin(),rows.end(),cmp);
 double oracle=-std::numeric_limits<double>::infinity();for(const auto& r:rows)oracle=std::max(oracle,r.actual);
 selected[groupkey].push_back(Selected{top,bottom,int(rows.size()),top.actual-bottom.actual});
 events<<sp<<','<<h<<','<<model<<','<<t<<','<<rows.size()<<','<<top.symbol<<','<<top.pred<<','<<top.actual<<','<<bottom.symbol<<','<<bottom.pred<<','<<bottom.actual<<','<<top.actual-bottom.actual<<','<<top.actual-cost<<','<<(top.actual-cost)*capital<<','<<oracle<<','<<oracle-top.actual<<'\n';
 }
 const std::vector<double> costGrid{0.,0.0005,0.001,0.002,0.003,0.005};
 for(auto& [key,vec]:selected){const auto& [sp,h,model]=key;std::sort(vec.begin(),vec.end(),[](const Selected&a,const Selected&b){return a.top.time<b.top.time;});
 const auto& p=pairs[key];double gross=0,spread=0;int positive=0;for(const auto& s:vec){gross+=s.top.actual;spread+=s.spread;positive+=s.top.actual>cost;}
 const double n=double(vec.size());rank<<sp<<','<<h<<','<<model<<','<<vec.size()<<','<<obs[key]<<','<<p.n<<','<<p.accuracy()<<','<<gross/n<<','<<gross/n-cost<<','<<double(positive)/n<<','<<capital*gross/n<<','<<capital*(gross/n-cost)<<','<<spread/n<<'\n';
 for(double c:costGrid){int win=0;for(const auto& s:vec)win+=s.top.actual>c;costs<<sp<<','<<h<<','<<model<<','<<c<<','<<c*capital<<','<<vec.size()<<','<<gross/n-c<<','<<capital*(gross/n-c)<<','<<double(win)/n<<'\n';}
 // Chronological stability audit. The forecasts are FIXED; this is NOT model retraining.
 const int actualFolds=std::min(folds,int(vec.size()));for(int fi=0;fi<actualFolds;++fi){const std::size_t begin=vec.size()*fi/actualFolds,end=vec.size()*(fi+1)/actualFolds;if(end<=begin)continue;double sum=0;int wins=0;for(std::size_t i=begin;i<end;++i){sum+=vec[i].top.actual;wins+=vec[i].top.actual>cost;}const double nn=double(end-begin);
 // Fold pairwise accuracy computed from contemporaneous matched rows only.
 PairAgg fp;for(std::size_t i=begin;i<end;++i){auto it=groups.find(GKey{sp,h,vec[i].top.time,model});check(it!=groups.end(),"Missing group");const auto& rows=it->second;for(std::size_t j=0;j<rows.size();++j)for(std::size_t k=j+1;k<rows.size();++k)fp.add(rows[j],rows[k]);}
 fold<<sp<<','<<h<<','<<model<<','<<(fi+1)<<','<<vec[begin].top.time<<','<<vec[end-1].top.time<<','<<(end-begin)<<','<<sum/nn-cost<<','<<capital*(sum/nn-cost)<<','<<double(wins)/nn<<','<<fp.accuracy()<<",fixed_forecast_chronological_audit_NOT_retrained_walk_forward\n";
 }
 }
 summary<<"metric,value\n"<<"matched_eligible_decision_groups,"<<groups.size()<<'\n'<<"unmatched_raw_rows,"<<unmatched<<'\n'<<"mismatched_outcome_or_eligibility_rows,"<<mismatched<<'\n'<<"groups_with_fewer_than_two_eligible_candidates,"<<small<<'\n'<<"position_INR,"<<capital<<'\n'<<"assumed_roundtrip_cost_fraction,"<<cost<<'\n'<<"chronological_fold_count_requested,"<<folds<<'\n'<<"warning,Precomputed fixed-model predictions only; chronological folds are stability audits NOT retrained walk-forward validation\n"<<"warning_2,Each decision group is a counterfactual opportunity; totals cannot be added into an executable portfolio PnL\n"<<"warning_3,Cost input is an illustrative all-in round-trip fraction; supply validated charges and slippage separately\n"<<"warning_4,This evaluates eligible candidates from existing cache; does not reconstruct top-five ranking selection from raw 10-stock universe\n";
 std::cout<<"[CETE][6C1E] Ranking audit complete; groups="<<groups.size()<<" unmatched="<<unmatched<<" mismatched="<<mismatched<<" output="<<out<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<"[CETE][6C1E][ERROR] "<<e.what()<<'\n';return 1;}}
