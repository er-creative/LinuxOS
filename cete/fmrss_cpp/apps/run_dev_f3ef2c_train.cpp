#include "devai/strategy/dev_ai_strategy/forecast/DevForecastImproved_F3EF2C.hpp"
#include <filesystem>
#include <iostream>
#include <functional>
#include <set>
using namespace devai::forecast::f3ef2c;
namespace fs=std::filesystem;
int main(int argc,char**argv){try{
if(argc!=5){std::cerr<<"Usage: "<<argv[0]<<" <F1C_dir> <shares.txt> <output_dir> <capital_INR>\n";return 2;}
fs::path input=argv[1],out=argv[3];double capital=std::stod(argv[4]);if(capital<=0)throw std::runtime_error("capital must be positive");fs::create_directories(out);
std::ifstream symbols(argv[2]);if(!symbols)throw std::runtime_error("cannot open symbols file");std::vector<std::string> names;std::string name;while(std::getline(symbols,name)){if(!name.empty()&&name.back()=='\r')name.pop_back();if(name.empty()||name[0]=='#'||name.find("NIFTY")!=std::string::npos)continue;names.push_back(name);}if(names.empty())throw std::runtime_error("no tradable symbols");
// Reuse the frozen F3E-F2 session split; test sessions never participate in fitting or hyperparameter selection.
constexpr int VAL=20260313,TEST=20260622;
auto scan=[&](const std::function<void(const Row&)>&f){for(auto&sym:names){std::ifstream in(input/(sym+"_training_f3ef1c.csv"));if(!in)throw std::runtime_error("missing dataset: "+sym);std::string line;std::getline(in,line);auto columns=header(line);while(std::getline(in,line)){if(line.empty())continue;auto r=parse(line,columns);if(finite(r))f(r);}}};
Stats stats;std::array<double,5> sum{},count{};scan([&](const Row&r){if(r.session>=VAL)return;stats.add(r.x);for(int h=0;h<5;++h)if(r.valid[h]&&std::isfinite(r.y[h])){sum[h]+=r.y[h];count[h]++;}});
Model model;model.scale.fit(stats);for(int h=0;h<5;++h){if(!count[h])throw std::runtime_error("no labels");model.base[h]=sum[h]/count[h];}
std::array<Normal,5> norm;scan([&](const Row&r){if(r.session>=VAL)return;auto x=model.scale.apply(r.x);for(int h=0;h<5;++h)if(r.valid[h]&&std::isfinite(r.y[h]))norm[h].add(x,r.y[h]);});
constexpr std::array<double,5> lambdas{1e-7,1e-5,1e-3,1e-2,1e-1};std::array<std::array<std::array<double,K+1>,5>,5> candidates{};for(int h=0;h<5;++h)for(int l=0;l<5;++l)candidates[h][l]=solve(norm[h],lambdas[l]);
std::array<std::array<Metric,5>,5> validation{};scan([&](const Row&r){if(r.session<VAL||r.session>=TEST)return;auto x=model.scale.apply(r.x);for(int h=0;h<5;++h)if(r.valid[h]&&std::isfinite(r.y[h]))for(int l=0;l<5;++l)validation[h][l].add(predict(candidates[h][l],x),r.y[h],model.base[h]);});
for(int h=0;h<5;++h){int best=0;for(int l=1;l<5;++l)if(validation[h][l].rmse()<validation[h][best].rmse())best=l;model.lambda[h]=lambdas[best];model.w[h]=candidates[h][best];}
model.save((out/"forward_forecast_f3ef2c.model").string());
std::array<Metric,5> test{};struct Opp{uint64_t n=0,eligible=0,positive=0;double gross=0,cost=0,net=0;};std::array<Opp,5> opp{};
std::ofstream detail(out/"opportunities_f3ef2c.csv");detail<<"symbol,decision_epoch,session_id,horizon_minutes,close,quantity,predicted_return,realized_return,estimated_cost_inr,estimated_net_pnl_inr,eligible\n";detail<<std::setprecision(12);
scan([&](const Row&r){if(r.session<TEST)return;auto pred=model.predict(r);for(int h=0;h<5;++h){if(!r.valid[h]||!std::isfinite(r.y[h]))continue;test[h].add(pred[h],r.y[h],model.base[h]);int qty=static_cast<int>(std::floor((capital*.75)/r.close));if(qty<=0)continue;double sell=r.close*(1+r.y[h]);if(sell<=0)continue;auto charges=zerodha_nse(r.close,sell,qty);double gross=(sell-r.close)*qty,net=gross-charges.total;bool eligible=pred[h]>0 && pred[h]*r.close*qty>zerodha_nse(r.close,r.close,qty).total;auto &o=opp[h];++o.n;if(eligible){++o.eligible;o.gross+=gross;o.cost+=charges.total;o.net+=net;if(net>0)++o.positive;}
// Write only eligible opportunities; these are overlapping hypothetical holds, not portfolio trades.
if(eligible)detail<<r.symbol<<','<<r.epoch<<','<<r.session<<','<<horizons[h]<<','<<r.close<<','<<qty<<','<<pred[h]<<','<<r.y[h]<<','<<charges.total<<','<<net<<",1\n";
}});
std::ofstream metrics(out/"metrics_f3ef2c.csv");metrics<<"horizon_minutes,test_n,model_rmse,baseline_rmse,validation_rmse,lambda,eligible_opportunities,eligible_mean_gross_inr,eligible_mean_cost_inr,eligible_mean_net_inr,eligible_win_fraction\n"<<std::setprecision(14);
for(int h=0;h<5;++h){auto &o=opp[h];double n=o.eligible?double(o.eligible):1.;metrics<<horizons[h]<<','<<test[h].n<<','<<test[h].rmse()<<','<<test[h].baseline()<<',';int li=0;for(int j=0;j<5;++j)if(lambdas[j]==model.lambda[h])li=j;metrics<<validation[h][li].rmse()<<','<<model.lambda[h]<<','<<o.eligible<<','<<o.gross/n<<','<<o.cost/n<<','<<o.net/n<<','<<o.positive/n<<'\n';std::cout<<"horizon="<<horizons[h]<<"m test_rmse="<<test[h].rmse()<<" baseline="<<test[h].baseline()<<" eligible="<<o.eligible<<" mean_net_INR="<<o.net/n<<'\n';}
std::cout<<"F3E-F2C COMPLETE symbols="<<names.size()<<" train_rows="<<stats.n<<" output="<<out<<'\n';
}catch(const std::exception&e){std::cerr<<"F3E-F2C ERROR: "<<e.what()<<'\n';return 1;}}
