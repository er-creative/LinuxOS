#include "cete/replay/ReplayEligibilityGate.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cete::replay {
namespace {
std::string trim(std::string s) {
    auto first=s.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return {};
    auto last=s.find_last_not_of(" \t\r\n");
    return s.substr(first,last-first+1);
}
std::vector<std::string> parseCsv(const std::string& line) {
    std::vector<std::string> fields;
    std::string value; bool quoted=false;
    for(std::size_t i=0;i<line.size();++i) {
        char c=line[i];
        if(c=='"') {
            if(quoted && i+1<line.size() && line[i+1]=='"') {value+='"';++i;}
            else quoted=!quoted;
        } else if(c==','&&!quoted) {fields.push_back(trim(value));value.clear();}
        else value+=c;
    }
    if(quoted)throw std::runtime_error("Unclosed CSV quote");
    fields.push_back(trim(value));return fields;
}
struct Reader {
    std::ifstream file;
    std::map<std::string,std::size_t> columns;
    std::size_t line=1;
    explicit Reader(const std::string& path):file(path) {
        if(!file)throw std::runtime_error("Cannot open CSV: "+path);
        std::string header;
        if(!std::getline(file,header))throw std::runtime_error("Empty CSV: "+path);
        if(header.size()>=3 && (unsigned char)header[0]==0xEF &&
           (unsigned char)header[1]==0xBB && (unsigned char)header[2]==0xBF)
            header.erase(0,3);
        auto names=parseCsv(header);
        for(std::size_t i=0;i<names.size();++i)
            if(!columns.emplace(names[i],i).second)
                throw std::runtime_error("Duplicate column: "+names[i]);
    }
    void require(std::initializer_list<const char*> names) const {
        for(const char* n:names)if(!columns.count(n))
            throw std::runtime_error(std::string("Missing CSV column: ")+n);
    }
    std::string get(const std::vector<std::string>& f,const std::string& n) const {
        auto i=columns.at(n);
        if(i>=f.size())throw std::runtime_error("Short CSV row at line "+std::to_string(line));
        return f[i];
    }
    bool flag(const std::vector<std::string>& f,const std::string& n) const {
        auto v=get(f,n);
        if(v!="0"&&v!="1")throw std::runtime_error("Invalid boolean "+n+" at line "+std::to_string(line));
        return v=="1";
    }
    bool next(std::vector<std::string>& f) {
        std::string s;
        while(std::getline(file,s)) {
            ++line;
            if(trim(s).empty())continue;
            f=parseCsv(s);return true;
        }
        return false;
    }
};
std::string dailyKey(const std::string& d,const std::string& s){return d+"|"+s;}
std::string decisionKey(const std::string& d,const std::string& t,const std::string& s){return d+"|"+t+"|"+s;}
int minuteOfDay(const std::string& s) {
    if(s.size()!=5||s[2]!=':'||!std::isdigit((unsigned char)s[0])||
       !std::isdigit((unsigned char)s[1])||!std::isdigit((unsigned char)s[3])||
       !std::isdigit((unsigned char)s[4]))return -1;
    int h=(s[0]-'0')*10+s[1]-'0',m=(s[3]-'0')*10+s[4]-'0';
    return h<24&&m<60?h*60+m:-1;
}
} // namespace
void ReplayEligibilityGate::load(const std::string& dailyCsv,const std::string& decisionCsv) {
    std::map<std::string,Daily> daily;
    std::map<std::string,Decision> decisions;
    {
        Reader r(dailyCsv);
        r.require({"symbol","date","decision_data_eligible","full_session_eligible","reason"});
        std::vector<std::string> f;
        while(r.next(f)) {
            auto sym=r.get(f,"symbol"),date=r.get(f,"date");
            if(sym.empty()||date.size()!=10)throw std::runtime_error("Invalid daily key");
            Daily d; d.decisionEligible=r.flag(f,"decision_data_eligible");
            d.fullSessionEligible=r.flag(f,"full_session_eligible");
            d.reason=r.get(f,"reason");
            if(r.columns.count("structural_issues"))d.structural=r.flag(f,"structural_issues");
            if(r.columns.count("timeframe_mismatches"))d.mismatch=r.flag(f,"timeframe_mismatches");
            if(!daily.emplace(dailyKey(date,sym),std::move(d)).second)
                throw std::runtime_error("Duplicate daily row: "+date+" "+sym);
        }
    }
    {
        Reader r(decisionCsv);
        r.require({"date","time","symbol","one_minute_present","completed_5m_available",
                   "completed_15m_available","benchmark_available","warmup_met",
                   "decision_eligible","forward_label_eligible","reason"});
        std::vector<std::string> f;
        while(r.next(f)) {
            auto date=r.get(f,"date"),time=r.get(f,"time"),sym=r.get(f,"symbol");
            int minute=minuteOfDay(time);
            if(date.size()!=10||sym.empty()||minute<555||minute>=915)
                throw std::runtime_error("Invalid decision key at line "+std::to_string(r.line));
            Decision d;
            d.oneMinute=r.flag(f,"one_minute_present");
            d.five=r.flag(f,"completed_5m_available");
            d.fifteen=r.flag(f,"completed_15m_available");
            d.benchmark=r.flag(f,"benchmark_available");
            d.warmup=r.flag(f,"warmup_met");
            d.eligible=r.flag(f,"decision_eligible");
            d.forward=r.flag(f,"forward_label_eligible");
            d.reason=r.get(f,"reason");
            // Fail closed if a report incorrectly labels a structurally invalid row eligible.
            if(d.eligible&&!(d.oneMinute&&d.five&&d.fifteen&&d.benchmark&&d.warmup))
                throw std::runtime_error("Inconsistent eligible row at line "+std::to_string(r.line));
            if(!decisions.emplace(decisionKey(date,time,sym),std::move(d)).second)
                throw std::runtime_error("Duplicate decision row: "+date+" "+time+" "+sym);
        }
    }
    if(daily.empty()||decisions.empty())throw std::runtime_error("Empty eligibility reports");
    daily_.swap(daily);decisions_.swap(decisions);
    hybrid_=false; sessions_.clear(); dates_.clear();
}
DecisionStatus ReplayEligibilityGate::check(const std::string& date,const std::string& time,
                                             const std::string& symbol) const {
    if(hybrid_)return hybridCheck(date,time,symbol);
    int minute=minuteOfDay(time);
    if(minute<555||minute>=915)return {false,false,"OUTSIDE_DECISION_WINDOW"};
    auto day=daily_.find(dailyKey(date,symbol));
    if(day==daily_.end())return {false,false,"DAILY_REPORT_MISSING"};
    if(!day->second.decisionEligible)return {false,false,"DAILY_INELIGIBLE:"+day->second.reason};
    auto it=decisions_.find(decisionKey(date,time,symbol));
    if(it==decisions_.end())return {false,false,"DECISION_REPORT_MISSING"};
    const auto& d=it->second;
    if(!d.eligible)return {false,false,d.reason.empty()?"DECISION_INELIGIBLE":d.reason};
    return {true,d.forward,"OK"};
}
UniverseStatus ReplayEligibilityGate::evaluate(const std::string& date,const std::string& time,
    const std::vector<std::string>& universe,std::size_t minCandidates) const {
    UniverseStatus out;
    int decisionMinute=minuteOfDay(time);
    if(decisionMinute<555||decisionMinute>=915) {
        out.reason="OUTSIDE_DECISION_WINDOW";return out;
    }
    if(minCandidates==0||universe.empty()||minCandidates>universe.size()) {
        out.reason="INVALID_UNIVERSE_CONFIGURATION";return out;
    }
    std::set<std::string> unique;
    if(hybrid_ && !hybridCheck(date,time,"NIFTY%2050").allowed) {
        out.reason="BENCHMARK_UNAVAILABLE";return out;
    }
    for(const auto& symbol:universe) {
        if(symbol.empty()||!unique.insert(symbol).second) {
            out.reason="DUPLICATE_OR_EMPTY_SYMBOL";return out;
        }
    }
    // 3B decision CSV has STOCK rows only. Benchmark availability is a
    // validated field in each stock row; the benchmark has a daily row.
    auto benchmarkDay=daily_.find(dailyKey(date,"NIFTY%2050"));
    if(benchmarkDay==daily_.end()||(!hybrid_&&!benchmarkDay->second.decisionEligible) ||
       (hybrid_&&(benchmarkDay->second.structural||benchmarkDay->second.mismatch))) {
        out.reason="BENCHMARK_DAILY_BLOCKED";return out;
    }
    if(hybrid_ && !hybridCheck(date,time,"NIFTY%2050").allowed) {
        out.reason="BENCHMARK_UNAVAILABLE";return out;
    }
    for(const auto& symbol:universe) {
        auto s=check(date,time,symbol);
        out.byStock.emplace(symbol,s);
        if(s.allowed)out.eligibleStocks.push_back(symbol);
    }
    out.ready=out.eligibleStocks.size()>=minCandidates;
    out.reason=out.ready?"READY":"INSUFFICIENT_ELIGIBLE_STOCKS";
    return out;
}

int ReplayEligibilityGate::tfIndex(int tf) {
    if(tf==1)return 0;
    if(tf==5)return 1;
    if(tf==15)return 2;
    throw std::runtime_error("Unsupported timeframe");
}
void ReplayEligibilityGate::loadHybrid(const std::string& dailyCsv,
        const std::string& decisionCsv, const std::string& phase3aDirectory,
        HybridConfig config) {
    // Load legacy reports for structural checks and backward-compatible row counts.
    load(dailyCsv,decisionCsv);
    if(config.historical1m==0||config.historical5m==0||config.historical15m==0)
        throw std::runtime_error("Hybrid historical lookbacks must be positive");
    std::map<std::string,Session> sessions;
    std::set<std::string> dates;
    const std::filesystem::path dir(phase3aDirectory);
    {
        Reader r((dir/"session_integrity.csv").string());
        r.require({"symbol","date","timeframe","expected","observed","missing_trading","missing_closing"});
        std::vector<std::string> f;
        while(r.next(f)) {
            auto sym=r.get(f,"symbol"), date=r.get(f,"date");
            if(!daily_.count(dailyKey(date,sym)))throw std::runtime_error("3A session not in 3B daily: "+date+" "+sym);
            int tf=std::stoi(r.get(f,"timeframe")),i=tfIndex(tf);
            auto& session=sessions[dailyKey(date,sym)];
            if(session.present[i])throw std::runtime_error("Duplicate 3A session");
            session.present[i]=true;
            if(std::stoi(r.get(f,"expected"))!=375/tf)throw std::runtime_error("Unexpected 3A session length");
            dates.insert(date);
        }
    }
    {
        Reader r((dir/"missing_candles.csv").string());
        r.require({"symbol","date","timeframe","time","classification"});
        std::vector<std::string> f;
        while(r.next(f)) {
            auto sym=r.get(f,"symbol"),date=r.get(f,"date");
            int tf=std::stoi(r.get(f,"timeframe"));
            int minute=minuteOfDay(r.get(f,"time"));
            if(minute<555||minute>=930||(minute-555)%tf!=0)
                throw std::runtime_error("Invalid missing candle timestamp");
            auto it=sessions.find(dailyKey(date,sym));
            if(it==sessions.end()||!it->second.present[tfIndex(tf)])
                throw std::runtime_error("Missing candle without 3A session");
            if(!it->second.missing[tfIndex(tf)].insert(minute).second)
                throw std::runtime_error("Duplicate missing candle");
        }
    }
    for(const auto& [key,day]:daily_) {
        auto it=sessions.find(key);
        if(it==sessions.end()||!it->second.present[0]||!it->second.present[1]||!it->second.present[2])
            throw std::runtime_error("3A session missing for "+key);
    }
    sessions_.swap(sessions);
    dates_.swap(dates);
    config_=config;
    hybrid_=true;
}
bool ReplayEligibilityGate::candlePresent(const std::string& date,
        const std::string& symbol,int tf,int startMinute) const {
    auto it=sessions_.find(dailyKey(date,symbol));
    if(it==sessions_.end()||startMinute<555||startMinute>=930||
       (startMinute-555)%tf!=0)return false;
    int i=tfIndex(tf);
    return it->second.present[i]&&!it->second.missing[i].count(startMinute);
}
std::size_t ReplayEligibilityGate::availableCount(const std::string& date,
        const std::string& time,const std::string& symbol,int tf) const {
    int now=minuteOfDay(time);
    if(now<0)return 0;
    std::size_t total=0;
    for(auto it=dates_.begin();it!=dates_.end()&&*it<=date;++it) {
        auto session=sessions_.find(dailyKey(*it,symbol));
        if(session==sessions_.end())continue;
        int i=tfIndex(tf);
        if(!session->second.present[i])continue;
        int count=*it==date ? std::clamp((now-555)/tf,0,375/tf) : 375/tf;
        // A bar at 09:15 completes at 09:15+tf; count only completed bars.
        int boundary=555+count*tf;
        const auto& gaps=session->second.missing[i];
        auto missingBefore=static_cast<std::size_t>(std::distance(gaps.begin(),gaps.lower_bound(boundary)));
        total+=static_cast<std::size_t>(count)-missingBefore;
    }
    return total;
}
DecisionStatus ReplayEligibilityGate::hybridCheck(const std::string& date,
        const std::string& time,const std::string& symbol) const {
    DecisionStatus result;
    int now=minuteOfDay(time);
    if(now<555||now>=915){result.reason="OUTSIDE_DECISION_WINDOW";return result;}
    auto d=daily_.find(dailyKey(date,symbol));
    if(d==daily_.end()){result.reason="DAILY_REPORT_MISSING";return result;}
    if(d->second.structural||d->second.mismatch){result.reason="STRUCTURAL_OR_TIMEFRAME_MISMATCH";return result;}
    if(!sessions_.count(dailyKey(date,symbol))){result.reason="SESSION_REPORT_MISSING";return result;}
    if(availableCount(date,time,symbol,1)<config_.historical1m ||
       availableCount(date,time,symbol,5)<config_.historical5m ||
       availableCount(date,time,symbol,15)<config_.historical15m) {
        result.reason="INSUFFICIENT_HISTORICAL_WARMUP";return result;
    }
    result.allowed=true;
    // Current-session 1m bar labeled now-1 completes at now.
    result.current1m=now>=556&&candlePresent(date,symbol,1,now-1);
    // Last current-session completed 5m and 15m bars.
    int elapsed=now-555;
    if(elapsed>=5)result.current5m=candlePresent(date,symbol,5,555+((elapsed/5)-1)*5);
    if(elapsed>=15)result.current15m=candlePresent(date,symbol,15,555+((elapsed/15)-1)*15);
    result.openEligible=!config_.requireCurrent1mForOpen||result.current1m;
    result.reason=result.openEligible?"OK": "AWAIT_CURRENT_1M_FOR_OPEN";
    // Future-label flags in 3B were computed using same-day future data and
    // must not enter decision-time evaluation. Offline labels are separate.
    result.forwardLabelEligible=false;
    return result;
}
} // namespace cete::replay
