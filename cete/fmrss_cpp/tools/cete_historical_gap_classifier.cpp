// CETE 3.19.3A: read-only gap classification, C++17
#include <algorithm>
#include <cmath>
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
namespace fs=std::filesystem;
struct Bar {double o,h,l,c,v;};
using Days=map<string,map<int,Bar>>;
struct Data {Days days;int malformed=0,invalid=0,duplicates=0,outside=0,offgrid=0;};
vector<string> fields(string s){vector<string> v;istringstream in(s);string x;while(getline(in,x,','))v.push_back(x);return v;}
int minutes(string s){if(s.size()!=5||s[2]!=':')return -1;try{int h=stoi(s.substr(0,2)),m=stoi(s.substr(3));return h<24&&m<60?h*60+m:-1;}catch(...){return -1;}}
string clocktime(int t){ostringstream o;o<<setfill('0')<<setw(2)<<t/60<<":"<<setw(2)<<t%60;return o.str();}
bool close(double a,double b){return abs(a-b)<=max(1e-5,1e-5*max(abs(a),abs(b)));}
Data readData(fs::path p,string sym,string first,string last,int tf){
 ifstream in(p);if(!in)throw runtime_error("Cannot read "+p.string());
 Data d;string line;
 while(getline(in,line)){
  if(line.empty())continue;
  auto f=fields(line);if(f.size()!=8||f[0]!=sym||f[1].size()!=10){d.malformed++;continue;}
  if(f[1]<first||f[1]>last){d.outside++;continue;}
  int t=minutes(f[2]);if(t<555||t>=930||(t-555)%tf){d.offgrid++;continue;}
  try{
   Bar b{stod(f[3]),stod(f[4]),stod(f[5]),stod(f[6]),stod(f[7])};
   if(!isfinite(b.o)||!isfinite(b.h)||!isfinite(b.l)||!isfinite(b.c)||!isfinite(b.v)||b.o<=0||b.h<=0||b.l<=0||b.c<=0||b.v<0||b.h<max(b.o,b.c)||b.l>min(b.o,b.c)){d.invalid++;continue;}
   if(!d.days[f[1]].emplace(t,b).second)d.duplicates++;
  }catch(...){d.malformed++;}
 }
 return d;
}
int main(int argc,char**argv){
 try{
 if(argc!=5){cerr<<"Usage: "<<argv[0]<<" DATA_ROOT START_DATE END_DATE OUTPUT_DIR\n";return 2;}
 fs::path root=argv[1],out=argv[4];string first=argv[2],last=argv[3];
 if(first.size()!=10||last.size()!=10||first>last)throw runtime_error("Invalid dates");
 const vector<string> symbols={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN","NIFTY%2050"};
 map<string,map<int,Data>> db;
 for(auto&s:symbols)for(int tf:{1,5,15}){
  string folder=tf==1?"minute":tf==5?"5minutes":"15minutes";
  string suffix=tf==1?"_1min.txt":tf==5?"_5mins.txt":"_15mins.txt";
  db[s][tf]=readData(root/folder/(s+suffix),s,first,last,tf);
 }
 set<string> dates;
 for(auto &[date,bars]:db.at("NIFTY%2050").at(1).days)if(!bars.empty())dates.insert(date);
 if(dates.empty())throw runtime_error("No benchmark dates");
 fs::create_directories(out);
 ofstream sessions(out/"session_integrity.csv"),gaps(out/"missing_candles.csv"),issues(out/"file_issues.csv"),compare(out/"timeframe_comparisons.csv");
 if(!sessions||!gaps||!issues||!compare)throw runtime_error("Cannot write report");
 sessions<<"symbol,date,timeframe,expected,observed,missing_trading,missing_closing,coverage\n";
 gaps<<"symbol,date,timeframe,time,classification\n";
 issues<<"symbol,timeframe,malformed,invalid,duplicates,out_of_range,off_grid\n";
 compare<<"symbol,date,timeframe,complete_buckets,matched,mismatched,incomplete_buckets\n";
 long long trading=0,closing=0,mismatches=0,incomplete=0;
 const map<int,Bar> empty;
 for(auto&s:symbols){
  for(int tf:{1,5,15}){auto &d=db[s][tf];issues<<s<<","<<tf<<","<<d.malformed<<","<<d.invalid<<","<<d.duplicates<<","<<d.outside<<","<<d.offgrid<<"\n";}
  for(auto&date:dates){
   for(int tf:{1,5,15}){
    auto &days=db[s][tf].days;auto it=days.find(date);const auto&bars=it==days.end()?empty:it->second;
    int mt=0,mc=0;
    for(int t=555;t<930;t+=tf)if(!bars.count(t)){
     bool isTrading=t<915;if(isTrading)mt++;else mc++;
     gaps<<s<<","<<date<<","<<tf<<","<<clocktime(t)<<","<<(isTrading?"TRADING_WINDOW":"CLOSING_SESSION")<<"\n";
    }
    trading+=mt;closing+=mc;
    sessions<<s<<","<<date<<","<<tf<<","<<375/tf<<","<<bars.size()<<","<<mt<<","<<mc<<","<<(mt+mc?"INCOMPLETE":"COMPLETE")<<"\n";
   }
   for(int tf:{5,15}){
    auto &a=db[s][1].days,&b=db[s][tf].days;
    auto ia=a.find(date),ib=b.find(date);const auto&one=ia==a.end()?empty:ia->second;const auto&higher=ib==b.end()?empty:ib->second;
    int complete=0,match=0,mis=0,inc=0;
    for(int t=555;t<930;t+=tf){
     bool valid=higher.count(t);for(int j=0;j<tf;j++)valid=valid&&one.count(t+j);
     if(!valid){inc++;continue;}
     complete++;Bar agg=one.at(t);agg.h=-1e100;agg.l=1e100;agg.v=0;
     for(int j=0;j<tf;j++){auto&x=one.at(t+j);agg.h=max(agg.h,x.h);agg.l=min(agg.l,x.l);agg.v+=x.v;}
     agg.c=one.at(t+tf-1).c;auto&x=higher.at(t);
     if(close(agg.o,x.o)&&close(agg.h,x.h)&&close(agg.l,x.l)&&close(agg.c,x.c)&&close(agg.v,x.v))match++;else mis++;
    }
    mismatches+=mis;incomplete+=inc;
    compare<<s<<","<<date<<","<<tf<<","<<complete<<","<<match<<","<<mis<<","<<inc<<"\n";
   }
  }
 }
 cout<<"CETE Phase 3.19.3A (read-only)\nTrading-window missing slots: "<<trading<<"\nClosing-session missing slots: "<<closing<<"\nTimeframe mismatches: "<<mismatches<<"\nUnverifiable buckets: "<<incomplete<<"\nReports: "<<out<<"\n";
 cout<<"No synthetic data; no source files modified. COMPLETE does not certify RL readiness.\n";
 }catch(const exception&e){cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
