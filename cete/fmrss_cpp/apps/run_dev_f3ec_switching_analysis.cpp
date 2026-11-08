#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Cells = std::map<std::string, std::string>;
struct Csv { std::vector<Cells> rows; };
static void demand(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> v; std::string cell;
    for (char c : line) { if (c == ',') { v.push_back(cell); cell.clear(); } else cell += c; }
    v.push_back(cell); return v;
}
static Csv load(const fs::path& file, const std::vector<std::string>& required) {
    std::ifstream in(file); demand(bool(in), "cannot open " + file.string());
    std::string line; demand(bool(std::getline(in,line)), "empty CSV: " + file.string());
    const auto columns=split(line); std::set<std::string> unique(columns.begin(),columns.end());
    demand(unique.size()==columns.size(),"duplicate CSV columns");
    for (const auto& c:required) demand(unique.count(c)!=0,"missing column: "+c);
    Csv result; std::size_t rowno=1;
    while(std::getline(in,line)) {
        ++rowno; if(line.empty()) continue;
        const auto fields=split(line); demand(fields.size()==columns.size(),"column count mismatch at row "+std::to_string(rowno));
        Cells row; for(std::size_t i=0;i<columns.size();++i) row.emplace(columns[i],fields[i]);
        result.rows.push_back(std::move(row));
    }
    return result;
}
static const std::string& get(const Cells& r, const std::string& k) { return r.at(k); }
static double number(const std::string& s, const std::string& field) {
    demand(!s.empty(),"missing numeric field: "+field);
    std::size_t used=0; double x=std::stod(s,&used);
    demand(used==s.size()&&std::isfinite(x),"invalid numeric field: "+field);
    return x;
}
static std::int64_t integer(const std::string& s,const std::string& field) {
    demand(!s.empty(),"missing integer: "+field);
    std::size_t used=0; auto n=std::stoll(s,&used);
    demand(used==s.size(),"invalid integer: "+field); return n;
}
static bool close(double a,double b) { return std::abs(a-b)<=1e-6*std::max({1.0,std::abs(a),std::abs(b)}); }
static double quantile(std::vector<double> v,double p) {
    if(v.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(),v.end()); double pos=(v.size()-1)*p;
    auto i=static_cast<std::size_t>(pos); double f=pos-i;
    return v[i]*(1-f)+v[std::min(i+1,v.size()-1)]*f;
}
int main(int argc,char** argv) {
 try {
    demand(argc==5,"Usage: run_dev_f3ec_switching_analysis <manager_q_diagnostics.csv> <full_session_audit.csv> <switching_analysis.csv> <switching_summary.txt>");
    auto q=load(argv[1],{"decision_time","held_before","desired","held_after","raw_action","final_action","transition","override","keep_action","keep_q","best_switch_action","best_switch_symbol","best_switch_q","switch_minus_keep_q","legal_switch_count","executed_legs","supervisor_recommendation"});
    auto e=load(argv[2],{"decision_time","next_decision_time","manager_action","desired_symbol","held_after","equity_before","equity_after_mark","gross_reward","net_reward","selected_reward","execution_costs","terminal"});
    demand(q.rows.size()==360&&e.rows.size()==360,"expected 360 rows in each CSV");
    fs::path detail=argv[3],summary=argv[4];
    if(!detail.parent_path().empty()) fs::create_directories(detail.parent_path());
    if(!summary.parent_path().empty()) fs::create_directories(summary.parent_path());
    std::ofstream out(detail); demand(bool(out),"cannot create detail CSV");
    out<<"decision_time,transition,held_before,desired,held_after,holding_age_minutes,selected_q_margin,best_switch_q_margin,execution_costs,net_pnl,rapid_return_2step\n";
    out<<std::setprecision(17);
    std::map<std::string,int> selected, pair_count;
    std::vector<double> selected_margins, best_margins, switch_costs;
    std::size_t switches=0, small001=0,small005=0,small010=0,rapid=0,one_min=0;
    std::size_t open=0,exit=0,hold=0,wait=0, overrides=0;
    double total_cost=0,total_net=0,switch_cost=0;
    std::int64_t prev_time=0,entry_time=0;
    std::string prior_held,prev_prev_held;
    for(std::size_t i=0;i<q.rows.size();++i) {
        const auto& a=q.rows[i]; const auto& b=e.rows[i];
        auto t=integer(get(a,"decision_time"),"decision_time");
        demand(t==integer(get(b,"decision_time"),"F3C decision_time"),"CSV timestamps do not align");
        demand(i==0||t==prev_time+60,"nonconsecutive timestamps");
        demand(integer(get(b,"next_decision_time"),"next_decision_time")==t+60,"invalid next time");
        demand(get(a,"desired")==get(b,"desired_symbol")&&get(a,"held_after")==get(b,"held_after"),"desired/held mismatch between files");
        demand(integer(get(a,"final_action"),"final_action")==integer(get(b,"manager_action"),"manager_action"),"action mismatch between files");
        demand(i==0||close(number(get(b,"equity_before"),"equity_before"),number(get(e.rows[i-1],"equity_after_mark"),"prior equity")),"equity continuity mismatch");
        const std::string before=get(a,"held_before"),after=get(a,"held_after");
        demand(before==prior_held,"held_before does not match previous holding");
        const double cost=number(get(b,"execution_costs"),"execution_costs");
        const double net=number(get(b,"equity_after_mark"),"equity_after_mark")-number(get(b,"equity_before"),"equity_before");
        demand(cost>=0,"negative costs"); total_cost+=cost; total_net+=net;
        const auto override_flag=integer(get(a,"override"),"override"); demand(override_flag==0||override_flag==1,"invalid override flag"); overrides+=static_cast<std::size_t>(override_flag);
        const std::string transition=get(a,"transition");
        std::string computed;
        if(before.empty()&&after.empty()) { computed="WAIT"; ++wait; }
        else if(before.empty()) { computed="OPEN"; ++open; }
        else if(after.empty()) { computed="EXIT"; ++exit; }
        else if(before==after) { computed="HOLD"; ++hold; }
        else { computed="SWITCH"; ++switches; switch_cost+=cost; switch_costs.push_back(cost); selected[after]++; pair_count[before+" -> "+after]++; }
        demand(transition==computed,"transition classification mismatch at "+std::to_string(t));
        int age=before.empty()?0:static_cast<int>((t-entry_time)/60);
        if(computed=="SWITCH" && age<=1) ++one_min;
        bool rapid_return=(computed=="SWITCH" && i>=2 && after==prev_prev_held && !after.empty());
        if(rapid_return) ++rapid;
        double selected_margin=std::numeric_limits<double>::quiet_NaN();
        double best_margin=std::numeric_limits<double>::quiet_NaN();
        if(!get(a,"switch_minus_keep_q").empty()) {
            best_margin=number(get(a,"switch_minus_keep_q"),"switch_minus_keep_q");
            best_margins.push_back(best_margin);
        }
        if(computed=="SWITCH") {
            demand(!get(a,"keep_q").empty(),"SWITCH without keep Q");
            // The selected candidate's Q-value is not present in this F3E-B CSV.
            // Best SWITCH margin is an upper bound on selected SWITCH margin unless
            // selected action is the best-switch action.
            const auto final_action=integer(get(a,"final_action"),"final_action");
            const auto best_action=integer(get(a,"best_switch_action"),"best_switch_action");
            if(final_action==best_action && !get(a,"best_switch_q").empty()) {
                selected_margin=number(get(a,"best_switch_q"),"best_switch_q")-number(get(a,"keep_q"),"keep_q");
                selected_margins.push_back(selected_margin);
                if(selected_margin<=.001) ++small001;
                if(selected_margin<=.005) ++small005;
                if(selected_margin<=.010) ++small010;
            }
        }
        out<<t<<','<<computed<<','<<before<<','<<get(a,"desired")<<','<<after<<','<<age<<',';
        if(std::isfinite(selected_margin)) out<<selected_margin;
        out<<','; if(std::isfinite(best_margin)) out<<best_margin;
        out<<','<<cost<<','<<net<<','<<(rapid_return?1:0)<<'\n';
        if(after.empty()) entry_time=0;
        else if(before!=after) entry_time=t;
        prev_prev_held=before; prior_held=after; prev_time=t;
    }
    demand(wait+open+exit+hold+switches==360,"transition count mismatch");
    demand(close(total_net,number(get(e.rows.back(),"equity_after_mark"),"final equity")-number(get(e.rows.front(),"equity_before"),"initial equity")),"net reconciliation mismatch");
    demand(switches==281,"baseline switch count changed");
    demand(close(total_cost,2295.6476),"baseline total costs changed");
    demand(close(switch_cost,2286.7599),"baseline switch costs changed");
    demand(integer(get(e.rows.back(),"terminal"),"terminal")==1&&prior_held.empty(),"terminal position not flat");
    std::ostringstream report;
    report<<std::fixed<<std::setprecision(6);
    report<<"D9F.4C-F3E-C — SWITCHING DECISION ANALYSIS\n";
    report<<"Decisions: 360 | Switches: "<<switches<<" | Holds: "<<hold<<" | Waits: "<<wait<<"\n";
    report<<"Total execution costs: "<<total_cost<<" | Switching costs: "<<switch_cost<<"\n";
    report<<"Net P&L: "<<total_net<<" | Switches after <=1 held minute: "<<one_min<<"\n";
    report<<"Two-step return-to-previous-stock patterns: "<<rapid<<"\n";
    report<<"Raw/final overrides: "<<overrides<<"\n";
    report<<"Best-SWITCH vs KEEP margins available: "<<best_margins.size()<<"\n";
    report<<"Selected-SWITCH margin verified: "<<selected_margins.size()<<" of "<<switches<<" switches\n";
    if(!selected_margins.empty()) {
        auto sum=std::accumulate(selected_margins.begin(),selected_margins.end(),0.0);
        report<<"Selected margin min/mean/p10/median/p90/max: "
              <<quantile(selected_margins,0)<<" / "<<sum/selected_margins.size()<<" / "
              <<quantile(selected_margins,.1)<<" / "<<quantile(selected_margins,.5)<<" / "
              <<quantile(selected_margins,.9)<<" / "<<quantile(selected_margins,1)<<"\n";
        report<<"Verified selected margins <= .001/.005/.010: "<<small001<<"/"<<small005<<"/"<<small010<<"\n";
    }
    std::vector<std::pair<std::string,int>> pairs(pair_count.begin(),pair_count.end());
    std::sort(pairs.begin(),pairs.end(),[](const auto& x,const auto& y){return x.second>y.second || (x.second==y.second && x.first<y.first);});
    report<<"Top switch pairs:\n";
    for(std::size_t i=0;i<std::min<std::size_t>(10,pairs.size());++i) report<<"  "<<pairs[i].first<<": "<<pairs[i].second<<"\n";
    report<<"F3E-C SWITCHING ANALYSIS: PASS\n";
    report<<"NOTE: Q margins are not rupees; this is baseline analysis, not counterfactual evidence.\n";
    std::ofstream txt(summary); demand(bool(txt),"cannot write summary"); txt<<report.str();
    demand(bool(out)&&bool(txt),"output write failed");
    std::cout<<report.str(); std::cout<<"Detail: "<<detail<<"\nSummary: "<<summary<<'\n';
    return 0;
 } catch(const std::exception& ex) { std::cerr<<"F3E-C SWITCHING ANALYSIS: FAILED: "<<ex.what()<<'\n'; return 1; }
}
