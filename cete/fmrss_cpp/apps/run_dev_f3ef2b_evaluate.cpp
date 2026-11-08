#include "devai/strategy/dev_ai_strategy/forecast/DevForecastEvaluation_F3EF2B.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
using namespace devai::forecast::f3ef2b;
namespace fs=std::filesystem;
int main(int argc,char**argv){try{
 if(argc!=7){std::cerr<<"Usage: "<<argv[0]<<" <F1C_dir> <shares.txt> <F2_model> <split.txt> <output_dir> <round_trip_cost_bps>\n";return 2;}
 const fs::path dir=argv[1],out=argv[5];const Model model=Model::load(argv[3]);double bps=num(argv[6]);if(bps<0||bps>1000)throw std::runtime_error("Cost bps outside 0..1000");double cost=bps/10000.;
 std::ifstream splitFile(argv[4]);if(!splitFile)throw std::runtime_error("Cannot open split file");std::string line;int testFrom=0;while(std::getline(splitFile,line)){auto p=line.find("test sessions from ");if(p!=std::string::npos)testFrom=std::stoi(line.substr(p+19));}if(testFrom<20200101)throw std::runtime_error("Missing test boundary");
 std::ifstream shares(argv[2]);if(!shares)throw std::runtime_error("Cannot open shares file");std::vector<std::string> symbols;std::set<std::string> unique;
 while(std::getline(shares,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();auto a=line.find_first_not_of(" \t");if(a==std::string::npos||line[a]=='#')continue;auto b=line.find_last_not_of(" \t");std::string s=line.substr(a,b-a+1);if(s=="NIFTY%2050"||s=="NIFTY 50")continue;if(unique.insert(s).second)symbols.push_back(s);}
 if(symbols.empty())throw std::runtime_error("No tradable symbols");fs::create_directories(out);
 std::array<Stats,5> global{};std::map<std::string,std::array<Stats,5>> perSymbol;std::vector<Record> rows;
 for(const auto& sym:symbols){std::ifstream f(dir/(sym+"_training_f3ef1c.csv"));if(!f)throw std::runtime_error("Missing dataset: "+sym);std::string header;if(!std::getline(f,header)||header!="symbol,candle_start_epoch,decision_epoch,session_id,close,mean20,std20,z20,return_1m_past,nifty_return_1m_past,valid_1m,target_return_1m,valid_3m,target_return_3m,valid_5m,target_return_5m,valid_10m,target_return_10m,valid_15m,target_return_15m")throw std::runtime_error("Unexpected F1C header: "+sym);
 long long count=0;while(std::getline(f,line)){if(line.empty())continue;auto v=split(line);int session=std::stoi(v.at(3));if(session<testFrom)continue;Record r=parse(v,model);if(r.symbol!=sym)throw std::runtime_error("Symbol mismatch");for(int h=0;h<5;++h)if(r.valid[h]){global[h].add(r.pred[h],r.actual[h],model.baseline[h],cost);perSymbol[sym][h].add(r.pred[h],r.actual[h],model.baseline[h],cost);}rows.push_back(std::move(r));++count;}std::cout<<"PASS "<<sym<<" heldout_rows="<<count<<'\n';}
 std::ofstream metric(out/"forecast_quality_f3ef2b.csv");metric<<"group,key,horizon_minutes,n,rmse,baseline_rmse,mae,direction_accuracy,better_than_baseline_fraction,mean_pred,mean_actual,mean_actual_minus_cost\n";
 for(int h=0;h<5;++h)statsRow(metric,"all","ALL",H[h],global[h]);for(const auto& [sym,s]:perSymbol)for(int h=0;h<5;++h)statsRow(metric,"symbol",sym,H[h],s[h]);
 // Forecast-strength deciles: determined by absolute predicted return, within each horizon on held-out rows.
 std::ofstream bucket(out/"forecast_strength_f3ef2b.csv");bucket<<"horizon_minutes,abs_pred_bucket,n,mean_abs_pred,mean_actual_signed_to_prediction,positive_direction_fraction,mean_signed_actual_minus_cost\n";
 for(int h=0;h<5;++h){std::vector<std::pair<double,size_t>> order;order.reserve(rows.size());for(size_t i=0;i<rows.size();++i)if(rows[i].valid[h])order.emplace_back(std::abs(rows[i].pred[h]),i);std::sort(order.begin(),order.end());struct B{long long n=0,correct=0;double strength=0,signedActual=0;};std::array<B,10> buckets{};
  for(size_t i=0;i<order.size();++i){int k=std::min(9,int((10LL*i)/std::max<size_t>(1,order.size())));const auto&r=rows[order[i].second];double signedY=(r.pred[h]>=0?1:-1)*r.actual[h];auto&b=buckets[k];++b.n;b.strength+=std::abs(r.pred[h]);b.signedActual+=signedY;b.correct+=(signedY>0);}
  for(int k=0;k<10;++k){const auto&b=buckets[k];if(b.n)bucket<<H[h]<<','<<k+1<<','<<b.n<<','<<csvNum(b.strength/b.n)<<','<<csvNum(b.signedActual/b.n)<<','<<csvNum(double(b.correct)/b.n)<<','<<csvNum(b.signedActual/b.n-cost)<<'\n';}}
 // Compare top predicted stock with a z-score ranking at each decision timestamp.
 // NOT DevManager's actual Top-5, and NOT an executable portfolio simulation.
 std::sort(rows.begin(),rows.end(),[](const Record&a,const Record&b){return a.t<b.t||(a.t==b.t&&a.symbol<b.symbol);});
 struct RankStats{long long n=0,win=0;double forecast=0,statistical=0,top5=0;};std::array<RankStats,5> rank{};
 std::ofstream selections(out/"ranking_examples_f3ef2b.csv");selections<<"decision_epoch,horizon_minutes,universe_size,forecast_top_symbol,forecast_top_pred,forecast_top_realized,statistical_top_symbol,statistical_top_realized,forecast_top5_mean_realized\n";long long examples=0;
 for(size_t a=0;a<rows.size();){size_t b=a+1;while(b<rows.size()&&rows[b].t==rows[a].t)++b;
  for(int h=0;h<5;++h){std::vector<const Record*> candidates;for(size_t i=a;i<b;++i)if(rows[i].valid[h])candidates.push_back(&rows[i]);if(candidates.size()<5)continue;
   std::sort(candidates.begin(),candidates.end(),[h](auto*x,auto*y){return x->pred[h]>y->pred[h]||(x->pred[h]==y->pred[h]&&x->symbol<y->symbol);});const auto* best=candidates.front();double top5=0;for(int i=0;i<5;++i)top5+=candidates[i]->actual[h]/5.;
   auto stat=*std::max_element(candidates.begin(),candidates.end(),[](auto*x,auto*y){return x->z<y->z;});auto& r=rank[h];++r.n;r.forecast+=best->actual[h];r.statistical+=stat->actual[h];r.top5+=top5;r.win+=(best->actual[h]>stat->actual[h]);
   if(h==3&&examples<10000){selections<<rows[a].t<<','<<H[h]<<','<<candidates.size()<<','<<best->symbol<<','<<csvNum(best->pred[h])<<','<<csvNum(best->actual[h])<<','<<stat->symbol<<','<<csvNum(stat->actual[h])<<','<<csvNum(top5)<<'\n';++examples;}
  }a=b;}
 std::ofstream ranking(out/"ranking_comparison_f3ef2b.csv");ranking<<"horizon_minutes,decision_times,forecast_top1_mean_realized,statistical_z_top1_mean_realized,forecast_top5_mean_realized,forecast_top1_minus_cost,statistical_top1_minus_cost,forecast_top1_beats_stat_fraction\n";
 for(int h=0;h<5;++h){auto&r=rank[h];if(!r.n)continue;ranking<<H[h]<<','<<r.n<<','<<csvNum(r.forecast/r.n)<<','<<csvNum(r.statistical/r.n)<<','<<csvNum(r.top5/r.n)<<','<<csvNum(r.forecast/r.n-cost)<<','<<csvNum(r.statistical/r.n-cost)<<','<<csvNum(double(r.win)/r.n)<<'\n';}
 std::cout<<"F3E-F2B COMPLETE symbols="<<symbols.size()<<" heldout_rows="<<rows.size()<<" test_from="<<testFrom<<" round_trip_cost_bps="<<bps<<" output="<<out<<"\n";
 }catch(const std::exception&e){std::cerr<<"F3E-F2B ERROR: "<<e.what()<<'\n';return 1;}}
