// CETE: Kite Connect multi-symbol historical downloader (C++17)
// Default period: 2025-10-29 through 2026-10-28 (last completed IST trading day).
// Read-only with respect to /home/hadoop/shareMarket_Data.
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <ctime>
using json=nlohmann::json;
namespace fs=std::filesystem;
struct Http {long code=0; std::string body;};
static size_t sink(char* p,size_t a,size_t b,void* u){static_cast<std::string*>(u)->append(p,a*b);return a*b;}
static std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n\"'");if(a==std::string::npos)return "";auto b=s.find_last_not_of(" \t\r\n\"'");return s.substr(a,b-a+1);}
static std::string env(const char* name){const char* p=std::getenv(name);return p?p:"";}
static std::string readToken(){std::string path=env("HOME")+"/.config/cete/kite_access_token";std::ifstream f(path);std::string t;std::getline(f,t);t=trim(t);if(t.empty())throw std::runtime_error("Access token missing: "+path);return t;}
static std::string urlEncode(CURL* c,const std::string& s){char* p=curl_easy_escape(c,s.c_str(),(int)s.size());if(!p)throw std::runtime_error("URL encode failed");std::string r=p;curl_free(p);return r;}
static Http request(const std::string& url,const std::string& apiKey,const std::string& accessToken){
  CURL* c=curl_easy_init();if(!c)throw std::runtime_error("curl init failed");
  curl_slist* h=nullptr;h=curl_slist_append(h,"X-Kite-Version: 3");
  std::string auth="Authorization: token "+apiKey+":"+accessToken;h=curl_slist_append(h,auth.c_str());
  Http r;curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);
  curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,sink);curl_easy_setopt(c,CURLOPT_WRITEDATA,&r.body);
  curl_easy_setopt(c,CURLOPT_TIMEOUT,75L);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
  curl_easy_setopt(c,CURLOPT_ACCEPT_ENCODING,"");
  auto rc=curl_easy_perform(c);curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&r.code);
  curl_slist_free_all(h);curl_easy_cleanup(c);
  if(rc!=CURLE_OK)throw std::runtime_error(std::string("Network error: ")+curl_easy_strerror(rc));
  return r;
}
static Http requestRetry(const std::string& url,const std::string& key,const std::string& token){
  for(int i=0;i<5;++i){
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    try {Http r=request(url,key,token);
      if(r.code==200)return r;
      if(r.code==401||r.code==403)throw std::runtime_error("Authentication denied (HTTP "+std::to_string(r.code)+"): "+r.body.substr(0,350));
      if(r.code!=429 && r.code<500)throw std::runtime_error("HTTP "+std::to_string(r.code)+": "+r.body.substr(0,350));
      std::cerr<<"Retryable HTTP "<<r.code<<" (attempt "<<i+1<<")\n";
    } catch(const std::runtime_error& e){std::string s=e.what();if(s.find("Authentication denied")!=std::string::npos||s.rfind("HTTP ",0)==0)throw;std::cerr<<s<<"\n";}
    std::this_thread::sleep_for(std::chrono::seconds(2*(i+1)));
  }
  throw std::runtime_error("Request failed after retries");
}
static std::vector<std::string> csv(const std::string& line){std::vector<std::string> v;std::string x;bool quoted=false;for(size_t i=0;i<line.size();++i){char c=line[i];if(c=='\"'){if(quoted&&i+1<line.size()&&line[i+1]=='\"'){x+='\"';++i;}else quoted=!quoted;}else if(c==','&&!quoted){v.push_back(x);x.clear();}else x+=c;}v.push_back(x);return v;}
static std::vector<std::string> symbols(const fs::path& file){std::ifstream f(file);if(!f)throw std::runtime_error("Cannot read "+file.string());std::set<std::string> seen;std::vector<std::string> out;std::string line;while(std::getline(f,line)){line=trim(line);if(line.empty()||line[0]=='#')continue;auto fields=csv(line);std::string s=trim(fields[0]);if(s=="SYMBOL"||s=="symbol"||s=="ticker"||s=="TICKER")continue;if(s=="NIFTY%2050"||s=="NIFTY 50")s="NIFTY 50";if(seen.insert(s).second)out.push_back(s);}if(seen.insert("NIFTY 50").second)out.push_back("NIFTY 50");return out;}
static std::map<std::string,std::string> instrumentTokens(const std::string& key,const std::string& token){
  auto r=requestRetry("https://api.kite.trade/instruments",key,token);
  std::istringstream in(r.body);std::string line;std::getline(in,line);auto hdr=csv(line);
  auto idx=[&](const std::string& name){auto i=std::find(hdr.begin(),hdr.end(),name);if(i==hdr.end())throw std::runtime_error("Instrument CSV missing "+name);return size_t(i-hdr.begin());};
  auto ti=idx("instrument_token"),si=idx("tradingsymbol"),ei=idx("exchange"),ty=idx("instrument_type");
  std::map<std::string,std::string> result;
  while(std::getline(in,line)){auto c=csv(line);if(c.size()<=std::max({ti,si,ei,ty}))continue;
    if(c[ei]=="NSE"&&c[ty]=="EQ")result.emplace(c[si],c[ti]);
    if(c[si]=="NIFTY 50"&&(c[ei]=="NSE"||c[ei]=="NIFTY"))result["NIFTY 50"]=c[ti];
  }return result;
}
static std::time_t parseDay(const std::string& s){std::tm t={};std::istringstream ss(s);ss>>std::get_time(&t,"%Y-%m-%d");if(ss.fail()||s.size()!=10)throw std::runtime_error("Invalid date: "+s);t.tm_hour=12;t.tm_isdst=0;return timegm(&t);}
static std::string day(std::time_t t){std::tm* p=gmtime(&t);char buf[16];std::strftime(buf,sizeof(buf),"%Y-%m-%d",p);return buf;}
static std::string formatNumber(const json& v){if(!v.is_number())throw std::runtime_error("Non-numeric OHLCV");std::ostringstream s;s<<std::setprecision(15)<<v.get<double>();return s.str();}
static std::map<std::string,std::string> loadExisting(const fs::path& path,const std::string& sym){std::map<std::string,std::string> m;std::ifstream f(path);std::string line;while(std::getline(f,line)){if(line.empty())continue;auto p=csv(line);if(p.size()!=8||p[0]!=sym||p[1].size()!=10||p[2].size()<5)throw std::runtime_error("Invalid existing row in "+path.string());std::string k=p[1]+" "+p[2].substr(0,5);if(!m.emplace(k,line).second)throw std::runtime_error("Duplicate existing candle: "+k);}return m;}
static void atomicSave(const fs::path& path,const std::map<std::string,std::string>& rows){fs::create_directories(path.parent_path());fs::path tmp=path;tmp+=".tmp";{std::ofstream f(tmp,std::ios::trunc);if(!f)throw std::runtime_error("Cannot write "+tmp.string());for(auto& [k,v]:rows)f<<v<<'\n';f.flush();if(!f)throw std::runtime_error("Write failed "+tmp.string());}fs::rename(tmp,path);}
static void download(const std::string& sym,const std::string& instrument,const std::string& interval,const std::string& folder,const std::string& suffix,int maxDays,const fs::path& base,std::time_t from,std::time_t to,const std::string& key,const std::string& token){
  const std::string filename=(sym=="NIFTY 50"?"NIFTY%2050":sym)+suffix;
  fs::path path=base/folder/filename;
  auto rows=loadExisting(path,sym=="NIFTY 50"?"NIFTY%2050":sym);
  size_t added=0,conflicts=0,requests=0;
  CURL* c=curl_easy_init();if(!c)throw std::runtime_error("curl init failed");
  try{
    for(std::time_t start=from;start<=to;start+=(std::time_t)maxDays*86400){
      auto end=std::min(to,start+(std::time_t)(maxDays-1)*86400);
      std::string fromStr=day(start)+" 00:00:00",toStr=day(end)+" 23:59:59";
      std::string url="https://api.kite.trade/instruments/historical/"+instrument+"/"+interval+"?from="+urlEncode(c,fromStr)+"&to="+urlEncode(c,toStr)+"&continuous=0&oi=0";
      auto response=requestRetry(url,key,token);++requests;
      auto doc=json::parse(response.body);
      if(doc.value("status","")!="success")throw std::runtime_error("Kite error: "+response.body.substr(0,300));
      const auto& candles=doc.at("data").at("candles");
      for(auto& a:candles){if(!a.is_array()||a.size()<6)throw std::runtime_error("Malformed candle");
        std::string stamp=a.at(0).get<std::string>();if(stamp.size()<19||stamp.substr(10,1)!="T")throw std::runtime_error("Malformed timestamp: "+stamp);
        if(stamp.find("+0530")==std::string::npos&&stamp.find("+05:30")==std::string::npos)throw std::runtime_error("Unexpected candle timezone: "+stamp);
        std::string d=stamp.substr(0,10),t=stamp.substr(11,5);if(d<day(from)||d>day(to))continue;
        double o=a[1].get<double>(),h=a[2].get<double>(),l=a[3].get<double>(),cl=a[4].get<double>(),vol=a[5].get<double>();
        if(!std::isfinite(o)||!std::isfinite(h)||!std::isfinite(l)||!std::isfinite(cl)||!std::isfinite(vol)||o<=0||l<=0||h<std::max(o,cl)||l>std::min(o,cl)||vol<0)throw std::runtime_error("Invalid OHLCV "+stamp);
        std::string symbolFile=(sym=="NIFTY 50"?"NIFTY%2050":sym);
        std::string row=symbolFile+","+d+","+t;
        for(int i=1;i<=5;++i)row+=","+formatNumber(a[i]);
        auto [it,inserted]=rows.emplace(d+" "+t,row);if(inserted)++added;else if(it->second!=row)++conflicts;
      }
      atomicSave(path,rows); // checkpoint after each successful chunk
      std::cout<<"    "<<day(start)<<".."<<day(end)<<" API="<<candles.size()<<" total="<<rows.size()<<"\n";
    }
  }catch(...){curl_easy_cleanup(c);throw;}
  curl_easy_cleanup(c);
  std::cout<<"  SAVED "<<path<<" | new="<<added<<" existing/format differences="<<conflicts<<" requests="<<requests<<"\n";
}
int main(int argc,char** argv){try{
  if(argc!=1&&argc!=3){std::cerr<<"Usage: "<<argv[0]<<" [START_DATE END_DATE] (YYYY-MM-DD)\n";return 2;}
  const std::string startDate=argc==3?argv[1]:"2025-10-29";
  const std::string endDate=argc==3?argv[2]:"2026-10-28";
  auto start=parseDay(startDate),end=parseDay(endDate);if(start>end)throw std::runtime_error("Start after end");
  std::string key=env("KITE_API_KEY");if(key.empty()){std::cout<<"Enter Kite API key: ";std::getline(std::cin,key);key=trim(key);}if(key.empty())throw std::runtime_error("Missing Kite API key");
  std::string token=readToken();
  const fs::path root="/home/devinderjeet/fmrss_cpp";
  const fs::path base=root/"data/validation/phase319_3a/kite_downloads";
  auto universe=symbols(root/"config/shares.txt");
  auto tokens=instrumentTokens(key,token);
  std::cout<<"CETE Kite historical downloader | "<<startDate<<" .. "<<endDate<<"\n";
  std::cout<<"Staging root: "<<base<<"\n";
  std::cout<<"Symbols: "<<universe.size()<<" (including NIFTY 50)\n";
  fs::create_directories(base/"minute");fs::create_directories(base/"5minutes");fs::create_directories(base/"15minutes");
  size_t failed=0;
  for(const auto& sym:universe){auto it=tokens.find(sym);if(it==tokens.end()){std::cerr<<"MISSING INSTRUMENT: "<<sym<<" (not downloaded)\n";++failed;continue;}
    std::cout<<"\n"<<sym<<" token="<<it->second<<"\n";
    for(auto& [interval,folder,suffix,days]:std::vector<std::tuple<std::string,std::string,std::string,int>>{
      {"minute","minute","_1min.txt",50},
      {"5minute","5minutes","_5mins.txt",90},
      {"15minute","15minutes","_15mins.txt",180}}){
      try{std::cout<<"  "<<interval<<"\n";download(sym,it->second,interval,folder,suffix,days,base,start,end,key,token);}
      catch(const std::exception& e){++failed;std::cerr<<"  FAILED "<<sym<<" "<<interval<<": "<<e.what()<<"\n";}
    }
  }
  std::cout<<"\nFINISHED. Failed symbol/timeframe jobs: "<<failed<<"\n";
  std::cout<<"Original /home/hadoop/shareMarket_Data unchanged.\n";
  return failed?1:0;
}catch(const std::exception& e){std::cerr<<"FATAL: "<<e.what()<<"\n";return 1;}}
