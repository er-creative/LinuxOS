#include "devai/strategy/dev_ai_strategy/forecast/DevForwardForecast_F3EF2.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
using namespace devai::forecast::f3ef2;
namespace fs=std::filesystem;
int main(int argc,char**argv){try{
 if(argc!=4){std::cerr<<"Usage: "<<argv[0]<<" <f1c_csv_dir> <shares_txt> <output_dir>\n";return 2;}
 fs::path input=argv[1],output=argv[3];fs::create_directories(output);
 std::ifstream sf(argv[2]);if(!sf)throw std::runtime_error("Cannot read shares file");std::vector<std::string> paths;std::string s;
 while(std::getline(sf,s)){auto a=s.find_first_not_of(" \t\r");if(a==std::string::npos||s[a]=='#')continue;auto b=s.find_last_not_of(" \t\r");s=s.substr(a,b-a+1);if(s=="NIFTY%2050"||s=="NIFTY 50")continue;
 fs::path p=input/(s+"_training_f3ef1c.csv");if(!fs::exists(p))throw std::runtime_error("Missing input "+p.string());paths.push_back(p.string());}
 if(paths.empty())throw std::runtime_error("No stock files");
 auto scan=[&](const std::function<void(const Row&)>& cb){for(auto& p:paths)read_csv(p,cb);};
 std::set<int> dates;scan([&](const Row&r){dates.insert(r.session);});
 if(dates.size()<20)throw std::runtime_error("Need at least 20 distinct sessions for chronological split");
 std::vector<int> sessions(dates.begin(),dates.end());size_t n=sessions.size();int val_start=sessions[static_cast<size_t>(n*.70)],test_start=sessions[static_cast<size_t>(n*.85)];
 auto partition=[&](const Row&r){return r.session<val_start?0:(r.session<test_start?1:2);};
 std::cout<<"F3E-F2 symbols="<<paths.size()<<" sessions="<<n<<" train_before="<<val_start<<" validation_before="<<test_start<<" test_from="<<test_start<<"\n";
 Moments mom;scan([&](const Row&r){if(partition(r)==0)mom.add(r.x);});mom.finish();
 std::array<Accumulator,H> acc{};scan([&](const Row&r){if(partition(r)!=0)return;auto z=mom.transform(r.x);for(int h=0;h<H;++h)if(r.valid[h])acc[h].add(z,r.y[h]);});
 constexpr std::array<double,4> lambdas{1.,100.,10000.,1000000.};
 std::array<std::array<std::array<double,D>,H>,4> candidates{};
 for(int c=0;c<4;++c)for(int h=0;h<H;++h){if(acc[h].n<100)throw std::runtime_error("Insufficient training labels");candidates[c][h]=solve(acc[h],lambdas[c]);}
 std::array<std::array<Metrics,H>,4> val{};
 scan([&](const Row&r){if(partition(r)!=1)return;auto z=mom.transform(r.x);for(int h=0;h<H;++h)if(r.valid[h])for(int c=0;c<4;++c)val[c][h].add(r.y[h],predict(candidates[c][h],z),acc[h].ysum/acc[h].n);});
 Model model;model.moments=mom;std::array<double,H> chosen{};
 for(int h=0;h<H;++h){int best=-1;double best_loss=std::numeric_limits<double>::infinity();for(int c=0;c<4;++c){if(val[c][h].n==0)continue;double loss=val[c][h].se/val[c][h].n;if(loss<best_loss){best_loss=loss;best=c;}}
 if(best<0)throw std::runtime_error("No validation labels");model.weights[h]=candidates[best][h];model.baselines[h]=acc[h].ysum/acc[h].n;chosen[h]=lambdas[best];}
 std::array<Metrics,H> test{};scan([&](const Row&r){if(partition(r)!=2)return;auto z=mom.transform(r.x);for(int h=0;h<H;++h)if(r.valid[h])test[h].add(r.y[h],predict(model.weights[h],z),model.baselines[h]);});
 model.save((output/"forward_forecast_f3ef2.model").string());
 std::ofstream report(output/"metrics_f3ef2.csv");report<<"horizon_minutes,train_labels,validation_labels,test_labels,ridge_lambda,validation_rmse,test_rmse,test_mae,baseline_test_rmse,baseline_test_mae,test_direction_accuracy,test_mean_actual_return\n"<<std::setprecision(12);
 for(int h=0;h<H;++h){int best=0;for(int c=0;c<4;++c)if(lambdas[c]==chosen[h])best=c;auto& v=val[best][h];auto& t=test[h];if(!t.n)throw std::runtime_error("No test labels");
 report<<horizons[h]<<','<<acc[h].n<<','<<v.n<<','<<t.n<<','<<chosen[h]<<','<<std::sqrt(v.se/v.n)<<','<<std::sqrt(t.se/t.n)<<','<<t.ae/t.n<<','<<std::sqrt(t.base_se/t.n)<<','<<t.base_ae/t.n<<','<<t.dir_correct/t.n<<','<<t.ysum/t.n<<'\n';
 std::cout<<"horizon="<<horizons[h]<<"m train="<<acc[h].n<<" val="<<v.n<<" test="<<t.n<<" test_rmse="<<std::sqrt(t.se/t.n)<<" baseline_rmse="<<std::sqrt(t.base_se/t.n)<<"\n";}
 std::ofstream manifest(output/"split_f3ef2.txt");manifest<<"training sessions before "<<val_start<<"\nvalidation sessions from "<<val_start<<" before "<<test_start<<"\ntest sessions from "<<test_start<<"\nAll horizon targets come from F3E-F1C; no target used as feature. Model chosen on validation only.\n";
 std::cout<<"F3E-F2 COMPLETE model="<<(output/"forward_forecast_f3ef2.model")<<" metrics="<<(output/"metrics_f3ef2.csv")<<"\n";
 }catch(const std::exception&e){std::cerr<<"F3E-F2 ERROR: "<<e.what()<<'\n';return 1;}}
