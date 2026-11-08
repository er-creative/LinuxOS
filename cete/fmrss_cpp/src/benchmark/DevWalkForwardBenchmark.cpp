#include "devai/benchmark/DevWalkForwardBenchmark.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace devai::benchmark {
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void finite(double x){require(std::isfinite(x),"H4: non-finite value");}
std::vector<std::string> split(const std::string& line){
    std::stringstream s(line);std::vector<std::string> v;std::string field;
    while(std::getline(s,field,','))v.push_back(field);
    return v;
}
int day(std::int64_t ts){return static_cast<int>(ts/10000);}
int clock(std::int64_t ts){return static_cast<int>(ts%10000);}
bool isManager(const std::string& m){return m=="LONG"||m=="SHORT";}
void checkRow(const WFDecision& r){
    require(!r.candidate.empty()&&!r.symbol.empty()&&isManager(r.manager),"H4: invalid identity");
    require(r.timestamp>=200001010000LL&&r.timestamp<=209912312359LL,"H4: invalid timestamp range");
    require(clock(r.timestamp)>=915&&clock(r.timestamp)<=1530,"H4: outside market session");
    finite(r.price);finite(r.score);require(r.price>0,"H4: nonpositive price");
    require(r.action>=0&&r.action<=3,"H4: invalid action");
    require(r.legal,"H4: illegal action in OOS ledger");
}
struct Position {bool open=false;std::string symbol;double units=0,entry_cost=0,entry_equity=0;};
}
std::vector<WFDecision> readWalkForwardCSV(const std::filesystem::path& path){
    std::ifstream in(path);require(bool(in),"H4: cannot open input CSV");
    std::string line;require(bool(std::getline(in,line)),"H4: empty CSV");
    require(line=="candidate,manager,timestamp,symbol,price,score,action,legal", "H4: unexpected CSV header");
    std::vector<WFDecision> rows;std::size_t line_no=1;
    while(std::getline(in,line)){
        ++line_no;if(line.empty())continue;
        auto x=split(line);require(x.size()==8,"H4: expected eight CSV fields");
        WFDecision r;
        try{
            r.candidate=x[0];r.manager=x[1];r.timestamp=std::stoll(x[2]);r.symbol=x[3];
            r.price=std::stod(x[4]);r.score=std::stod(x[5]);r.action=std::stoi(x[6]);
            require(x[7]=="0"||x[7]=="1","H4: legal must be 0 or 1");r.legal=x[7]=="1";
        }catch(const std::invalid_argument&){throw std::runtime_error("H4: invalid numeric CSV field");}
         catch(const std::out_of_range&){throw std::runtime_error("H4: numeric CSV field out of range");}
        checkRow(r);rows.push_back(std::move(r));
    }
    require(!rows.empty(),"H4: no observations");return rows;
}
std::vector<WFReport> evaluateWalkForward(const std::vector<WFDecision>& rows,const WFConfig& c){
    finite(c.initial_capital);finite(c.allocation_fraction);finite(c.fee_per_order);finite(c.slippage_bps);
    require(c.initial_capital>0&&c.allocation_fraction>0&&c.allocation_fraction<=1&&
        c.fee_per_order>=0&&c.slippage_bps>=0&&c.slippage_bps<10000&&c.min_test_days>=1,
        "H4: invalid configuration");
    using Key=std::pair<std::string,std::string>;
    std::map<Key,std::vector<WFDecision>> grouped;
    for(const auto& r:rows){checkRow(r);grouped[{r.candidate,r.manager}].push_back(r);}
    std::vector<WFReport> reports;
    for(auto& [key,series]:grouped){
        // Fail closed on duplicates or shuffled events. One action per manager per minute.
        for(std::size_t i=1;i<series.size();++i)
            require(series[i].timestamp>series[i-1].timestamp,"H4: nonchronological or duplicate decisions");
        std::set<int> days;for(const auto& r:series)days.insert(day(r.timestamp));
        require(days.size()>=c.min_test_days,"H4: insufficient OOS days");
        double cash=c.initial_capital,peak=c.initial_capital,maxdd=0,gains=0,losses=0;
        std::size_t trades=0,wins=0,rejected=0;
        Position pos{};double last_price=0;
        const bool is_short=key.second=="SHORT";
        const auto equity=[&](double price){
            if(!pos.open)return cash;
            // cash is equity immediately before entry less entry fee; mark-to-market profit is relative to entry.
            return cash+(is_short?(pos.entry_cost-price):(price-pos.entry_cost))*pos.units;
        };
        const auto close=[&](double price){
            require(pos.open,"H4: close without position");
            const double exit_price=price*(is_short?(1+c.slippage_bps/10000.0):(1-c.slippage_bps/10000.0));
            const double pnl=(is_short?(pos.entry_cost-exit_price):(exit_price-pos.entry_cost))*pos.units;
            const double trade_net=pnl-2*c.fee_per_order;
            cash+=pnl-c.fee_per_order;
            ++trades;if(trade_net>0){++wins;gains+=trade_net;}else losses+=-trade_net;
            pos=Position{};
        };
        int previous_day=0;
        for(const auto& r:series){
            // Input must contain executable, already validated OOS decisions.
            if(previous_day!=0 && day(r.timestamp)!=previous_day)
                require(!pos.open,"H4: overnight position; missing same-day exit");
            previous_day=day(r.timestamp);
            if(pos.open && r.symbol!=pos.symbol)
                throw std::runtime_error("H4: held symbol price missing from ledger");
            // Enforce flat by 15:15. No entries after 15:15.
            if(pos.open && clock(r.timestamp)>=1515)close(r.price);
            if(r.action==1){
                if(pos.open||clock(r.timestamp)>=1515){++rejected;}
                else{
                    const double fill=r.price*(is_short?(1-c.slippage_bps/10000.0):(1+c.slippage_bps/10000.0));
                    const double budget=cash*c.allocation_fraction;
                    if(budget<=c.fee_per_order){++rejected;}
                    else{
                        const double units=std::floor((budget-c.fee_per_order)/fill);
                        if(units<1){++rejected;}
                        else{pos={true,r.symbol,units,fill,cash};cash-=c.fee_per_order;}
                    }
                }
            }else if(r.action==3){
                if(!pos.open||pos.symbol!=r.symbol){++rejected;}
                else close(r.price);
            }else if(pos.open && r.symbol!=pos.symbol){
                // A one-symbol-per-minute ledger cannot value a different held symbol.
                throw std::runtime_error("H4: held symbol price missing from ledger");
            }
            last_price=r.price;
            const double e=equity(last_price);finite(e);
            peak=std::max(peak,e);
            maxdd=std::max(maxdd,100.0*(peak-e)/peak);
        }
        // No synthetic exit at an unknown price: require explicit flat state by the end.
        require(!pos.open,"H4: position remains open at end of OOS ledger");
        WFReport report{};
        report.candidate=key.first;report.manager=key.second;
        report.rows=series.size();report.trades=trades;report.rejected=rejected;
        report.test_days=days.size();report.starting_equity=c.initial_capital;
        report.ending_equity=cash;report.net_pnl=cash-c.initial_capital;
        report.max_drawdown_pct=maxdd;report.win_rate_pct=trades?100.0*wins/trades:0;
        report.profit_factor=losses>0?gains/losses:(gains>0?std::numeric_limits<double>::infinity():0);
        reports.push_back(report);
    }
    return reports;
}
void writeWalkForwardCSV(const std::filesystem::path& path,const std::vector<WFReport>& reports){
    std::ofstream out(path);require(bool(out),"H4: cannot write report");
    out<<"candidate,manager,rows,test_days,trades,rejected,starting_equity,ending_equity,net_pnl,max_drawdown_pct,win_rate_pct,profit_factor\n";
    out<<std::setprecision(12);
    for(const auto& r:reports){
        out<<r.candidate<<','<<r.manager<<','<<r.rows<<','<<r.test_days<<','<<r.trades<<','<<r.rejected<<','
           <<r.starting_equity<<','<<r.ending_equity<<','<<r.net_pnl<<','<<r.max_drawdown_pct<<','
           <<r.win_rate_pct<<','<<r.profit_factor<<'\n';
    }
    require(bool(out),"H4: report write failed");
}
}
