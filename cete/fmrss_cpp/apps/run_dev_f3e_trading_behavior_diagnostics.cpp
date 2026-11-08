// D9F.4C-F3E: read-only diagnostics of frozen F3C replay CSV.
// This is NOT a counterfactual strategy simulator and does not inspect Q-values.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;
namespace {
struct Row { std::int64_t t{}, next{}; int action{}; std::string desired, held; double before{}, after{}, costs{}; bool terminal{}; };
void check(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
std::vector<std::string> split(const std::string& s) { std::vector<std::string> v; size_t p=0; while(true){auto n=s.find(',',p);if(n==std::string::npos){v.push_back(s.substr(p));break;}v.push_back(s.substr(p,n-p));p=n+1;}return v; }
double real(const std::string& s){size_t p=0;auto v=std::stod(s,&p);check(p==s.size()&&std::isfinite(v),"invalid floating point");return v;}
std::int64_t integer(const std::string& s){size_t p=0;auto v=std::stoll(s,&p);check(p==s.size(),"invalid integer");return v;}
bool near(double a,double b){return std::abs(a-b)<=1e-8*std::max({1.,std::abs(a),std::abs(b)});}
int minuteIST(std::int64_t t){auto s=((t+19800)%86400+86400)%86400;return static_cast<int>(s/60);}
std::string type(const std::string& previous,const std::string& current){if(previous.empty())return current.empty()?"WAIT":"OPEN";if(current.empty())return "EXIT";return previous==current?"HOLD":"SWITCH";}
struct Stats {size_t count{};double cost{},gross{},net{};};
size_t bucket(const std::string& t){if(t=="WAIT")return 0;if(t=="OPEN")return 1;if(t=="HOLD")return 2;if(t=="SWITCH")return 3;return 4;}
}
int main(int argc,char** argv){
 try {
  check(argc==3,"Usage: run_dev_f3e_trading_behavior_diagnostics <F3C.csv> <diagnostics.csv>");
  std::ifstream in(argv[1]);check(static_cast<bool>(in),"cannot open F3C CSV");
  std::string line;check(static_cast<bool>(std::getline(in,line)),"missing CSV header");
  check(line=="decision_time,next_decision_time,manager_action,desired_symbol,held_after,equity_before,equity_after_mark,gross_reward,net_reward,selected_reward,execution_costs,terminal","unexpected F3C CSV schema");
  std::vector<Row> rows;
  while(std::getline(in,line)){
   if(line.empty())continue;
   auto f=split(line);check(f.size()==12,"invalid column count at row "+std::to_string(rows.size()+2));
   Row r; r.t=integer(f[0]);r.next=integer(f[1]);r.action=static_cast<int>(integer(f[2]));r.desired=f[3];r.held=f[4];r.before=real(f[5]);r.after=real(f[6]);r.costs=real(f[10]);r.terminal=integer(f[11])==1;
   check(r.action>=0&&r.action<=6&&r.next-r.t==60&&r.before>0&&r.after>0&&r.costs>=0,"invalid action, interval, equity or cost");
   check(integer(f[11])==0||integer(f[11])==1,"invalid terminal flag");
   (void)real(f[9]);
   if(!rows.empty()) {check(rows.back().next==r.t,"nonconsecutive decision times");check(near(rows.back().after,r.before),"equity continuity failure");check(!rows.back().terminal,"rows after terminal");}
   rows.push_back(std::move(r));
  }
  check(rows.size()==360,"expected 360 decision rows");
  check(minuteIST(rows.front().t)==556&&minuteIST(rows.back().t)==915&&rows.back().terminal&&rows.back().held.empty(),"session/terminal mismatch");
  fs::path output=argv[2];if(output.has_parent_path())fs::create_directories(output.parent_path());
  std::ofstream out(output);check(static_cast<bool>(out),"cannot write diagnostics CSV");
  out<<"decision_time,minute_ist,transition,manager_action,held_before,held_after,holding_age_before_minutes,step_gross_pnl,step_costs,step_net_pnl,cumulative_costs,cumulative_net_pnl\n";
  out<<std::setprecision(17);
  std::array<Stats,5> stats{};std::string previous;std::int64_t entry=0;size_t shortHolds=0,afterCutoffSwitches=0,afterCutoffOpens=0;double cumulativeCost=0,cumulativeNet=0;std::vector<int> completedDurations;
  for(const auto& r:rows){
   const std::string transition=type(previous,r.held);
   const auto k=bucket(transition);
   const double net=r.after-r.before, gross=net+r.costs;
   auto& st=stats[k];++st.count;st.cost+=r.costs;st.net+=net;st.gross+=gross;
   int age=previous.empty()?0:static_cast<int>((r.t-entry)/60);
   if(transition=="EXIT"||transition=="SWITCH") {completedDurations.push_back(age);if(age<=1)++shortHolds;}
   if(transition=="OPEN"||transition=="SWITCH")entry=r.t;
   if(minuteIST(r.t)>=885){if(transition=="SWITCH")++afterCutoffSwitches;if(transition=="OPEN")++afterCutoffOpens;}
   cumulativeCost+=r.costs;cumulativeNet+=net;
   out<<r.t<<','<<minuteIST(r.t)<<','<<transition<<','<<r.action<<','<<previous<<','<<r.held<<','<<age<<','<<gross<<','<<r.costs<<','<<net<<','<<cumulativeCost<<','<<cumulativeNet<<'\n';
   previous=r.held;
  }
  check(near(cumulativeNet,rows.back().after-rows.front().before),"P&L reconciliation failed");
  check(afterCutoffOpens==0&&afterCutoffSwitches==0,"entry/switch after 14:45");
  const std::array<std::string,5> names={"WAIT","OPEN","HOLD","SWITCH","EXIT"};
  std::cout<<std::fixed<<std::setprecision(4)<<"D9F.4C-F3E — FROZEN BASELINE BEHAVIOR DIAGNOSTICS\n";
  std::cout<<"Decisions: "<<rows.size()<<" | Initial equity: "<<rows.front().before<<" | Final equity: "<<rows.back().after<<'\n';
  for(size_t k=0;k<stats.size();++k)std::cout<<std::setw(7)<<names[k]<<" count="<<stats[k].count<<" gross="<<stats[k].gross<<" costs="<<stats[k].cost<<" net="<<stats[k].net<<'\n';
  std::cout<<"Total costs: "<<cumulativeCost<<" | Net P&L: "<<cumulativeNet<<'\n';
  std::cout<<"Completed holding intervals: "<<completedDurations.size()<<" | <=1 minute: "<<shortHolds<<'\n';
  if(!completedDurations.empty()){std::sort(completedDurations.begin(),completedDurations.end());double sum=0;for(int n:completedDurations)sum+=n;std::cout<<"Holding duration (minutes): mean="<<sum/completedDurations.size()<<" median="<<completedDurations[completedDurations.size()/2]<<" max="<<completedDurations.back()<<'\n';}
  std::cout<<"Post-14:45 opens/switches: "<<afterCutoffOpens<<'/'<<afterCutoffSwitches<<'\n';
  std::cout<<"Output: "<<output<<"\nF3E BASELINE DIAGNOSTICS: PASS\n";
  std::cout<<"LIMIT: CSV lacks manager Q-values, per-leg costs, quantities and counterfactual HOLD prices. No policy improvement or cost-aware switching benefit is established.\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"F3E BASELINE DIAGNOSTICS: FAILED: "<<e.what()<<'\n';return 1;}
}
