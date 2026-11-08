// D9F.4C-F3E-B: read-only seven-action Q-value diagnostics.
// Consumes the companion CSV emitted by DevCandleManagerReplay::writeCsv().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;
namespace {
void ensure(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
std::vector<std::string> split(const std::string& s){std::vector<std::string> v;std::string x;std::istringstream in(s);while(std::getline(in,x,','))v.push_back(x);if(!s.empty()&&s.back()==',')v.emplace_back();return v;}
std::int64_t integer(const std::string& s){std::size_t n=0;auto v=std::stoll(s,&n);ensure(n==s.size(),"bad integer: "+s);return v;}
double real(const std::string& s){std::size_t n=0;double v=std::stod(s,&n);ensure(n==s.size()&&std::isfinite(v),"bad Q/score: "+s);return v;}
std::string safe(const std::string& s){ensure(s.find_first_of(",\r\n\"")==std::string::npos,"invalid CSV text");return s;}
struct Record {
 std::int64_t time{};int raw{},final{};std::string before,desired,after,priority,recommendation;
 int age{};bool stagnant{},stop{};int legs{};
 std::array<bool,7> legal{};std::array<double,7> q{};
 std::array<std::string,5> symbol{};std::array<double,5> score{};
};
Record parse(const std::vector<std::string>& f){
 ensure(f.size()==36,"expected 36 audit columns; found "+std::to_string(f.size()));
 Record r;r.time=integer(f[0]);r.raw=integer(f[1]);r.final=integer(f[2]);
 r.before=f[3];r.desired=f[4];r.after=f[5];r.priority=f[6];r.recommendation=f[7];
 r.age=integer(f[8]);r.stagnant=integer(f[9])!=0;r.stop=integer(f[10])!=0;r.legs=integer(f[11]);
 for(int a=0;a<7;++a){auto v=integer(f[12+a]);ensure(v==0||v==1,"invalid legal mask");r.legal[a]=v==1;}
 for(int a=0;a<7;++a)r.q[a]=real(f[19+a]);
 for(int i=0;i<5;++i){r.symbol[i]=f[26+2*i];r.score[i]=real(f[27+2*i]);}
 ensure(r.raw>=0&&r.raw<7&&r.final>=0&&r.final<7,"action out of range");
 ensure(r.legal[r.final],"final action is masked");
 ensure(r.legs>=0&&r.legs<=2,"invalid executed legs");
 return r;
}
}
int main(int argc,char**argv){try{
 ensure(argc==3,"Usage: run_dev_f3eb_manager_q_diagnostics <F3C_decision_audit.csv> <F3E_output.csv>");
 std::ifstream in(argv[1]);ensure(bool(in),"cannot open input audit CSV");
 std::string line;ensure(bool(std::getline(in,line)),"empty input");
 auto header=split(line);ensure(header.size()==36,"unexpected audit schema: expected 36 columns");
 const std::array<std::string,12> required={"decision_time","raw_action","final_action","held_before","desired","held_after","supervisor_priority","supervisor_recommendation","holding_age_minutes","stagnant","stop_triggered","executed_legs"};
 for(int i=0;i<12;++i)ensure(header[i]==required[i],"audit header mismatch at column "+std::to_string(i));
 for(int a=0;a<7;++a){ensure(header[12+a]=="legal_"+std::to_string(a),"legal header mismatch");ensure(header[19+a]=="q_"+std::to_string(a),"Q header mismatch");}
 for(int i=0;i<5;++i){ensure(header[26+2*i]=="slot_"+std::to_string(i)+"_symbol","slot header mismatch");ensure(header[27+2*i]=="slot_"+std::to_string(i)+"_score","score header mismatch");}
 fs::path outpath(argv[2]);if(!outpath.parent_path().empty())fs::create_directories(outpath.parent_path());
 std::ofstream out(outpath);ensure(bool(out),"cannot open output");
 out<<"decision_time,held_before,desired,held_after,raw_action,final_action,transition,override,keep_action,keep_q,best_switch_action,best_switch_symbol,best_switch_q,switch_minus_keep_q,legal_switch_count,executed_legs,supervisor_recommendation\n";
 out<<std::setprecision(17);
 std::size_t n=0,flat=0,invested=0,switched=0,switchedWithKeep=0,switchedQGreater=0,switchedQNotGreater=0,overrides=0,heldSlotSelected=0,maskedBest=0,noSwitchOptions=0;
 double marginSum=0;std::size_t marginN=0;
 std::int64_t last=0;
 while(std::getline(in,line)){
  if (line.empty()) continue;
  auto r = parse(split(line));
  ensure(n==0||r.time==last+60,"nonconsecutive timestamps");last=r.time;++n;
  bool isInvested=!r.before.empty();if(isInvested)++invested;else ++flat;
  if(r.raw!=r.final)++overrides;
  // Action 1 is KEEP. Selecting the held stock in a Top-5 slot is also KEEP.
  int keep=-1;double keepQ=-std::numeric_limits<double>::infinity();
  if(isInvested){for(int a=1;a<7;++a){bool same=(a==1)||(a>=2&&r.symbol[a-2]==r.before);if(same&&r.legal[a]&&r.q[a]>keepQ){keep=a;keepQ=r.q[a];}}ensure(keep>=0,"invested without legal KEEP");}
  int bestSwitch=-1;double switchQ=-std::numeric_limits<double>::infinity();std::string switchSymbol;int switchOptions=0;
  for(int a=2;a<7;++a){int slot=a-2;if(r.legal[a]&&!r.symbol[slot].empty()&&r.symbol[slot]!=r.before){++switchOptions;if(r.q[a]>switchQ){bestSwitch=a;switchQ=r.q[a];switchSymbol=r.symbol[slot];}}}
  if(isInvested&&switchOptions==0)++noSwitchOptions;
  if(isInvested&&r.final>=2&&r.symbol[r.final-2]==r.before)++heldSlotSelected;
  bool isSwitch=isInvested&&!r.after.empty()&&r.after!=r.before;
  if(isSwitch){++switched;ensure(r.final>=2&&r.desired==r.after,"SWITCH not aligned with final candidate action");if(keep>=0){++switchedWithKeep;if(r.q[r.final]>keepQ+1e-12)++switchedQGreater;else ++switchedQNotGreater;}}
  if(isInvested&&bestSwitch>=0){marginSum+=switchQ-keepQ;++marginN;}
  const char* transition=r.before.empty()?(r.after.empty()?"WAIT":"OPEN"):(r.after.empty()?"EXIT":(r.after==r.before?"HOLD":"SWITCH"));
  // Raw Q argmax may be overridden by session safety; do not interpret that as policy preference.
  int rawBest=-1;double rawBestQ=-std::numeric_limits<double>::infinity();for(int a=0;a<7;++a)if(r.legal[a]&&r.q[a]>rawBestQ){rawBest=a;rawBestQ=r.q[a];}
  if(r.raw!=rawBest)++maskedBest;
  out<<r.time<<','<<safe(r.before)<<','<<safe(r.desired)<<','<<safe(r.after)<<','<<r.raw<<','<<r.final<<','<<transition<<','<<(r.raw!=r.final?1:0)<<','<<keep<<',';
  if (keep>=0) out<<keepQ;
  out<<','<<bestSwitch<<','<<safe(switchSymbol)<<',';
  if (bestSwitch>=0) out<<switchQ;
  out<<',';
  if (keep>=0&&bestSwitch>=0) out<<(switchQ-keepQ);
  out<<','<<switchOptions<<','<<r.legs<<','<<safe(r.recommendation)<<'\n';
 }
 ensure(bool(out),"failed writing output");ensure(n>0,"no audit rows");
 std::cout<<"D9F.4C-F3E-B — MANAGER Q-VALUE DIAGNOSTICS\n"
          <<"Decisions: "<<n<<" | Invested at decision: "<<invested<<" | Flat: "<<flat<<'\n'
          <<"Actual switches: "<<switched<<" | Switches with legal KEEP: "<<switchedWithKeep<<'\n'
          <<"Switch chosen Q > best KEEP Q: "<<switchedQGreater<<" | Not greater: "<<switchedQNotGreater<<'\n'
          <<"Raw/final action overrides: "<<overrides<<" | Held-symbol Top-5 action selected: "<<heldSlotSelected<<'\n'
          <<"Invested minutes without legal SWITCH: "<<noSwitchOptions<<'\n'
          <<"Raw action differs from computed legal argmax (ties possible): "<<maskedBest<<'\n'
          <<"Mean best-SWITCH minus best-KEEP Q: ";if(marginN)std::cout<<std::setprecision(8)<<marginSum/marginN;else std::cout<<"N/A";
 std::cout<<" ("<<marginN<<" comparable decisions)\nOutput: "<<outpath<<"\nF3E-B Q DIAGNOSTICS: PASS\n"
          <<"NOTE: Q margins are model scores, NOT expected rupee profit. No counterfactual performance is inferred.\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"F3E-B Q DIAGNOSTICS: FAILED: "<<e.what()<<'\n';return 1;}}
