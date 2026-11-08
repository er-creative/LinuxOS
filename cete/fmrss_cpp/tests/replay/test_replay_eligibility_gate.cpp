#include "cete/replay/ReplayEligibilityGate.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using cete::replay::ReplayEligibilityGate;
static void require(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
static void write(const fs::path& path,const std::string& contents){std::ofstream f(path);if(!f)throw std::runtime_error("Cannot write fixture");f<<contents;}
int main(int argc,char**argv){
 try {
  // A tiny deterministic fixture exercises the causal boundary independently of user data.
  fs::path p=fs::temp_directory_path()/"cete_hybrid_gate_fixture";
  fs::create_directories(p);
  const std::vector<std::string> symbols={"RELIANCE","NIFTY%2050"};
  const std::vector<std::string> dates={"2026-01-01","2026-01-02"}; // synthetic trading sessions
  std::string daily="symbol,date,decision_data_eligible,full_session_eligible,structural_issues,timeframe_mismatches,reason\n";
  std::string decisions="date,time,symbol,one_minute_present,completed_5m_available,completed_15m_available,benchmark_available,warmup_met,decision_eligible,forward_label_eligible,reason\n";
  std::string sessions="symbol,date,timeframe,expected,observed,missing_trading,missing_closing,coverage\n";
  std::string missing="symbol,date,timeframe,time,classification\n";
  for(auto& date:dates)for(auto& sym:symbols){
   daily+=sym+","+date+",1,1,0,0,OK\n";
   for(int tf:{1,5,15})sessions+=sym+","+date+","+std::to_string(tf)+","+std::to_string(375/tf)+","+std::to_string(375/tf)+",0,0,COMPLETE\n";
   for(int t=555;t<915;++t){
    char buf[6];std::snprintf(buf,sizeof(buf),"%02d:%02d",t/60,t%60);
    decisions+=date+","+buf+","+sym+",1,1,1,1,1,1,0,OK\n";
   }
  }
  write(p/"daily_eligibility.csv",daily);
  write(p/"decision_eligibility.csv",decisions);
  write(p/"session_integrity.csv",sessions);
  write(p/"missing_candles.csv",missing);
  ReplayEligibilityGate gate;
  gate.loadHybrid((p/"daily_eligibility.csv").string(),
      (p/"decision_eligibility.csv").string(),p.string());
  auto first=gate.check("2026-01-01","09:15","RELIANCE");
  require(!first.allowed,"First day with no warmup must be blocked");
  auto opening=gate.check("2026-01-02","09:15","RELIANCE");
  require(opening.allowed&&!opening.openEligible,"Previous session should allow evaluation, not OPEN at 09:15");
  auto one=gate.check("2026-01-02","09:16","RELIANCE");
  require(one.allowed&&one.openEligible&&one.current1m&&!one.current5m&&!one.current15m,"09:16 causal readiness incorrect");
  auto five=gate.check("2026-01-02","09:20","RELIANCE");
  require(five.current5m&&!five.current15m,"09:20 5m readiness incorrect");
  auto twentyNine=gate.check("2026-01-02","09:29","RELIANCE");
  require(!twentyNine.current15m,"09:29 must not see 15m candle");
  auto thirty=gate.check("2026-01-02","09:30","RELIANCE");
  require(thirty.current15m,"09:30 must see 15m candle");
  require(!gate.check("2026-01-02","15:15","RELIANCE").allowed,"15:15 must block new decisions");
  require(!thirty.forwardLabelEligible,"Forward labels must not leak into live decision API");
  auto universe=gate.evaluate("2026-01-02","09:15",{"RELIANCE"},1);
  require(universe.ready,"Historical evaluation at 09:15 should be ready");
  // Missing first minute of day 2 blocks new OPEN at 09:16 but not historical evaluation.
  missing+="RELIANCE,2026-01-02,1,09:15,TRADING_WINDOW\n";
  write(p/"missing_candles.csv",missing);
  gate.loadHybrid((p/"daily_eligibility.csv").string(),
      (p/"decision_eligibility.csv").string(),p.string());
  auto gap=gate.check("2026-01-02","09:16","RELIANCE");
  require(gap.allowed&&!gap.openEligible,"Missing current 1m must block OPEN");
  fs::remove_all(p);
  std::cout<<"PASS CETE Step 2 hybrid causal eligibility\n"
           <<"09:15 previous-session history: EVALUATION_READY, OPEN_BLOCKED\n"
           <<"09:16: CURRENT_1M_READY\n09:20: CURRENT_5M_READY\n"
           <<"09:29: CURRENT_15M_NOT_READY\n09:30: CURRENT_15M_READY\n"
           <<"15:15: OUTSIDE_DECISION_WINDOW\n";
  if(argc==3){
   ReplayEligibilityGate real;
   fs::path b=argv[1];
   real.loadHybrid((b/"daily_eligibility.csv").string(),
       (b/"decision_eligibility.csv").string(),argv[2]);
   std::cout<<"Real reports loaded: "<<real.dailyRows()<<" daily, "<<real.decisionRows()<<" decisions\n";
   auto s=real.check("2025-10-30","09:15","RELIANCE");
   std::cout<<"2025-10-30 09:15 RELIANCE: "<<s.reason
            <<" (historical evaluation="<<s.allowed<<", OPEN="<<s.openEligible<<")\n";
  } else if(argc!=1){std::cerr<<"Usage: "<<argv[0]<<" [PHASE_3B_DIR PHASE_3A_DIR]\n";return 2;}
  return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
