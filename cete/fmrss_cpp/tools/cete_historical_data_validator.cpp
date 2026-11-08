// CETE historical data integrity validator v2 — C++17, standard library only.
// Read-only for market-data files. Writes a CSV report alongside kite_downloads.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;
struct Candle { std::string symbol,date,time; double o=0,h=0,l=0,c=0,v=0; int minute=-1; };
struct Stats {
    size_t rows=0, parse_errors=0, invalid=0, duplicates=0, outside=0, misaligned=0;
    size_t missing=0, comparable=0, mismatches=0, incomplete=0;
    bool file_missing=false;
    std::map<std::string,std::map<int,Candle>> days;
};
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v; std::stringstream ss(s); std::string item;
    while (std::getline(ss,item,',')) v.push_back(item);
    return v;
}
bool digits(const std::string& s) { return !s.empty() && std::all_of(s.begin(),s.end(),[](unsigned char c){return c>='0'&&c<='9';}); }
bool date_ok(const std::string& s) {
    if(s.size()!=10||s[4]!='-'||s[7]!='-'||!digits(s.substr(0,4))||!digits(s.substr(5,2))||!digits(s.substr(8,2))) return false;
    int y=std::stoi(s.substr(0,4)),m=std::stoi(s.substr(5,2)),d=std::stoi(s.substr(8,2));
    if(y<1900||m<1||m>12) return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int limit=days[m-1]; if(m==2&&(y%400==0||(y%4==0&&y%100!=0))) ++limit;
    return d>=1&&d<=limit;
}
int time_minutes(const std::string& s) {
    if(s.size()!=5||s[2]!=':'||!digits(s.substr(0,2))||!digits(s.substr(3,2))) return -1;
    int h=std::stoi(s.substr(0,2)),m=std::stoi(s.substr(3,2));
    return h<24&&m<60?h*60+m:-1;
}
bool number(const std::string& s,double& value) {
    try {size_t used=0; value=std::stod(s,&used); return used==s.size()&&std::isfinite(value);}catch(...){return false;}
}
bool parse(const std::string& line,Candle& c) {
    auto v=split(line); if(v.size()!=8) return false;
    c.symbol=v[0];c.date=v[1];c.time=v[2];c.minute=time_minutes(c.time);
    return date_ok(c.date)&&c.minute>=0&&number(v[3],c.o)&&number(v[4],c.h)&&number(v[5],c.l)&&number(v[6],c.c)&&number(v[7],c.v);
}
bool valid(const Candle& c) {
    return c.o>0&&c.h>0&&c.l>0&&c.c>0&&c.v>=0&&
        c.h+1e-8>=std::max({c.o,c.l,c.c})&&c.l-1e-8<=std::min({c.o,c.h,c.c});
}
Stats load(const fs::path& path,const std::string& expected,int step,const std::string& start,const std::string& end) {
    Stats s; std::ifstream in(path); if(!in){s.file_missing=true;return s;}
    std::string line;
    while(std::getline(in,line)) {
        ++s.rows; Candle c;
        if(!parse(line,c)||c.symbol!=expected){++s.parse_errors;continue;}
        if(!valid(c)){++s.invalid;continue;}
        if(c.date<start||c.date>end){++s.outside;continue;}
        int offset=c.minute-555; // 09:15 IST
        if(offset<0||c.minute>=930||offset%step!=0){++s.misaligned;continue;}
        if(!s.days[c.date].emplace(c.minute,c).second) ++s.duplicates;
    }
    // Missing candle counts only for dates with at least one observed valid candle.
    for(const auto& day:s.days) for(int t=555;t<930;t+=step) if(!day.second.count(t)) ++s.missing;
    return s;
}
bool near(double a,double b) {return std::abs(a-b)<=std::max(0.011,std::max(std::abs(a),std::abs(b))*1e-5);}
void compare(Stats& higher,const Stats& one,int step) {
    for(const auto& day:higher.days) for(const auto& bar:day.second) {
        const Candle& c=bar.second; auto day_it=one.days.find(day.first);
        if(day_it==one.days.end()){++higher.incomplete;continue;}
        const auto& src=day_it->second; std::vector<const Candle*> group;
        for(int t=bar.first;t<bar.first+step;++t){auto it=src.find(t);if(it==src.end())break;group.push_back(&it->second);}
        if(static_cast<int>(group.size())!=step){++higher.incomplete;continue;}
        ++higher.comparable;
        double hi=group.front()->h,lo=group.front()->l,volume=0;
        for(const auto* x:group){hi=std::max(hi,x->h);lo=std::min(lo,x->l);volume+=x->v;}
        if(!near(c.o,group.front()->o)||!near(c.h,hi)||!near(c.l,lo)||!near(c.c,group.back()->c)||
           std::abs(c.v-volume)>std::max(1.0,volume*1e-5)) ++higher.mismatches;
    }
}
int main(int argc,char** argv) {
    fs::path root=argc>1?argv[1]:"data/validation/phase319_3a/kite_downloads";
    std::string start=argc>2?argv[2]:"2025-10-29",end=argc>3?argv[3]:"2026-10-28";
    if(!date_ok(start)||!date_ok(end)||start>end){std::cerr<<"Invalid date range. Usage: "<<argv[0]<<" [root] [YYYY-MM-DD] [YYYY-MM-DD]\n";return 1;}
    for(const auto* folder:{"minute","5minutes","15minutes"}) if(!fs::is_directory(root/folder)){
        std::cerr<<"Missing directory: "<<(root/folder)<<'\n';return 1;
    }
    std::set<std::string> symbols;
    for(const auto& e:fs::directory_iterator(root/"minute")) if(e.is_regular_file()){
        std::string n=e.path().filename().string(),suffix="_1min.txt";
        if(n.size()>suffix.size()&&n.compare(n.size()-suffix.size(),suffix.size(),suffix)==0)
            symbols.insert(n.substr(0,n.size()-suffix.size()));
    }
    if(symbols.empty()){std::cerr<<"No *_1min.txt files found\n";return 1;}
    fs::path report=root.parent_path()/"historical_integrity_report.csv";
    std::ofstream out(report);if(!out){std::cerr<<"Cannot write: "<<report<<'\n';return 1;}
    out<<"symbol,timeframe,file_missing,rows,parse_errors,invalid_ohlcv,duplicates,out_of_range,misaligned,missing_in_observed_sessions,compared_bars,ohlcv_mismatches,incomplete_1m_comparisons\n";
    size_t total_mismatch=0,total_missing=0,total_outside=0,missing_files=0;
    for(const auto& symbol:symbols){
        Stats one=load(root/"minute"/(symbol+"_1min.txt"),symbol,1,start,end);
        Stats five=load(root/"5minutes"/(symbol+"_5mins.txt"),symbol,5,start,end);
        Stats fifteen=load(root/"15minutes"/(symbol+"_15mins.txt"),symbol,15,start,end);
        compare(five,one,5);compare(fifteen,one,15);
        auto write=[&](const char* tf,const Stats& s){
            out<<symbol<<','<<tf<<','<<s.file_missing<<','<<s.rows<<','<<s.parse_errors<<','<<s.invalid<<','<<s.duplicates<<','<<s.outside<<','<<s.misaligned<<','<<s.missing<<','<<s.comparable<<','<<s.mismatches<<','<<s.incomplete<<'\n';
            total_mismatch+=s.mismatches;total_missing+=s.missing;total_outside+=s.outside;missing_files+=s.file_missing;
            std::cout<<std::left<<std::setw(15)<<symbol<<std::setw(10)<<tf<<" rows="<<s.rows<<" outside="<<s.outside<<" missing="<<s.missing<<" mismatches="<<s.mismatches<<(s.file_missing?" FILE MISSING":"")<<'\n';
        };
        write("1minute",one);write("5minute",five);write("15minute",fifteen);
    }
    std::cout<<"\nSymbols: "<<symbols.size()<<" | Missing files: "<<missing_files<<" | Out-of-range rows: "<<total_outside<<" | Missing observed-session slots: "<<total_missing<<" | OHLCV mismatches: "<<total_mismatch<<'\n';
    std::cout<<"REPORT: "<<fs::absolute(report)<<'\n';
    std::cout<<"Note: no exchange holiday/special-session calendar; missing counts cover observed dates only.\n";
    std::cout<<"Market data files were not modified.\n";
    return 0;
}
