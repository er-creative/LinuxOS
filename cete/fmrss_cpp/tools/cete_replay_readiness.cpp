// CETE Phase 3.19.3B - Replay Readiness and Data Eligibility
// Standalone C++17, read-only. Requires Phase 3.19.3A CSV reports.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
namespace fs = std::filesystem;
struct Coverage {int trading=0, closing=0, expected=0, observed=0; bool seen=false;};
struct Issues {int malformed=0, invalid=0, duplicates=0, outside=0, offgrid=0;};
struct Compare {int mismatched=0, incomplete=0;};
using Key=pair<string,string>; // symbol, date
using TFKey=pair<Key,int>;
vector<string> split(const string &s){vector<string> out;string v;istringstream in(s);while(getline(in,v,','))out.push_back(v);return out;}
int integer(const string &s){size_t pos=0;int x=stoi(s,&pos);if(pos!=s.size())throw runtime_error("Bad integer: "+s);return x;}
string timeAt(int minute){ostringstream o;o<<setfill('0')<<setw(2)<<minute/60<<':'<<setw(2)<<minute%60;return o.str();}
void require(bool condition,const string &message){if(!condition)throw runtime_error(message);}
ifstream openFile(const fs::path &p){ifstream in(p);if(!in)throw runtime_error("Cannot open "+p.string());return in;}
int main(int argc,char **argv){
 try{
  if(argc<4||argc>6){cerr<<"Usage: "<<argv[0]<<" REPORT_3A_DIR OUTPUT_3B_DIR HORIZON_MINUTES [WARMUP_MINUTES=0] [MIN_CANDIDATES=5]\n";return 2;}
  fs::path src=argv[1],dst=argv[2];
  int horizon=integer(argv[3]),warmup=argc>=5?integer(argv[4]):0,minCandidates=argc>=6?integer(argv[5]):5;
  require(horizon>=1&&horizon<=360,"HORIZON_MINUTES must be 1..360");
  require(warmup>=0&&warmup<=360,"WARMUP_MINUTES must be 0..360");
  require(minCandidates>=1&&minCandidates<=10,"MIN_CANDIDATES must be 1..10");
  const vector<string> stocks={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN"};
  const string benchmark="NIFTY%2050";
  set<string> universe(stocks.begin(),stocks.end());universe.insert(benchmark);
  map<TFKey,Coverage> coverage;
  map<pair<string,int>,Issues> issues;
  map<TFKey,Compare> comparisons;
  map<TFKey,set<int>> missing;
  set<string> dates;
  {
   auto in=openFile(src/"session_integrity.csv");string line;getline(in,line);
   require(line=="symbol,date,timeframe,expected,observed,missing_trading,missing_closing,coverage","Unexpected session_integrity.csv header");
   while(getline(in,line)){if(line.empty())continue;auto f=split(line);require(f.size()==8,"Malformed session integrity row");
    if(!universe.count(f[0]))continue;
    int tf=integer(f[2]);
    require(tf==1||tf==5||tf==15,"Unexpected timeframe");
    TFKey key={{f[0],f[1]},tf};require(!coverage[key].seen,"Duplicate session integrity row");
    auto &c=coverage[key];c.expected=integer(f[3]);c.observed=integer(f[4]);c.trading=integer(f[5]);c.closing=integer(f[6]);c.seen=true;dates.insert(f[1]);
   }
  }
  {
   auto in=openFile(src/"file_issues.csv");string line;getline(in,line);
   require(line=="symbol,timeframe,malformed,invalid,duplicates,out_of_range,off_grid","Unexpected file_issues.csv header");
   while(getline(in,line)){if(line.empty())continue;auto f=split(line);require(f.size()==7,"Malformed file issues row");if(!universe.count(f[0]))continue;
    issues[{f[0],integer(f[1])}]={integer(f[2]),integer(f[3]),integer(f[4]),integer(f[5]),integer(f[6])};
   }
  }
  {
   auto in=openFile(src/"timeframe_comparisons.csv");string line;getline(in,line);
   require(line=="symbol,date,timeframe,complete_buckets,matched,mismatched,incomplete_buckets","Unexpected timeframe comparisons header");
   while(getline(in,line)){if(line.empty())continue;auto f=split(line);require(f.size()==7,"Malformed timeframe comparison row");if(!universe.count(f[0]))continue;
    comparisons[{{f[0],f[1]},integer(f[2])}]={integer(f[5]),integer(f[6])};
   }
  }
  {
   auto in=openFile(src/"missing_candles.csv");string line;getline(in,line);
   require(line=="symbol,date,timeframe,time,classification","Unexpected missing candles header");
   while(getline(in,line)){if(line.empty())continue;auto f=split(line);require(f.size()==5,"Malformed missing candle row");if(!universe.count(f[0]))continue;
    require(f[3].size()==5&&f[3][2]==':',"Bad missing candle timestamp");int t=integer(f[3].substr(0,2))*60+integer(f[3].substr(3,2));
    missing[{{f[0],f[1]},integer(f[2])}].insert(t);
   }
  }
  require(!dates.empty(),"No session dates in 3A reports");
  // Fail closed when required reports are incomplete or malformed.
  for(const auto &date:dates)for(const auto &sym:universe){
   for(int tf:{1,5,15})require(coverage[{{sym,date},tf}].seen,"Missing 3A coverage: "+sym+" "+date);
   for(int tf:{5,15})require(comparisons.count({{sym,date},tf})>0,"Missing 3A timeframe comparison");
  }
  for(const auto &sym:universe)for(int tf:{1,5,15})require(issues.count({sym,tf})>0,"Missing 3A file issues");
  fs::create_directories(dst);
  ofstream daily(dst/"daily_eligibility.csv"),minutes(dst/"decision_eligibility.csv"),summary(dst/"universe_readiness.csv");
  require(bool(daily)&&bool(minutes)&&bool(summary),"Cannot write output files");
  daily<<"symbol,date,trading_complete,closing_complete,full_session_complete,structural_issues,timeframe_mismatches,decision_data_eligible,full_session_eligible,reason\n";
  minutes<<"date,time,symbol,one_minute_present,completed_5m_available,completed_15m_available,benchmark_available,warmup_met,decision_eligible,forward_label_eligible,reason\n";
  summary<<"date,decision_minutes_checked,all_10_eligible_minutes,at_least_k_eligible_minutes,zero_eligible_minutes,full_session_eligible_stocks,benchmark_full_session_eligible,minimum_candidates\n";
  long long totalEligible=0,totalLabel=0,totalBlocked=0;
  for(const auto &date:dates){
   map<string,bool> dayDecision,dayFull;
   for(const auto &sym:universe){
    bool trade=true,full=true,structural=false,mismatch=false;
    for(int tf:{1,5,15}){
     auto c=coverage.at({{sym,date},tf});if(c.trading>0)trade=false;if(c.closing>0)full=false;
     auto i=issues.at({sym,tf});if(i.malformed||i.invalid||i.duplicates||i.offgrid)structural=true;
    }
    for(int tf:{5,15})if(comparisons.at({{sym,date},tf}).mismatched) mismatch=true;
    dayDecision[sym]=trade&&!structural&&!mismatch;
    dayFull[sym]=trade&&full&&!structural&&!mismatch;
    string reason=structural?"STRUCTURAL_ISSUES":mismatch?"OHLCV_MISMATCH":!trade?"TRADING_GAPS":!full?"CLOSING_GAPS":"OK";
    daily<<sym<<','<<date<<','<<trade<<','<<full<<','<<(trade&&full)<<','<<structural<<','<<mismatch<<','<<dayDecision[sym]<<','<<dayFull[sym]<<','<<reason<<'\n';
   }
   int all=0,atLeast=0,zero=0;
   for(int t=555;t<915;t++){
    // At t, the minute candle labeled t is completed only at t+1.
    // Completed 5m/15m bars are those whose final 1m candle is at or before t.
    auto present=[&](const string &sym,int tf,int timestamp){return missing[{{sym,date},tf}].count(timestamp)==0;};
    auto eligibleAt=[&](const string &sym){
     bool one=present(sym,1,t);
     int b5=555+((t-555+1)/5)*5-5;
     int b15=555+((t-555+1)/15)*15-15;
     bool five=b5>=555&&present(sym,5,b5);
     bool fifteen=b15>=555&&present(sym,15,b15);
     bool warm=t-555>=warmup;
     return one&&five&&fifteen&&warm&&dayDecision[sym];
    };
    bool bench=eligibleAt(benchmark);
    int candidates=0;
    for(const auto &sym:stocks)if(bench&&eligibleAt(sym))candidates++;
    if(candidates==10)all++;
    if(candidates>=minCandidates)atLeast++;
    if(candidates==0)zero++;
    for(const auto &sym:stocks){
     bool one=present(sym,1,t);
     int b5=555+((t-555+1)/5)*5-5,b15=555+((t-555+1)/15)*15-15;
     bool five=b5>=555&&present(sym,5,b5),fifteen=b15>=555&&present(sym,15,b15);
     bool warm=t-555>=warmup;
     bool eligible=bench&&one&&five&&fifteen&&warm&&dayDecision[sym]&&candidates>=minCandidates;
     // Labels are strictly within same day, ending no later than 15:14.
     // Missing intermediate 1m candles invalidate the horizon, not only endpoint.
     bool label=eligible&&t+horizon<915;
     if(label)for(int k=1;k<=horizon;k++)if(!present(sym,1,t+k)){label=false;break;}
     if(eligible)totalEligible++;else totalBlocked++;
     if(label)totalLabel++;
     string reason=!dayDecision[sym]?"DAY_DATA_INVALID":!bench?"BENCHMARK_UNAVAILABLE":!one?"MISSING_1M":!five?"NO_COMPLETED_5M":!fifteen?"NO_COMPLETED_15M":!warm?"WARMUP":candidates<minCandidates?"INSUFFICIENT_CANDIDATES":"OK";
     minutes<<date<<','<<timeAt(t)<<','<<sym<<','<<one<<','<<five<<','<<fifteen<<','<<bench<<','<<warm<<','<<eligible<<','<<label<<','<<reason<<'\n';
    }
   }
   int fullStocks=0;for(const auto &sym:stocks)fullStocks+=dayFull[sym];
   summary<<date<<",360,"<<all<<','<<atLeast<<','<<zero<<','<<fullStocks<<','<<dayFull[benchmark]<<','<<minCandidates<<'\n';
  }
  cout<<"CETE Phase 3.19.3B (read-only)\nSessions: "<<dates.size()<<"\nEligible stock-minute decisions: "<<totalEligible<<"\nBlocked stock-minute decisions: "<<totalBlocked<<"\nEligible within-session forward labels: "<<totalLabel<<"\nReports: "<<dst<<"\n";
  cout<<"IMPORTANT: No training labels computed. Eligibility is conservative; 5m/15m bars require current-session completion.\n";
  cout<<"WARNING: Phase 3A date coverage relies on NIFTY-observed sessions; this tool does not verify external exchange calendars.\n";
  return 0;
 }catch(const exception &e){cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
