#include <algorithm>
#include <cmath>
#include <cstdint>
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
namespace fs=std::filesystem;
namespace f4 {
struct Quote {std::string sym; long long t=0; int session=0; double close=0,z=0,oldp=0,newp=0;};
struct Fees {double brokerage=0,stt=0,exchange=0,sebi=0,gst=0,stamp=0,total=0;};
Fees fees(double buy,double sell,int qty){if(buy<=0||sell<=0||qty<=0)throw std::runtime_error("invalid fees input");double b=buy*qty,s=sell*qty,t=b+s;Fees c;c.brokerage=std::min(20.,b*.0003)+std::min(20.,s*.0003);c.stt=std::floor(s*.00025+.5);c.exchange=t*.0000307;c.sebi=t*.000001;c.stamp=b*.00003;c.gst=.18*(c.brokerage+c.exchange+c.sebi);c.total=c.brokerage+c.stt+c.exchange+c.sebi+c.stamp+c.gst;return c;}
std::vector<std::string> split(const std::string&s){std::vector<std::string> v;std::stringstream ss(s);std::string x;while(std::getline(ss,x,','))v.push_back(x);return v;}
std::map<std::string,int> header(const std::string&s){auto a=split(s);std::map<std::string,int> m;for(int i=0;i<(int)a.size();++i)m[a[i]]=i;return m;}
std::string cell(const std::vector<std::string>&v,const std::map<std::string,int>&m,const std::string&key){auto it=m.find(key);if(it==m.end()||it->second>=(int)v.size())throw std::runtime_error("missing CSV field "+key);return v[it->second];}
int local_minutes(long long epoch){long long minute=(epoch+19800)/60;int d=(int)(minute%1440);return d<0?d+1440:d;}
struct Position {std::string sym;long long entered=0;double buy=0;int qty=0;};
struct Result {int entries=0,exits=0,wins=0,forced=0,missing_exit=0;double gross=0,cost=0,net=0,equity=0,peak=0,maxdd=0;};
struct Policy {std::string name;int mode=0;bool gate=false;};
}
int main(int argc,char**argv){using namespace f4;try{
 if(argc!=6){std::cerr<<"Usage: "<<argv[0]<<" <heldout_predictions_f3ef2c3.csv> <f2c2_dataset_dir> <shares.txt> <output_dir> <initial_capital_INR>\n";return 2;}
 fs::path pred=argv[1],dir=argv[2],out=argv[4];double capital=std::stod(argv[5]);if(!(capital>0))throw std::runtime_error("capital must be positive");fs::create_directories(out);
 std::ifstream sf(argv[3]);if(!sf)throw std::runtime_error("shares file missing");std::set<std::string> symbols;std::string line;while(std::getline(sf,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#'||line.find("NIFTY")!=std::string::npos)continue;symbols.insert(line);}if(symbols.empty())throw std::runtime_error("no stock symbols");
 // Build only the test-period z-score index. The 10m target is NEVER used for decisions.
 std::map<std::pair<long long,std::string>,double> zindex;
 for(const auto&sym:symbols){std::ifstream in(dir/(sym+"_training_f3ef2c2.csv"));if(!in)throw std::runtime_error("missing dataset for "+sym);std::getline(in,line);auto h=header(line);int count=0;while(std::getline(in,line)){if(line.empty())continue;auto v=split(line);int session=std::stoi(cell(v,h,"session_id"));if(session<20260622)continue;auto t=std::stoll(cell(v,h,"decision_epoch"));double z=std::stod(cell(v,h,"z20"));if(!std::isfinite(z))continue;zindex[{t,sym}]=z;++count;}std::cerr<<"INDEX "<<sym<<" rows="<<count<<'\n';}
 std::map<long long,std::vector<Quote>> ticks;std::ifstream in(pred);if(!in)throw std::runtime_error("missing heldout predictions");std::getline(in,line);auto h=header(line);long long n=0;while(std::getline(in,line)){if(line.empty())continue;auto v=split(line);if(std::stoi(cell(v,h,"horizon_minutes"))!=10)continue;Quote q;q.sym=cell(v,h,"symbol");if(!symbols.count(q.sym))continue;q.t=std::stoll(cell(v,h,"decision_epoch"));q.session=std::stoi(cell(v,h,"session_id"));q.close=std::stod(cell(v,h,"close"));q.oldp=std::stod(cell(v,h,"f2c_predicted_return"));q.newp=std::stod(cell(v,h,"f2c3_predicted_return"));auto it=zindex.find({q.t,q.sym});if(it==zindex.end())continue;q.z=it->second;if(q.session<20260622||!std::isfinite(q.close)||q.close<=0||!std::isfinite(q.oldp)||!std::isfinite(q.newp))continue;ticks[q.t].push_back(q);++n;}
 if(ticks.empty())throw std::runtime_error("no aligned heldout rows");std::cerr<<"ALIGNED 10m rows="<<n<<" decision_times="<<ticks.size()<<'\n';
 std::vector<Policy> policies={{"STAT_Z20_PROXY",0,false},{"TOP5_F2C_UNGATED",1,false},{"TOP5_F2C3_UNGATED",2,false},{"TOP5_F2C_COST_GATE",1,true},{"TOP5_F2C3_COST_GATE",2,true}};
 std::ofstream trades(out/"trades_f3ef2c4.csv"),summary(out/"portfolio_summary_f3ef2c4.csv"),decisions(out/"decision_audit_f3ef2c4.csv");if(!trades||!summary||!decisions)throw std::runtime_error("output file error");trades<<"policy,symbol,entry_epoch,exit_epoch,entry_price,exit_price,qty,gross_inr,charges_inr,net_inr,reason\n";decisions<<"policy,decision_epoch,top5_symbols,selected_symbol,predicted_10m_return,action,reason\n";summary<<"policy,entries,exits,wins,forced_exits,missing_exit_quotes,gross_inr,charges_inr,net_inr,final_equity_inr,max_drawdown_inr\n";
 trades<<std::setprecision(12);summary<<std::setprecision(12);decisions<<std::setprecision(12);
 for(auto pol:policies){Result r;r.equity=capital;r.peak=capital;Position pos;int prevsession=0;
 auto close_position=[&](const Quote&q,const std::string&reason){if(pos.sym.empty())return;double gross=(q.close-pos.buy)*pos.qty;double charge=fees(pos.buy,q.close,pos.qty).total;double net=gross-charge;r.gross+=gross;r.cost+=charge;r.net+=net;r.equity+=net;r.peak=std::max(r.peak,r.equity);r.maxdd=std::max(r.maxdd,r.peak-r.equity);++r.exits;if(net>0)++r.wins;if(reason=="SESSION_FLAT")++r.forced;trades<<pol.name<<','<<pos.sym<<','<<pos.entered<<','<<q.t<<','<<pos.buy<<','<<q.close<<','<<pos.qty<<','<<gross<<','<<charge<<','<<net<<','<<reason<<'\n';pos={};};
 for(auto it=ticks.begin();it!=ticks.end();++it){long long t=it->first;auto &qs=it->second;if(qs.empty())continue;int session=qs.front().session;int mins=local_minutes(t);if(prevsession&&session!=prevsession&& !pos.sym.empty())throw std::runtime_error("open position crossed session: missing 15:15 quote");prevsession=session;
 // Exit at 10 minutes or mandatory 15:15, using only the quote observed at the current decision time.
 if(!pos.sym.empty()&&(t-pos.entered>=600||mins>=15*60+15)){auto qi=std::find_if(qs.begin(),qs.end(),[&](const Quote&q){return q.sym==pos.sym;});if(qi!=qs.end())close_position(*qi,mins>=15*60+15?"SESSION_FLAT":"TEN_MINUTES");else ++r.missing_exit;}
 if(!pos.sym.empty()||mins<9*60+15||mins>=14*60+45||r.equity<=0)continue;
 // Statistical top five: descending z20, a documented proxy only, NOT actual DevManager ranking.
 std::vector<Quote> top=qs;std::sort(top.begin(),top.end(),[](const Quote&a,const Quote&b){if(a.z!=b.z)return a.z>b.z;return a.sym<b.sym;});if(top.size()>5)top.resize(5);
 if(top.empty())continue;Quote chosen=top.front();if(pol.mode!=0){for(auto &q:top){double a=pol.mode==1?q.oldp:q.newp;double b=pol.mode==1?chosen.oldp:chosen.newp;if(a>b)chosen=q;}}
 std::string names;for(const auto&q:top){if(!names.empty())names+='|';names+=q.sym;}
 int qty=(int)std::floor((r.equity*.75)/chosen.close);double forecast=pol.mode==1?chosen.oldp:(pol.mode==2?chosen.newp:0.);std::string action="WAIT",reason="";
 if(qty<=0)reason="INSUFFICIENT_CAPITAL";
 else if(pol.gate&&(forecast<=0||chosen.close*(1+forecast)<=0||forecast*chosen.close*qty<=fees(chosen.close,chosen.close*(1+forecast),qty).total))reason="BELOW_COST_GATE";
 else {pos={chosen.sym,t,chosen.close,qty};++r.entries;action="OPEN";reason="SELECTED";}
 decisions<<pol.name<<','<<t<<','<<names<<','<<chosen.sym<<','<<forecast<<','<<action<<','<<reason<<'\n';
 }
 if(!pos.sym.empty())throw std::runtime_error("unclosed final position; missing exit quotes");summary<<pol.name<<','<<r.entries<<','<<r.exits<<','<<r.wins<<','<<r.forced<<','<<r.missing_exit<<','<<r.gross<<','<<r.cost<<','<<r.net<<','<<r.equity<<','<<r.maxdd<<'\n';std::cout<<pol.name<<" trades="<<r.exits<<" net_INR="<<r.net<<" final_equity="<<r.equity<<" missing_exit="<<r.missing_exit<<'\n';
 }
 std::cout<<"F3E-F2C.4 COMPLETE output="<<out<<" (PROXY POLICIES, NOT RL BACKTEST)\n";return 0;
 }catch(const std::exception&e){std::cerr<<"F3E-F2C.4 ERROR: "<<e.what()<<'\n';return 1;}}
