#include "devai/strategy/dev_ai_strategy/diagnostics/DevRankingDecisionValidator_D9F4CE.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
namespace devai::strategy::d9f4ce {
namespace {
std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::string field;
    bool quoted=false;
    for (std::size_t i=0;i<s.size();++i) {
        char c=s[i];
        if (c=='"') { if (quoted && i+1<s.size() && s[i+1]=='"') {field+='"';++i;} else quoted=!quoted; }
        else if (c==',' && !quoted) {out.push_back(field);field.clear();}
        else field+=c;
    }
    if (quoted) throw std::runtime_error("Unterminated CSV quote");
    out.push_back(field);
    return out;
}
int minuteIST(long long t) {
    long long m=((t+19800)%86400+86400)%86400;
    return static_cast<int>(m/60);
}
void warn(Summary& s,const std::string& msg) {s.warnings.push_back(msg);}
}
std::vector<Row> readCsv(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open audit CSV: "+path.string());
    std::string line;
    if (!std::getline(in,line)) throw std::runtime_error("Empty audit CSV");
    auto headings=splitCsv(line);
    std::map<std::string,std::size_t> columns;
    for(std::size_t i=0;i<headings.size();++i) columns[headings[i]]=i;
    auto get=[&](const std::vector<std::string>& fields,const std::string& name)->std::string {
        auto it=columns.find(name);
        if(it==columns.end() || it->second>=fields.size()) throw std::runtime_error("Missing column/value: "+name);
        return fields[it->second];
    };
    std::vector<Row> rows;
    while(std::getline(in,line)) {
        if(line.empty()) continue;
        auto f=splitCsv(line);
        Row r;
        r.time=std::stoll(get(f,"decision_time"));
        r.raw=std::stoi(get(f,"raw_action"));
        r.final=std::stoi(get(f,"final_action"));
        r.age=std::stoi(get(f,"holding_age_minutes"));
        r.legs=std::stoi(get(f,"executed_legs"));
        r.held_before=get(f,"held_before");r.desired=get(f,"desired");r.held_after=get(f,"held_after");
        r.priority=get(f,"supervisor_priority");r.recommendation=get(f,"supervisor_recommendation");
        for(int i=0;i<7;++i) {
            r.legal[i]=std::stoi(get(f,"legal_"+std::to_string(i)))!=0;
            r.q[i]=std::stod(get(f,"q_"+std::to_string(i)));
            if(!std::isfinite(r.q[i])) throw std::runtime_error("Nonfinite manager Q-value");
        }
        for(int i=0;i<5;++i) {
            r.symbols[i]=get(f,"slot_"+std::to_string(i)+"_symbol");
            r.scores[i]=std::stod(get(f,"slot_"+std::to_string(i)+"_score"));
            if(!std::isfinite(r.scores[i])) throw std::runtime_error("Nonfinite candidate score");
        }
        rows.push_back(std::move(r));
    }
    if(rows.empty()) throw std::runtime_error("Audit CSV has no data rows");
    return rows;
}
Summary validate(const std::vector<Row>& rows) {
    Summary s; s.rows=rows.size();
    const auto initial_scores=rows.front().scores;
    for(std::size_t j=0;j<rows.size();++j) {
        const auto& r=rows[j];
        if(r.raw==4) ++s.raw_action4;
        if(r.scores!=initial_scores) s.all_scores_constant=false;
        if(std::all_of(r.scores.begin(),r.scores.end(),[&](double x){return x==r.scores[0];})) ++s.equal_score_rows;
        if(std::any_of(r.symbols.begin(),r.symbols.end(),[](const auto& x){return x.empty();})) ++s.missing_candidates;
        if(j && r.symbols!=rows[j-1].symbols) ++s.candidate_slot_changes;
        if(j && r.scores!=rows[j-1].scores) ++s.score_changes;
        if(r.raw<0 || r.raw>6 || r.final<0 || r.final>6) throw std::runtime_error("Action outside 0..6");
        if(!r.legal[r.final]) ++s.illegal_final;
        int greedy=-1;
        for(int a=0;a<7;++a) if(r.legal[a] && (greedy<0 || r.q[a]>r.q[greedy])) greedy=a;
        if(greedy<0) throw std::runtime_error("No legal action");
        if(r.raw!=greedy) ++s.raw_not_greedy;
        if(r.recommendation=="EXIT_REQUIRED" && r.final==0 && r.raw!=0) ++s.forced_exits;
        if(r.recommendation=="EXIT_RECOMMENDED" && !r.held_after.empty()) ++s.advisory_holds;
        if(r.age>=10 && r.priority!="NORMAL") ++s.stagnant_reviews;
        const int m=minuteIST(r.time);
        if(m>=14*60+45 && !r.desired.empty() && r.desired!=r.held_before) ++s.entry_cutoff_violations;
        if(m>=15*60+15 && !r.held_after.empty()) ++s.deadline_violations;
    }
    if(s.all_scores_constant) warn(s,"Candidate scores are constant across the entire audit; inspect historical cache generation and ranking inputs.");
    if(s.equal_score_rows==s.rows) warn(s,"All five candidate scores are identical on every decision; ranking may be tied or placeholder-driven.");
    if(s.candidate_slot_changes==0) warn(s,"Top-5 slot-to-symbol mapping never changes; confirm universe ranking is refreshed causally.");
    if(s.raw_action4==s.rows || s.raw_action4+1==s.rows) warn(s,"Raw action 4 dominates; inspect manager feature encoding, training distribution and Q-value calibration.");
    if(s.advisory_holds) warn(s,"EXIT_RECOMMENDED was advisory only; manager continued holding on some rows.");
    if(s.illegal_final || s.entry_cutoff_violations || s.deadline_violations || s.raw_not_greedy) warn(s,"Decision integrity counters are nonzero; inspect individual audit rows.");
    return s;
}
std::string report(const Summary& s) {
    std::ostringstream o;
    o<<"D9F4CE_RANKING_DECISION_AUDIT rows="<<s.rows
     <<" raw_action4="<<s.raw_action4<<" raw_not_greedy="<<s.raw_not_greedy
     <<" illegal_final="<<s.illegal_final<<" forced_exits="<<s.forced_exits
     <<" advisory_holds="<<s.advisory_holds<<" slot_changes="<<s.candidate_slot_changes
     <<" score_changes="<<s.score_changes<<" equal_score_rows="<<s.equal_score_rows
     <<" all_scores_constant="<<s.all_scores_constant
     <<" cutoff_violations="<<s.entry_cutoff_violations
     <<" deadline_violations="<<s.deadline_violations<<'\n';
    for(const auto& w:s.warnings) o<<"WARNING: "<<w<<'\n';
    return o.str();
}
}
