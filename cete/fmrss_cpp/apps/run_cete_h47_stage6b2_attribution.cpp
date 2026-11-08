// CETE H4.7 Stage 5E.2K.6B.2: READ-ONLY signal/outcome and ledger attribution.
// Inputs: Stage 6B enriched pipeline CSV, Stage 6B mathematical-only ledger CSV.
// Future returns are used only for offline attribution; NEVER for decision making.
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
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace {
using Row = std::map<std::string,std::string>;
std::vector<std::string> parseCSV(const std::string& s) {
    std::vector<std::string> v; std::string cell; bool quoted=false;
    for (std::size_t i=0;i<s.size();++i) {
        const char c=s[i];
        if (c=='"') { if (quoted && i+1<s.size() && s[i+1]=='"') {cell+='"';++i;} else quoted=!quoted; }
        else if(c==',' && !quoted) {v.push_back(cell);cell.clear();}
        else cell+=c;
    }
    if(quoted) throw std::runtime_error("Unclosed CSV quote");
    v.push_back(cell); return v;
}
std::vector<Row> readCSV(const fs::path& p, const std::vector<std::string>& required) {
    std::ifstream f(p); if(!f) throw std::runtime_error("Cannot open "+p.string());
    std::string line; if(!std::getline(f,line)) throw std::runtime_error("Empty CSV: "+p.string());
    auto hdr=parseCSV(line); if(!hdr.empty() && !hdr[0].empty() && hdr[0][0]=='\xef') hdr[0].erase(0,3);
    std::set<std::string> hs(hdr.begin(),hdr.end());
    for(const auto& c:required) if(!hs.count(c)) throw std::runtime_error("Missing CSV column: "+c+" in "+p.string());
    std::vector<Row> out; std::size_t lineNo=1;
    while(std::getline(f,line)) {++lineNo; if(line.empty()) continue; auto cells=parseCSV(line);
        if(cells.size()!=hdr.size()) throw std::runtime_error("CSV width mismatch line "+std::to_string(lineNo));
        Row r; for(std::size_t j=0;j<hdr.size();++j) r.emplace(hdr[j],cells[j]); out.push_back(std::move(r));
    }
    return out;
}
const std::string& get(const Row& r,const std::string& k) {return r.at(k);}
double number(const Row& r,const std::string& k) {
    const auto& s=get(r,k); if(s.empty()) throw std::runtime_error("Missing numeric value "+k);
    std::size_t n=0; double v=std::stod(s,&n); if(n!=s.size() || !std::isfinite(v)) throw std::runtime_error("Bad numeric "+k); return v;
}
std::int64_t epoch(const Row& r) {const double d=number(r,"decision_time"); if(d<=0 || std::floor(d)!=d) throw std::runtime_error("Invalid decision epoch");return static_cast<std::int64_t>(d);}
int flag(const Row& r,const std::string& k) {const double v=number(r,k);if(v!=0 && v!=1) throw std::runtime_error("Invalid flag "+k);return static_cast<int>(v);}
struct Agg {std::size_t n=0,positive=0,negative=0,zero=0;double sum=0,sumSq=0;
    void add(double x){++n;sum+=x;sumSq+=x*x;if(x>0)++positive;else if(x<0)++negative;else ++zero;}
};
struct Signal {std::int64_t t=0;std::string symbol;int rank=0;bool eligible=false;};
struct Trade {std::int64_t start=0,end=0;std::string symbol;double cost=0;double gross=0;double net=0;std::size_t legs=0;};
void require(bool v,const std::string& msg){if(!v) throw std::runtime_error(msg);}
void writeAgg(std::ofstream& f,const std::string& group,int horizon,const Agg& a){
    const double mean=a.n?a.sum/static_cast<double>(a.n):0;
    const double variance=a.n?std::max(0.,a.sumSq/static_cast<double>(a.n)-mean*mean):0;
    f<<group<<','<<horizon<<','<<a.n<<','<<a.positive<<','<<a.negative<<','<<a.zero<<','
     <<mean<<','<<std::sqrt(variance)<<','<<(a.n?static_cast<double>(a.positive)/a.n:0)<<'\n';
}
}
int main(int argc,char** argv){
 try {
    if(argc!=4) {std::cerr<<"Usage: "<<argv[0]<<" <enriched_pipeline_audit.csv> <math_ledger.csv> <output_directory>\n";return 2;}
    const fs::path dir(argv[3]);fs::create_directories(dir);
    const auto signals=readCSV(argv[1],{"decision_time","symbol","rank","return_z_percentile_1m","return_z_percentile_5m","return_z_percentile_15m","available_1m","available_5m","available_15m","observation_ready","observation_integrity","outcome_horizon_bars","offline_forward_return","offline_outcome_available"});
    const auto ledger=readCSV(argv[2],{"decision_time","previous_symbol","desired_symbol","held_symbol","quantity","equity_before","equity_after","gross_pnl","brokerage","slippage","other_costs","net_pnl","executed_legs"});
    require(!signals.empty() && !ledger.empty(),"Empty input");
    // A candidate can have multiple horizon rows. Deduplicate signal counts by (time,symbol).
    std::map<std::pair<std::int64_t,std::string>,Signal> signalByKey;
    std::map<std::pair<std::string,int>,Agg> outcomeAgg;
    std::map<std::pair<std::int64_t,std::string>,std::map<int,double>> usableReturns;
    std::set<int> horizons;
    for(const auto& r:signals) {
        const auto t=epoch(r);const auto symbol=get(r,"symbol");require(!symbol.empty(),"Empty signal symbol");
        const auto rank=static_cast<int>(number(r,"rank"));
        bool eligible=flag(r,"observation_ready") && flag(r,"observation_integrity");
        for(const auto& tf:{"1m","5m","15m"}) {
            const std::string suffix(tf);
            eligible=eligible && flag(r,"available_"+suffix) && number(r,"return_z_percentile_"+suffix)>0;
        }
        const auto key=std::make_pair(t,symbol);
        auto [it,inserted]=signalByKey.emplace(key,Signal{t,symbol,rank,eligible});
        if(!inserted) require(it->second.rank==rank && it->second.eligible==eligible,"Inconsistent repeated candidate signal");
        const int h=static_cast<int>(number(r,"outcome_horizon_bars"));
        if(h<=0 || !flag(r,"offline_outcome_available")) continue;
        const double ret=number(r,"offline_forward_return");horizons.insert(h);
        auto& returns=usableReturns[key];require(returns.emplace(h,ret).second,"Duplicate usable outcome for candidate/horizon");
        outcomeAgg[{"all_top5",h}].add(ret);
        outcomeAgg[{eligible?"eligible_signal":"ineligible_signal",h}].add(ret);
        outcomeAgg[{"rank_"+std::to_string(rank),h}].add(ret);
    }
    std::ofstream sigout(dir/"signal_forward_attribution.csv");require(bool(sigout),"Cannot write signal report");
    sigout<<"group,horizon_bars,count,positive,negative,zero,mean_forward_return,stddev_forward_return,positive_fraction\n"<<std::setprecision(15);
    for(const auto& [key,a]:outcomeAgg) writeAgg(sigout,key.first,key.second,a);
    // Identify actual entries by changes in held symbol, rather than desired policy signals.
    std::ofstream entryout(dir/"executed_entry_forward_returns.csv");require(bool(entryout),"Cannot write entry report");
    entryout<<"decision_time,symbol,signal_eligible,rank,horizon_bars,outcome_available,forward_return\n"<<std::setprecision(15);
    std::ofstream tradesout(dir/"trade_durations.csv");require(bool(tradesout),"Cannot write trades report");
    tradesout<<"entry_time,exit_time,symbol,holding_minutes,attributed_gross_pnl,attributed_costs,attributed_net_pnl,executed_legs,complete\n"<<std::setprecision(15);
    std::map<int,Agg> entryReturnAgg;std::map<std::string,Agg> holdingAgg;
    std::size_t entries=0,completed=0,unmatched=0,changes=0;
    std::string prevHeld;Trade active;bool hasActive=false;
    std::int64_t previousTime=0;double totalGross=0,totalCosts=0,totalNet=0;
    for(const auto& r:ledger) {
        const auto t=epoch(r);require(t>previousTime,"Ledger must be strictly chronological");previousTime=t;
        const auto held=get(r,"held_symbol");const auto legs=static_cast<std::size_t>(number(r,"executed_legs"));
        const double gross=number(r,"gross_pnl");const double costs=number(r,"brokerage")+number(r,"slippage")+number(r,"other_costs");
        const double net=number(r,"net_pnl");totalGross+=gross;totalCosts+=costs;totalNet+=net;
        if(held!=prevHeld) {
            ++changes;
            if(hasActive) {
                active.end=t; active.gross+=gross;active.cost+=costs;active.net+=net;active.legs+=legs;
                const double mins=static_cast<double>(t-active.start)/60.;holdingAgg["complete"].add(mins);
                tradesout<<active.start<<','<<active.end<<','<<active.symbol<<','<<mins<<','<<active.gross<<','<<active.cost<<','<<active.net<<','<<active.legs<<",1\n";
                ++completed;hasActive=false;
                // A switch would need split-leg attribution; stop instead of misreporting.
                require(held.empty(),"Direct rotation detected: cannot allocate switch costs to individual trades");
            }
            if(!held.empty()) {
                active=Trade{};active.start=t;active.symbol=held;hasActive=true;++entries;
                const auto it=signalByKey.find({t,held});
                const bool found=it!=signalByKey.end();if(!found) ++unmatched;
                for(const auto h:horizons) {
                    const auto foundReturns=usableReturns.find({t,held});
                    const auto ret=foundReturns==usableReturns.end()?std::map<int,double>::const_iterator{}:foundReturns->second.find(h);
                    const bool available=foundReturns!=usableReturns.end() && ret!=foundReturns->second.end();
                    entryout<<t<<','<<held<<','<<(found && it->second.eligible?1:0)<<','<<(found?it->second.rank:0)<<','<<h<<','<<(available?1:0)<<','<<(available?ret->second:0)<<'\n';
                    if(available) entryReturnAgg[h].add(ret->second);
                }
            }
        }
        if(hasActive) {active.gross+=gross;active.cost+=costs;active.net+=net;active.legs+=legs;}
        prevHeld=held;
    }
    if(hasActive) {
        tradesout<<active.start<<",0,"<<active.symbol<<",0,"<<active.gross<<','<<active.cost<<','<<active.net<<','<<active.legs<<",0\n";
    }
    std::ofstream summary(dir/"attribution_summary.csv");require(bool(summary),"Cannot write summary");
    summary<<"metric,value\n"<<std::setprecision(15);
    const auto metric=[&](const std::string& name,auto value){summary<<name<<','<<value<<'\n';};
    std::size_t eligible=0;for(const auto& [key,s]:signalByKey){(void)key;if(s.eligible)++eligible;}
    metric("unique_candidate_signals",signalByKey.size());metric("eligible_candidate_signals",eligible);
    metric("executed_entries",entries);metric("completed_trades",completed);
    metric("unmatched_entry_signal_timestamps",unmatched);metric("held_symbol_changes",changes);
    metric("total_gross_pnl",totalGross);metric("total_costs",totalCosts);metric("total_net_pnl",totalNet);
    if(holdingAgg["complete"].n) metric("mean_holding_minutes",holdingAgg["complete"].sum/holdingAgg["complete"].n);
    for(const auto& [h,a]:entryReturnAgg) {metric("entry_forward_count_h"+std::to_string(h),a.n);metric("entry_mean_forward_return_h"+std::to_string(h),a.n?a.sum/a.n:0);}
    require(std::abs(totalGross-totalCosts-totalNet)<0.05,"Ledger P&L decomposition mismatch");
    std::cout<<"[CETE][H47][5E2K6B2] READ-ONLY ATTRIBUTION COMPLETE\n"
             <<"Candidate signals: "<<signalByKey.size()<<" eligible: "<<eligible<<"\n"
             <<"Executed entries: "<<entries<<" completed trades: "<<completed<<" unmatched entry timestamps: "<<unmatched<<"\n"
             <<"Gross: "<<totalGross<<" Costs: "<<totalCosts<<" Net: "<<totalNet<<"\n"
             <<"Reports: "<<dir<<"\n";
    return hasActive?3:0;
 } catch(const std::exception& e){std::cerr<<"[CETE][H47][5E2K6B2] FAIL: "<<e.what()<<'\n';return 1;}
}
