#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace fs=std::filesystem;
static void check(bool b,const std::string& m){if(!b)throw std::runtime_error(m);}
static std::vector<std::string> parse(const std::string& s){std::vector<std::string> a;std::string v;bool q=false;for(size_t i=0;i<s.size();++i){char c=s[i];if(c=='"'){if(q&&i+1<s.size()&&s[i+1]=='"'){v+='"';++i;}else q=!q;}else if(c==','&&!q){a.push_back(v);v.clear();}else v+=c;}check(!q,"Unclosed CSV quote");a.push_back(v);return a;}
struct Row{std::string split,symbol;std::int64_t time=0,end=0;int rank=0,h=0,eligible=0;double pred=0,actual=0;};
struct Key{std::string split;int h;std::string group;bool operator<(const Key& o)const{return std::tie(split,h,group)<std::tie(o.split,o.h,o.group);}};
static std::string num(double x){if(!std::isfinite(x))return "NA";return std::to_string(x);}
static double mean(const std::vector<double>& a){return a.empty()?std::numeric_limits<double>::quiet_NaN():std::accumulate(a.begin(),a.end(),0.0)/a.size();}
static double correlation(const std::vector<Row>& a){if(a.size()<2)return std::numeric_limits<double>::quiet_NaN();double px=0,py=0;for(auto& r:a){px+=r.pred;py+=r.actual;}px/=a.size();py/=a.size();double xx=0,yy=0,xy=0;for(auto& r:a){double x=r.pred-px,y=r.actual-py;xx+=x*x;yy+=y*y;xy+=x*y;}return xx>0&&yy>0?xy/std::sqrt(xx*yy):std::numeric_limits<double>::quiet_NaN();}
int main(int argc,char**argv){try{
 check(argc==5||argc==6,"Usage: run_cete_h47_stage6c1a_audit <forecast_predictions.csv> <output_dir> <position_value_INR> <roundtrip_cost_fraction> [optional_validation_only_threshold]");
 const fs::path input=argv[1],out=argv[2];const double position=std::stod(argv[3]),cost=std::stod(argv[4]);check(std::isfinite(position)&&position>0,"Position value must be positive");check(std::isfinite(cost)&&cost>=0&&cost<1,"Roundtrip cost fraction must be between 0 and 1");
 std::ifstream f(input);check(bool(f),"Cannot open predictions CSV: "+input.string());std::string line;check(bool(std::getline(f,line)),"Empty predictions CSV");auto hdr=parse(line);std::map<std::string,size_t> col;for(size_t i=0;i<hdr.size();++i)col[hdr[i]]=i;
 for(const char* k:{"split","decision_time","outcome_time","symbol","rank","horizon_bars","exact_signal_eligible","predicted_forward_return","realized_forward_return"})check(col.count(k)>0,std::string("Missing CSV column: ")+k);
 std::vector<Row> rows;size_t lineno=1;while(std::getline(f,line)){++lineno;if(line.empty())continue;auto c=parse(line);check(c.size()==hdr.size(),"CSV width mismatch line "+std::to_string(lineno));Row r;r.split=c[col.at("split")];r.symbol=c[col.at("symbol")];r.time=std::stoll(c[col.at("decision_time")]);r.end=std::stoll(c[col.at("outcome_time")]);r.rank=std::stoi(c[col.at("rank")]);r.h=std::stoi(c[col.at("horizon_bars")]);r.eligible=std::stoi(c[col.at("exact_signal_eligible")]);r.pred=std::stod(c[col.at("predicted_forward_return")]);r.actual=std::stod(c[col.at("realized_forward_return")]);check(r.split=="validation"||r.split=="test","Unexpected split on line "+std::to_string(lineno));check(r.end>r.time&&std::isfinite(r.pred)&&std::isfinite(r.actual),"Invalid prediction/outcome line "+std::to_string(lineno));check(r.eligible==0||r.eligible==1,"Invalid eligibility");rows.push_back(r);}check(!rows.empty(),"No predictions");fs::create_directories(out);
 std::map<Key,std::vector<Row>> groups;for(const auto& r:rows){groups[{r.split,r.h,"all"}].push_back(r);groups[{r.split,r.h,r.eligible?"eligible":"ineligible"}].push_back(r);}
 std::ofstream metrics(out/"predictive_value_metrics.csv"),bins(out/"prediction_deciles.csv"),opp(out/"forecast_amounts.csv"),summary(out/"audit_summary.csv");check(bool(metrics)&&bool(bins)&&bool(opp)&&bool(summary),"Cannot write outputs");metrics<<std::setprecision(15);bins<<std::setprecision(15);opp<<std::setprecision(15);summary<<std::setprecision(15);
 metrics<<"split,horizon_bars,group,count,positive_actual_fraction,positive_prediction_fraction,positive_forecast_count,positive_forecast_precision,rmse,rmse_zero_baseline,directional_accuracy,directional_accuracy_always_negative,mean_predicted_return,mean_actual_return,pearson_prediction_actual,mean_actual_when_predicted_positive,mean_actual_when_predicted_above_cost,above_cost_count,above_cost_positive_fraction\n";
 bins<<"split,horizon_bars,group,decile_low_to_high,count,mean_predicted_return,mean_realized_return,positive_realized_fraction,mean_predicted_gross_INR,mean_realized_gross_INR\n";
 opp<<"split,decision_time,outcome_time,symbol,rank,horizon_bars,exact_signal_eligible,predicted_return_fraction,realized_return_fraction,predicted_return_percent,realized_return_percent,hypothetical_position_INR,predicted_gross_INR,realized_gross_INR,assumed_roundtrip_cost_INR,predicted_net_INR,realized_net_INR,predicted_exceeds_assumed_cost\n";
 const double fee=position*cost;
 for(const auto& r:rows){opp<<r.split<<','<<r.time<<','<<r.end<<','<<r.symbol<<','<<r.rank<<','<<r.h<<','<<r.eligible<<','<<r.pred<<','<<r.actual<<','<<r.pred*100<<','<<r.actual*100<<','<<position<<','<<r.pred*position<<','<<r.actual*position<<','<<fee<<','<<r.pred*position-fee<<','<<r.actual*position-fee<<','<<(r.pred>cost?1:0)<<'\n';}
 for(auto& [key,v]:groups){double se=0,zero=0,correct=0,negcorrect=0,sumP=0,sumY=0;size_t actualPos=0,predPos=0,predPosCorrect=0,above=0,aboveCorrect=0;double yPredPos=0,yAbove=0;for(const auto& r:v){se+=(r.pred-r.actual)*(r.pred-r.actual);zero+=r.actual*r.actual;correct+=((r.pred>0&&r.actual>0)||(r.pred<0&&r.actual<0))?1:0;negcorrect+=r.actual<0?1:0;sumP+=r.pred;sumY+=r.actual;actualPos+=r.actual>0;predPos+=r.pred>0;predPosCorrect+=r.pred>0&&r.actual>0;if(r.pred>0)yPredPos+=r.actual;if(r.pred>cost){++above;aboveCorrect+=r.actual>0;yAbove+=r.actual;}}
 double n=static_cast<double>(v.size());metrics<<key.split<<','<<key.h<<','<<key.group<<','<<v.size()<<','<<actualPos/n<<','<<predPos/n<<','<<predPos<<','<<(predPos?num(double(predPosCorrect)/predPos):"NA")<<','<<std::sqrt(se/n)<<','<<std::sqrt(zero/n)<<','<<correct/n<<','<<negcorrect/n<<','<<sumP/n<<','<<sumY/n<<','<<num(correlation(v))<<','<<(predPos?num(yPredPos/predPos):"NA")<<','<<(above?num(yAbove/above):"NA")<<','<<above<<','<<(above?num(double(aboveCorrect)/above):"NA")<<'\n';
 std::sort(v.begin(),v.end(),[](const Row& a,const Row& b){if(a.pred!=b.pred)return a.pred<b.pred;return a.time<b.time;});for(int d=0;d<10;++d){size_t lo=v.size()*d/10,hi=v.size()*(d+1)/10;if(hi<=lo)continue;double p=0,y=0;size_t pos=0;for(size_t i=lo;i<hi;++i){p+=v[i].pred;y+=v[i].actual;pos+=v[i].actual>0;}double count=double(hi-lo);bins<<key.split<<','<<key.h<<','<<key.group<<','<<d+1<<','<<hi-lo<<','<<p/count<<','<<y/count<<','<<pos/count<<','<<p/count*position<<','<<y/count*position<<'\n';}}
 summary<<"metric,value\n"<<"input_rows,"<<rows.size()<<'\n'<<"hypothetical_position_INR,"<<position<<'\n'<<"assumed_roundtrip_cost_fraction,"<<cost<<'\n'<<"assumed_roundtrip_cost_INR,"<<fee<<'\n'<<"note,INR values are hypothetical fractional-return conversions; not executable fill-level trade PnL\n"<<"note_2,Overlapping candidate horizons and same-time stock predictions are correlated; no independent significance test is performed\n"<<"note_3,Cost fraction is a scenario assumption and not a Zerodha tariff calculation\n";
 std::cout<<"[CETE][H47][5E2K6C1A] READ-ONLY PREDICTIVE-VALUE AUDIT COMPLETE\nPredictions: "<<rows.size()<<" Hypothetical position: INR "<<position<<" Cost fraction: "<<cost<<" (INR "<<fee<<")\nReports: "<<out<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"[CETE][H47][5E2K6C1A] FAIL: "<<e.what()<<'\n';return 1;}}
