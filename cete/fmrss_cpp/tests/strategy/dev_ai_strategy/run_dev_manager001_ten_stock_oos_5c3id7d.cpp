#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalStrategyD7B.hpp"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai::strategy;
namespace {
using Key = std::int64_t;
int number(const std::string& s, std::size_t pos, std::size_t len) {
    int n=0;
    for(std::size_t i=pos;i<pos+len;++i) {
        if(i>=s.size() || s[i]<'0' || s[i]>'9') throw std::invalid_argument("Bad OOS date digits");
        n=n*10+(s[i]-'0');
    }
    return n;
}
bool leap(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
Key dayOrdinal(int y,unsigned m,unsigned d) {
    y-=m<=2;
    const int era=(y>=0?y:y-399)/400;
    const unsigned yoe=static_cast<unsigned>(y-era*400);
    const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    return static_cast<Key>(era)*146097+doe;
}
Key parseOOSDay(const std::string& s) {
    if(s.size()!=10 || s[4]!='-' || s[7]!='-') throw std::invalid_argument("OOS date must be YYYY-MM-DD");
    const int y=number(s,0,4), m=number(s,5,2), d=number(s,8,2);
    static const int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<1900 || y>2200 || m<1 || m>12 || d<1 || d>days[m]+(m==2 && leap(y)))
        throw std::invalid_argument("Invalid OOS date");
    return dayOrdinal(y,static_cast<unsigned>(m),static_cast<unsigned>(d));
}
struct Partition {
    std::size_t decisions=0, evaluated=0, rejected=0, selected=0;
    std::size_t consecutive=0, non5=0, carried5=0, carried15=0, empty=0;
    Key last=0;
};
void inspect(Partition& p,const DevManager001RankingFrame& f,
             const std::set<std::string>& universe,std::size_t top_k) {
    const Key key=f.decision_key;
    if(f.manager_id!="DevManager_001" || key<=0 || (p.last && key<=p.last))
        throw std::runtime_error("Manager identity or decision order invalid");
    if(p.last && key==p.last+1) ++p.consecutive;
    if(key%5!=0) ++p.non5;
    p.last=key; ++p.decisions;
    if(f.evaluated!=universe.size() || f.rejected>f.evaluated ||
       f.selected.size()>top_k || f.selected.size()+f.rejected>f.evaluated)
        throw std::runtime_error("Full-universe evaluated/rejected/Top-K mismatch");
    p.evaluated+=f.evaluated; p.rejected+=f.rejected; p.selected+=f.selected.size();
    if(f.selected.empty()) ++p.empty;
    std::set<std::string> seen;
    double last_score=0; bool first=true;
    for(const auto& c:f.selected) {
        if(!universe.count(c.symbol) || !seen.insert(c.symbol).second)
            throw std::runtime_error("Unknown/duplicate selected stock");
        if(!first && c.ranking_score>last_score)
            throw std::runtime_error("Ranking not in descending order");
        first=false;last_score=c.ranking_score;
        const auto& o=c.observations;
        const Key k5=o.five_minute.completed_key, k15=o.fifteen_minute.completed_key;
        if(o.one_minute.completed_key!=key || k5<=0 || k15<=0 || k5>key || k15>key ||
           key-k5>=5 || key-k15>=15 ||
           (k5-1)/1440!=(key-1)/1440 || (k15-1)/1440!=(key-1)/1440 ||
           !o.close_1m.ready || !o.close_5m.ready || !o.close_15m.ready)
            throw std::runtime_error("Look-ahead, stale or unready selected observation");
        if(k5<key) ++p.carried5;
        if(k15<key) ++p.carried15;
    }
}
void print(const char* label,const Partition& p) {
    std::cout<<label<<" decisions="<<p.decisions<<" evaluated="<<p.evaluated
             <<" rejected="<<p.rejected<<" top5_assignments="<<p.selected
             <<" consecutive_1m_pairs="<<p.consecutive<<" non_5m_boundaries="<<p.non5
             <<" carried_5m="<<p.carried5<<" carried_15m="<<p.carried15
             <<" no_candidate_frames="<<p.empty<<'\n';
}
} // namespace
int main(int argc,char** argv) {
    try {
        // OOS_START_DATE BENCHMARK_FILE SYMBOL FILE x10
        if(argc!=23) {
            std::cerr<<"Usage: "<<argv[0]
                     <<" OOS_START_YYYY-MM-DD NIFTY50_FILE STOCK1 FILE1 ... STOCK10 FILE10\n"
                     <<"Exactly 10 unique cash-equity stocks; NIFTY%2050 is benchmark.\n";
            return 2;
        }
        const std::string oos_date=argv[1];
        const Key oos_day=parseOOSDay(oos_date);
        const std::string benchmark="NIFTY%2050";
        std::vector<DevHistoricalMinuteInput> inputs{{benchmark,argv[2]}};
        std::set<std::string> universe;
        for(int i=3;i<argc;i+=2) {
            const std::string symbol=argv[i];
            if(symbol.empty() || symbol==benchmark || !universe.insert(symbol).second)
                throw std::invalid_argument("Ten distinct stocks required; benchmark cannot be ranked");
            inputs.push_back({symbol,argv[i+1]});
        }
        if(universe.size()!=10 || inputs.size()!=11) throw std::runtime_error("Wrong stock count");
        constexpr std::size_t top_k=5;
        DevHistoricalFeedConfig feed_config;
        feed_config.manager_id="DevManager_001";
        feed_config.benchmark_symbol=benchmark;
        feed_config.sessions.session_overrides["2025-10-21"]={13*60+45,14*60+45};
        DevRealHistoricalMultiStockFeed feed(feed_config);
        DevD7StrategyConfig strategy_config;
        strategy_config.manager_id=feed_config.manager_id;
        strategy_config.min_expected_return=0.0;
        // Fixed diagnostic policy: NOT fitted on pre-OOS data, NOT a predictive model.
        DevManager001StatisticalStrategyD7 strategy(strategy_config,diagnosticPastOnlyForecast);
        DevManager001HistoricalStrategyD7B manager({feed_config.manager_id,top_k,1},strategy);
        Partition development, holdout;
        Key global_last=0; std::size_t callbacks=0;
        const auto report=manager.replay(feed,inputs,[&](const DevManager001RankingFrame& f) {
            if(global_last && f.decision_key<=global_last) throw std::runtime_error("Non-increasing replay keys");
            global_last=f.decision_key; ++callbacks;
            // Exchange-local completion key; boundary at 00:00 belongs to previous session.
            const Key completed_day=(f.decision_key-1)/1440;
            inspect(completed_day<oos_day?development:holdout,f,universe,top_k);
        });
        std::cout<<"manager=DevManager_001 stocks=10 top_k=5 cadence_minutes=1"
                 <<" oos_start="<<oos_date<<" benchmark="<<benchmark<<'\n';
        for(const auto& s:report.feed.validation.symbols)
            std::cout<<s.symbol<<" rows="<<s.rows
                     <<" missing_session_minutes="<<s.missing_session_minutes<<'\n';
        print("DEVELOPMENT",development); print("OOS_HOLDOUT",holdout);
        std::cout<<"D4_pass="<<(report.feed.validation.passed?"yes":"no")
                 <<" D5_eligible="<<report.feed.eligible_decisions
                 <<" skipped_incomplete="<<report.feed.skipped_incomplete
                 <<" skipped_warmup="<<report.feed.skipped_warmup
                 <<" observed="<<report.observed_frames
                 <<" decision_frames="<<report.decision_frames
                 <<" callbacks="<<callbacks<<'\n';
        if(!report.feed.validation.passed || development.decisions==0 || holdout.decisions==0 ||
           holdout.consecutive==0 || holdout.non5==0 ||
           report.decision_frames!=callbacks || report.observed_frames!=callbacks ||
           report.feed.eligible_decisions!=callbacks ||
           report.evaluated!=callbacks*10 ||
           development.evaluated!=development.decisions*10 ||
           holdout.evaluated!=holdout.decisions*10 ||
           development.selected+holdout.selected!=report.selected ||
           development.rejected+holdout.rejected!=report.rejected)
            throw std::runtime_error("D7D acceptance criteria not met");
        std::cout<<"PASS 5C-3I-D7D ten-stock one-minute chronological holdout replay"
                 <<" (FIXED DIAGNOSTIC POLICY; NOT TRAINED OOS PERFORMANCE OR TRADING)\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL 5C-3I-D7D: "<<e.what()<<'\n';
        return 1;
    }
}
