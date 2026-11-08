// CETE H4.7 Stage 5E.2K.6B.1 - read-only mathematical baseline ledger audit.
// Standard C++20; does not depend on or modify CETE trading code.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Row {
    std::int64_t t{};
    std::string previous, desired, held;
    long long qty{}, legs{};
    double cash{}, before{}, after{}, gross{}, brokerage{}, slippage{}, other{}, net{}, dd{};
};
std::vector<std::string> csvFields(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    bool quoted = false;
    for (std::size_t i=0; i<line.size(); ++i) {
        const char c=line[i];
        if (c=='"') {
            if (quoted && i+1<line.size() && line[i+1]=='"') { field+='"'; ++i; }
            else quoted=!quoted;
        } else if (c==',' && !quoted) { out.push_back(field); field.clear(); }
        else field+=c;
    }
    if (quoted) throw std::runtime_error("Unclosed quoted CSV field");
    out.push_back(field);
    return out;
}
std::string required(const std::map<std::string,std::size_t>& header,
                     const std::vector<std::string>& f, const std::string& key) {
    auto it=header.find(key);
    if (it==header.end() || it->second>=f.size()) throw std::runtime_error("Missing CSV column: "+key);
    return f[it->second];
}
double number(const std::string& s, const std::string& key) {
    std::size_t used=0;
    if (s.empty()) throw std::runtime_error("Empty number in "+key);
    const double n=std::stod(s,&used);
    if (used!=s.size() || !std::isfinite(n)) throw std::runtime_error("Invalid number in "+key+": "+s);
    return n;
}
long long integer(const std::string& s, const std::string& key) {
    std::size_t used=0;
    if (s.empty()) throw std::runtime_error("Empty integer in "+key);
    const auto n=std::stoll(s,&used);
    if (used!=s.size()) throw std::runtime_error("Invalid integer in "+key);
    return n;
}
std::string istDay(std::int64_t epoch) {
    // Civil date algorithm (Howard Hinnant), no timezone database needed.
    auto days=(epoch+19800)/86400;
    if ((epoch+19800)<0 && (epoch+19800)%86400) --days;
    days+=719468;
    const auto era=(days>=0 ? days : days-146096)/146097;
    const auto doe=static_cast<unsigned>(days-era*146097);
    const auto yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    auto y=static_cast<int>(yoe)+static_cast<int>(era*400);
    const auto doy=doe-(365*yoe+yoe/4-yoe/100);
    const auto mp=(5*doy+2)/153;
    const auto d=doy-(153*mp+2)/5+1;
    const auto m=static_cast<int>(mp)+(mp<10?3:-9);
    y+=(m<=2);
    std::ostringstream os;
    os<<std::setfill('0')<<std::setw(4)<<y<<'-'<<std::setw(2)<<m<<'-'<<std::setw(2)<<d;
    return os.str();
}
int istMinute(std::int64_t epoch) {
    auto sec=(epoch+19800)%86400;
    if (sec<0) sec+=86400;
    return static_cast<int>(sec/60);
}
struct Daily {
    std::size_t rows{}, events{}, entries{}, exits{}, switches{}, anomalies{};
    long long legs{};
    double opening{}, closing{}, gross{}, brokerage{}, slippage{}, other{}, net{}, maxdd{};
    bool first=true;
};
void writeDaily(const std::filesystem::path& path, const std::map<std::string,Daily>& daily) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot open daily report: "+path.string());
    out<<"ist_date,rows,events,entries,exits,switches,executed_legs,opening_equity,closing_equity,equity_change,sum_gross_pnl,sum_brokerage,sum_slippage,sum_other_costs,sum_net_pnl,max_reported_drawdown,anomalies\n";
    out<<std::setprecision(15);
    for (const auto& [day,d]:daily) {
        out<<day<<','<<d.rows<<','<<d.events<<','<<d.entries<<','<<d.exits<<','<<d.switches<<','<<d.legs<<','
           <<d.opening<<','<<d.closing<<','<<(d.closing-d.opening)<<','<<d.gross<<','<<d.brokerage<<','
           <<d.slippage<<','<<d.other<<','<<d.net<<','<<d.maxdd<<','<<d.anomalies<<'\n';
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc!=4) throw std::invalid_argument(
            "Usage: run_cete_h47_stage6b1_math_diagnostic <ledger.csv> <daily.csv> <anomalies.csv>");
        const std::filesystem::path ledgerPath=argv[1], dailyPath=argv[2], anomalyPath=argv[3];
        if (std::filesystem::absolute(ledgerPath)==std::filesystem::absolute(dailyPath) ||
            std::filesystem::absolute(ledgerPath)==std::filesystem::absolute(anomalyPath) ||
            std::filesystem::absolute(dailyPath)==std::filesystem::absolute(anomalyPath))
            throw std::invalid_argument("Input and output paths must be distinct");
        std::ifstream in(ledgerPath);
        if (!in) throw std::runtime_error("Cannot open ledger: "+ledgerPath.string());
        std::string line;
        if (!std::getline(in,line)) throw std::runtime_error("Empty ledger");
        if (!line.empty() && line.back()=='\r') line.pop_back();
        const auto names=csvFields(line);
        std::map<std::string,std::size_t> header;
        for (std::size_t i=0;i<names.size();++i) header[names[i]]=i;
        const std::vector<std::string> needed={"decision_time","previous_symbol","desired_symbol","held_symbol",
            "quantity","cash","equity_before","equity_after","gross_pnl","brokerage","slippage",
            "other_costs","net_pnl","drawdown","executed_legs"};
        for (const auto& k:needed) if (!header.count(k)) throw std::runtime_error("Missing column: "+k);
        if (anomalyPath.has_parent_path()) std::filesystem::create_directories(anomalyPath.parent_path());
        std::ofstream anomalies(anomalyPath);
        if (!anomalies) throw std::runtime_error("Cannot open anomalies report");
        anomalies<<"csv_line,ist_date,decision_time,code,detail\n";
        std::map<std::string,Daily> daily;
        std::size_t rows=0, flagged=0, nonzeroEvents=0;
        long long totalLegs=0;
        double gross=0,brokerage=0,slippage=0,other=0,net=0;
        Row prior{};
        bool havePrior=false;
        double firstEquity=0, lastEquity=0, maxDrawdown=0;
        auto warn=[&](std::size_t csvLine, const std::string& day, std::int64_t t,
                      const std::string& code, const std::string& detail, Daily& d) {
            anomalies<<csvLine<<','<<day<<','<<t<<','<<code<<','<<detail<<'\n';
            ++flagged; ++d.anomalies;
        };
        while (std::getline(in,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.empty()) continue;
            const auto f=csvFields(line);
            Row r;
            auto s=[&](const std::string& key){return required(header,f,key);};
            r.t=integer(s("decision_time"),"decision_time");
            r.previous=s("previous_symbol"); r.desired=s("desired_symbol"); r.held=s("held_symbol");
            r.qty=integer(s("quantity"),"quantity"); r.legs=integer(s("executed_legs"),"executed_legs");
            r.cash=number(s("cash"),"cash"); r.before=number(s("equity_before"),"equity_before");
            r.after=number(s("equity_after"),"equity_after"); r.gross=number(s("gross_pnl"),"gross_pnl");
            r.brokerage=number(s("brokerage"),"brokerage"); r.slippage=number(s("slippage"),"slippage");
            r.other=number(s("other_costs"),"other_costs"); r.net=number(s("net_pnl"),"net_pnl");
            r.dd=number(s("drawdown"),"drawdown");
            const auto day=istDay(r.t);
            auto& d=daily[day];
            if (d.first) {d.opening=r.before; d.first=false;}
            d.closing=r.after; ++d.rows;
            if (rows==0) firstEquity=r.before;
            lastEquity=r.after;
            const auto csvLine=rows+2;
            if (r.qty<0) warn(csvLine,day,r.t,"NEGATIVE_QUANTITY","Long-only baseline must not be short",d);
            if (r.legs<0) warn(csvLine,day,r.t,"NEGATIVE_LEGS","Executed legs cannot be negative",d);
            if (r.t<=0 || (havePrior && r.t<=prior.t))
                warn(csvLine,day,r.t,"NONCHRONOLOGICAL","Decision timestamps must increase",d);
            if (r.dd< -1e-8 || r.dd>1.0+1e-8)
                warn(csvLine,day,r.t,"INVALID_DRAWDOWN","Expected drawdown fraction in [0,1]",d);
            if (r.qty==0 && !r.held.empty())
                warn(csvLine,day,r.t,"FLAT_WITH_HELD_SYMBOL","Held symbol present with zero quantity",d);
            if (r.qty>0 && r.held.empty())
                warn(csvLine,day,r.t,"POSITION_WITHOUT_SYMBOL","Positive quantity with no held symbol",d);
            if (istMinute(r.t)>=15*60+15 && r.qty!=0)
                warn(csvLine,day,r.t,"AFTER_CUTOFF_POSITION","Position still open at/after 15:15 IST",d);
            if (havePrior && istDay(prior.t)==day && std::abs(r.before-prior.after)>0.02)
                warn(csvLine,day,r.t,"EQUITY_DISCONTINUITY","equity_before differs from previous equity_after",d);
            if (r.legs>0) {++d.events; ++nonzeroEvents;}
            if (r.qty>0 && (!havePrior || prior.qty==0)) ++d.entries;
            if (havePrior && prior.qty>0 && r.qty==0) ++d.exits;
            if (havePrior && prior.qty>0 && r.qty>0 && prior.held!=r.held) ++d.switches;
            d.legs+=r.legs; totalLegs+=r.legs;
            d.gross+=r.gross; d.brokerage+=r.brokerage; d.slippage+=r.slippage;
            d.other+=r.other; d.net+=r.net; d.maxdd=std::max(d.maxdd,r.dd);
            gross+=r.gross; brokerage+=r.brokerage; slippage+=r.slippage;
            other+=r.other; net+=r.net; maxDrawdown=std::max(maxDrawdown,r.dd);
            prior=r; havePrior=true; ++rows;
        }
        if (!rows) throw std::runtime_error("Ledger contains no data rows");
        if (prior.qty!=0) warn(rows+1,istDay(prior.t),prior.t,"OPEN_AT_END","Last record holds a position",daily[istDay(prior.t)]);
        writeDaily(dailyPath,daily);
        std::cout<<std::setprecision(12)
            <<"[CETE][H47][5E2K6B1] READ-ONLY LEDGER DIAGNOSTIC COMPLETE\n"
            <<"Rows: "<<rows<<" IST sessions: "<<daily.size()<<" Executed legs (sum): "<<totalLegs
            <<" Rows with executions: "<<nonzeroEvents<<"\n"
            <<"Opening equity: "<<firstEquity<<" Closing equity: "<<lastEquity
            <<" Equity change: "<<(lastEquity-firstEquity)<<"\n"
            <<"SUM of ledger columns (interpretation depends on runner semantics):\n"
            <<"  gross_pnl="<<gross<<" brokerage="<<brokerage<<" slippage="<<slippage
            <<" other_costs="<<other<<" net_pnl="<<net<<"\n"
            <<"  summed costs="<<(brokerage+slippage+other)
            <<" max reported drawdown="<<maxDrawdown<<"\n"
            <<"Flags: "<<flagged<<" (flags are diagnostic, not automatically defects)\n"
            <<"Daily report: "<<dailyPath<<"\nAnomalies: "<<anomalyPath<<'\n';
        // No false claim of accounting reconciliation: per-row PnL may be realized-only,
        // and equity_after includes unrealized mark-to-market changes.
        return flagged==0?0:3;
    } catch (const std::exception& e) {
        std::cerr<<"[CETE][H47][5E2K6B1] ERROR: "<<e.what()<<'\n';
        return 1;
    }
}
