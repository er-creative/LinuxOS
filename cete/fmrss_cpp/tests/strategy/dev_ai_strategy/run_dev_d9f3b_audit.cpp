#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using Row=std::map<std::string,std::string>;
std::vector<std::string> split(const std::string&s){std::vector<std::string>v;std::stringstream in(s);std::string x;while(std::getline(in,x,','))v.push_back(x);if(!s.empty()&&s.back()==',')v.push_back("");return v;}
std::vector<Row> read(const fs::path&p){std::ifstream in(p);if(!in)throw std::runtime_error("Cannot read "+p.string());std::string line;std::getline(in,line);auto hdr=split(line);std::vector<Row>out;while(std::getline(in,line)){if(line.empty())continue;auto vals=split(line);if(vals.size()!=hdr.size())throw std::runtime_error("CSV width mismatch");Row r;for(size_t i=0;i<hdr.size();++i)r[hdr[i]]=vals[i];out.push_back(std::move(r));}return out;}
std::string val(const Row&r,const std::string&k){auto it=r.find(k);if(it==r.end())throw std::runtime_error("Missing "+k);return it->second;}
bool number(const Row&r,const std::string&k,double&v){auto s=val(r,k);if(s.empty())return false;try{size_t n=0;v=std::stod(s,&n);return n==s.size()&&std::isfinite(v);}catch(...){return false;}}
struct Stats{size_t n=0,correct=0,negative=0;double pred=0,actual=0,error=0,absolute=0,square=0,xx=0,yy=0,xy=0;void add(double p,double a){++n;pred+=p;actual+=a;error+=p-a;absolute+=std::abs(p-a);square+=(p-a)*(p-a);xx+=p*p;yy+=a*a;xy+=p*a;if((p>0&&a>0)||(p<0&&a<0))++correct;if(p<0)++negative;}double corr()const{double x=n*xx-pred*pred,y=n*yy-actual*actual;return x>0&&y>0?(n*xy-pred*actual)/std::sqrt(x*y):0;}};
int main(int argc,char**argv){try{
if(argc!=5)throw std::runtime_error("Usage: run_dev_d9f3b_audit forward_audit.csv trade_ledger.csv output_directory assumed_cost_bps");
size_t parsed=0;double costbps=std::stod(argv[4],&parsed);if(parsed!=std::string(argv[4]).size()||!std::isfinite(costbps)||costbps<0)throw std::runtime_error("Invalid cost bps");double cost=costbps/10000.;
auto audit=read(argv[1]),trades=read(argv[2]);fs::path dir=argv[3];fs::create_directories(dir);
std::ofstream cal(dir/"ForecastCalibration_D9F3B.csv"),cov(dir/"ForecastCoverage_D9F3B.csv"),att(dir/"TradeForecastAttribution_D9F3B.csv");if(!cal||!cov||!att)throw std::runtime_error("Cannot write outputs");
cal<<"phase,symbol,n,mean_predicted_fraction,mean_actual_fraction,mean_error_fraction,mae_fraction,rmse_fraction,strict_sign_accuracy,negative_forecasts,pearson\n";
cov<<"phase,symbol,rows,forecast_ready,horizon_available,valid_pairs\n";
att<<"phase,symbol,entry_key,exit_key,holding_minutes,quantity,entry_predicted_price,entry_predicted_return,entry_actual_price,entry_actual_return,exit_predicted_price,exit_predicted_return,exit_actual_price,exit_actual_return,negative_entry_forecast,entry_predicted_exceeds_assumed_cost,net_pnl,exit_reason\n";
using Key=std::pair<std::string,std::string>;struct Coverage{size_t rows=0,ready=0,horizon=0,pairs=0;};std::map<Key,Coverage> coverage;std::map<Key,Stats> stats;
for(const auto&r:audit){auto phase=val(r,"phase");if(phase!="TRAIN"&&phase!="VALIDATION")throw std::runtime_error("OOS phase forbidden");auto sym=val(r,"symbol");auto k=Key{phase,sym};auto&c=coverage[k];++c.rows;if(val(r,"forecast_ready")=="1")++c.ready;if(val(r,"horizon_available")=="1")++c.horizon;double p,a;if(number(r,"predicted_5bar_return",p)&&number(r,"actual_5bar_return",a)){stats[k].add(p,a);stats[{phase,"ALL"}].add(p,a);++c.pairs;}}
for(const auto&[k,s]:stats)cal<<k.first<<','<<k.second<<','<<s.n<<','<<s.pred/s.n<<','<<s.actual/s.n<<','<<s.error/s.n<<','<<s.absolute/s.n<<','<<std::sqrt(s.square/s.n)<<','<<static_cast<double>(s.correct)/s.n<<','<<s.negative<<','<<s.corr()<<'\n';
for(const auto&[k,c]:coverage)cov<<k.first<<','<<k.second<<','<<c.rows<<','<<c.ready<<','<<c.horizon<<','<<c.pairs<<'\n';
size_t negative=0,valid=0;double net=0;
for(const auto&r:trades){auto phase=val(r,"phase");if(phase!="TRAIN"&&phase!="VALIDATION")throw std::runtime_error("OOS ledger forbidden");double p=0,n=0;bool ready=number(r,"entry_predicted_5bar_return",p);if(ready){++valid;if(p<0)++negative;}if(!number(r,"net_pnl",n))throw std::runtime_error("Invalid P&L");net+=n;
for(const auto&key:{"phase","symbol","entry_key","exit_key","holding_minutes","quantity","entry_predicted_5bar_price","entry_predicted_5bar_return","entry_actual_5bar_price","entry_actual_5bar_return","exit_predicted_5bar_price","exit_predicted_5bar_return","exit_actual_5bar_price","exit_actual_5bar_return"})att<<val(r,key)<<',';
att<<(ready?(p<0?"1":"0"):"")<<','<<(ready?(p>cost?"1":"0"):"")<<','<<val(r,"net_pnl")<<','<<val(r,"exit_reason")<<'\n';}
std::cout<<"D9F3B_COMPLETE audit_rows="<<audit.size()<<" trades="<<trades.size()<<" entry_forecasts_available="<<valid<<" negative_entry_forecasts="<<negative<<" ledger_net="<<net<<" assumed_cost_bps="<<costbps<<" oos_evaluated=no forecast_horizon_verified=no q_attribution=pending\n";
}catch(const std::exception&e){std::cerr<<"D9F3B_FAIL "<<e.what()<<'\n';return 1;}return 0;}
