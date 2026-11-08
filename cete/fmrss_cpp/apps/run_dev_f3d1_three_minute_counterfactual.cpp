// D9F.4C-F3E-D1: isolated baseline vs 3-minute holding counterfactual.
// Read-only: no training, checkpoint writes, or live orders.
#include "devai/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
using devai::strategy::d9f4c::CandleReplayConfig;
using devai::strategy::d9f4c::CandleReplayReport;
using devai::strategy::d9f4c::DevCandleManagerReplay;
static void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
struct Stats {size_t decisions{},switches{},opens{},exits{},holds{},waits{},blocked{},risk_exits{},terminal_exits{},legs{};double gross{},costs{},net{},equity{};};
static Stats summarize(const CandleReplayReport& r,bool experimental){
 Stats x{};x.decisions=r.steps.size();x.legs=r.executed_legs;x.equity=r.final_equity;
 check(r.steps.size()==r.audit.size(),"step/audit length mismatch");
 for(size_t i=0;i<r.steps.size();++i){
  const auto& s=r.steps[i];const auto& a=r.audit[i];
  check(s.decision_time==a.decision_time,"step/audit timestamp mismatch");
  const bool before=!a.held_before.empty(),after=!a.held_after.empty();
  if(before && after && a.held_before!=a.held_after)++x.switches;
  else if(!before && after)++x.opens;
  else if(before && !after)++x.exits;
  else if(before)++x.holds;
  else ++x.waits;
  x.gross+=s.reward.gross_reward; x.net+=s.reward.net_reward;
  x.costs+=s.reward.gross_reward-s.reward.net_reward;
  if(experimental && before && a.raw_action!=a.final_action &&
     a.supervisor_recommendation!="EXIT_REQUIRED" && !s.terminal)++x.blocked;
  if(before && !after && a.supervisor_recommendation=="EXIT_REQUIRED")++x.risk_exits;
  if(s.terminal && after)throw std::runtime_error("position remained at 15:15");
  if(s.terminal && before && !after)++x.terminal_exits;
  const int m=DevCandleManagerReplay::minuteIST(s.decision_time);
  if(m>=14*60+45 && after && a.held_after!=a.held_before)
    throw std::runtime_error("new entry or switch at/after 14:45");
  if(experimental && before && after && a.held_before!=a.held_after &&
     a.holding_age_minutes<3 && a.supervisor_recommendation!="EXIT_REQUIRED")
    throw std::runtime_error("ordinary switch before 3 minutes");
  if(experimental && before && !after && !s.terminal &&
     a.holding_age_minutes<3 && a.supervisor_recommendation!="EXIT_REQUIRED")
    throw std::runtime_error("ordinary exit before 3 minutes");
 }
 check(std::abs((r.final_equity-7000.)-x.net)<1e-5,"equity/net P&L reconciliation failed");
 return x;
}
static void writeSummary(const fs::path& p,const Stats& a,const Stats& b){
 std::ofstream f(p);check(bool(f),"cannot open comparison CSV");
 f<<"metric,baseline,three_minute\n"<<std::setprecision(17);
 auto line=[&](const char* k,auto u,auto v){f<<k<<','<<u<<','<<v<<'\n';};
 line("decisions",a.decisions,b.decisions);line("opens",a.opens,b.opens);
 line("switches",a.switches,b.switches);line("holds",a.holds,b.holds);
 line("exits",a.exits,b.exits);line("waits",a.waits,b.waits);
 line("executed_legs",a.legs,b.legs);line("gross_pnl",a.gross,b.gross);
 line("execution_costs",a.costs,b.costs);line("net_pnl",a.net,b.net);
 line("final_equity",a.equity,b.equity);
 line("risk_required_exits",a.risk_exits,b.risk_exits);
 line("terminal_exits",a.terminal_exits,b.terminal_exits);
 line("raw_final_action_differences",a.blocked,b.blocked);
 check(bool(f),"comparison write failed");
}
int main(int argc,char** argv){try{
 if(argc!=5){std::cerr<<"Usage: run_dev_f3d1_three_minute_counterfactual <cache> <minute_folder> <checkpoint> <output_directory>\n";return 2;}
 CandleReplayConfig cfg{};cfg.cache_file=argv[1];cfg.minute_folder=argv[2];cfg.shared_checkpoint=argv[3];
 fs::path out=argv[4];
 check(fs::is_regular_file(cfg.cache_file),"cache not found");
 check(fs::is_directory(cfg.minute_folder),"minute folder not found");
 check(fs::is_regular_file(cfg.shared_checkpoint),"checkpoint not found");
 check(!fs::exists(out)||fs::is_directory(out),"output path is not directory");
 fs::create_directories(out);
 check(!fs::exists(out/"baseline.csv")&&!fs::exists(out/"three_minute.csv")&&
       !fs::exists(out/"comparison.csv"),"output already exists; refusing overwrite");
 cfg.minimum_holding_minutes=0;
 auto baseline=DevCandleManagerReplay::run(cfg);
 auto bs=summarize(baseline,false);
 check(bs.decisions==360,"baseline not 360 decisions");
 check(bs.switches==281,"baseline mismatch: expected 281 switches");
 check(std::abs(bs.equity-4600.602415)<1e-3,"baseline equity mismatch");
 cfg.minimum_holding_minutes=3;
 auto experiment=DevCandleManagerReplay::run(cfg);
 auto es=summarize(experiment,true);
 check(es.decisions==360,"counterfactual not 360 decisions");
 DevCandleManagerReplay::writeCsv(out/"baseline.csv",baseline);
 DevCandleManagerReplay::writeCsv(out/"three_minute.csv",experiment);
 writeSummary(out/"comparison.csv",bs,es);
 std::cout<<std::fixed<<std::setprecision(4)
  <<"D9F.4C-F3E-D1 — 3-MINUTE COUNTERFACTUAL\n"
  <<"Baseline: switches="<<bs.switches<<" costs="<<bs.costs<<" net="<<bs.net<<" equity="<<bs.equity<<'\n'
  <<"3-minute: switches="<<es.switches<<" costs="<<es.costs<<" net="<<es.net<<" equity="<<es.equity<<'\n'
  <<"Difference in net P&L: "<<es.net-bs.net<<'\n'
  <<"Action differences (including overrides): "<<es.blocked<<'\n'
  <<"Emergency/risk exits: "<<es.risk_exits<<" | Terminal exits: "<<es.terminal_exits<<'\n'
  <<"F3E-D1 COUNTERFACTUAL: PASS\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"F3E-D1 COUNTERFACTUAL: FAILED: "<<e.what()<<'\n';return 1;}}
