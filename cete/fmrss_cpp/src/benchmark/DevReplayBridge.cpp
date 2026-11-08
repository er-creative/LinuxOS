#include "devai/benchmark/DevReplayBridge.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
namespace devai::benchmark {
namespace {
void require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
std::vector<std::string> fields(const std::string& s){std::stringstream ss(s);std::vector<std::string> v;std::string t;while(std::getline(ss,t,','))v.push_back(t);return v;}
struct Account {double equity{7000},peak{7000},drawdown{};unsigned trades{};std::string last,position,entry_time;double entry{},qty{};};
}
std::vector<ReplayEvent> DevReplayBridge::readCsv(const std::filesystem::path& path){
 std::ifstream in(path);require(bool(in),"cannot open CSV");std::string line;
 require(bool(std::getline(in,line)),"empty CSV");
 require(line=="candidate,manager,symbol,date,time,side,action,price,legal,completed,past_only","unexpected CSV header");
 std::vector<ReplayEvent> rows;
 while(std::getline(in,line)){if(line.empty())continue;auto f=fields(line);require(f.size()==11,"CSV requires 11 fields");
  require((f[8]=="0"||f[8]=="1")&&(f[9]=="0"||f[9]=="1")&&(f[10]=="0"||f[10]=="1"),"invalid flag");
  std::size_t used=0;double price=std::stod(f[7],&used);require(used==f[7].size(),"invalid price");
  rows.push_back({f[0],f[1],f[2],f[3],f[4],f[5],f[6],price,f[8]=="1",f[9]=="1",f[10]=="1"});
 }require(!rows.empty(),"no events");return rows;
}
ReplayOutput DevReplayBridge::evaluate(const std::vector<ReplayEvent>& rows){
 require(!rows.empty(),"no events");std::map<std::pair<std::string,std::string>,Account> accounts;ReplayOutput out;
 for(const auto& e:rows){
  require(!e.candidate.empty()&&!e.symbol.empty(),"empty candidate/symbol");
  require(e.date.size()==10&&e.time.size()==19&&e.time.substr(0,10)==e.date&&e.time[10]==' ',"invalid date/time");
  auto clock=e.time.substr(11);require(clock>="09:15:00"&&clock<="15:30:00","outside market hours");
  require(e.legal&&e.completed&&e.past_only,"unsafe or look-ahead event");
  require(std::isfinite(e.price)&&e.price>0,"invalid price");
  require((e.manager=="DevManager_001"&&e.side=="LONG")||(e.manager=="DevManager_002"&&e.side=="SHORT"),"manager/side mismatch");
  auto& a=accounts[{e.candidate,e.manager}];require(a.last.empty()||e.time>a.last,"duplicate or out-of-order event");
  require(a.position.empty()||a.entry_time.substr(0,10)==e.date,"overnight position");a.last=e.time;
  if(e.action=="OPEN"){
   require(clock<"15:15:00"&&a.position.empty(),"illegal OPEN");
   a.qty=std::floor(a.equity*0.75/e.price);require(a.qty>=1,"insufficient funds");a.entry=e.price;a.entry_time=e.time;a.position=e.symbol;
  }else if(e.action=="HOLD"){
   require(a.position==e.symbol&&clock<"15:15:00","illegal HOLD");
  }else if(e.action=="EXIT"){
   require(a.position==e.symbol&&clock<="15:15:00","illegal EXIT");
   double gross=(e.side=="LONG"?1.0:-1.0)*(e.price-a.entry)*a.qty;
   double net=gross-40.0;a.equity+=net;require(std::isfinite(a.equity),"invalid equity");
   a.peak=std::max(a.peak,a.equity);a.drawdown=std::max(a.drawdown,100.0*(a.peak-a.equity)/a.peak);
   ++a.trades;out.trades.push_back({e.candidate,e.manager,e.symbol,e.side,a.entry_time,e.time,a.qty,gross,40.0,net});a.position.clear();
  }else if(e.action=="WAIT"){
   require(a.position.empty()||clock<"15:15:00","WAIT after flat deadline");
  }else throw std::runtime_error("unknown action");
 }
 for(const auto& [key,a]:accounts){require(a.position.empty(),"unclosed position");out.reports.push_back({key.first,key.second,a.equity,a.equity-7000,a.drawdown,a.trades});}
 return out;
}
void DevReplayBridge::writeCsv(const std::filesystem::path& path,const ReplayOutput& out){
 std::ofstream f(path);require(bool(f),"cannot write report");f<<std::setprecision(15);
 f<<"candidate,manager,symbol,side,entry_time,exit_time,quantity,gross,fees,net\n";
 for(const auto& t:out.trades)f<<t.candidate<<','<<t.manager<<','<<t.symbol<<','<<t.side<<','<<t.entry_time<<','<<t.exit_time<<','<<t.quantity<<','<<t.gross<<','<<t.fees<<','<<t.net<<'\n';
 require(bool(f),"write failed");
}
}
