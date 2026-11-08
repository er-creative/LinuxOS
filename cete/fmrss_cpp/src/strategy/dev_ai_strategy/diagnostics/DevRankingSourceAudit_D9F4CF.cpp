#include "devai/strategy/dev_ai_strategy/diagnostics/DevRankingSourceAudit_D9F4CF.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
namespace devai::strategy::diagnostics::d9f4cf {
namespace {
using Fields = std::unordered_map<std::string,std::string>;
std::vector<std::string> parseCsv(const std::string& s) {
    std::vector<std::string> out; std::string value; bool quoted=false;
    for(std::size_t i=0;i<s.size();++i){const char c=s[i]; if(c=='"'){if(quoted && i+1<s.size() && s[i+1]=='"'){value+='"';++i;}else quoted=!quoted;}else if(c==','&&!quoted){out.push_back(value);value.clear();}else value+=c;}
    if(quoted) throw std::runtime_error("Unterminated CSV quoted field");
    out.push_back(value);return out;
}
std::vector<Fields> readCsv(const std::string& path,const std::vector<std::string>& required){
    std::ifstream in(path);if(!in)throw std::runtime_error("Cannot open: "+path);
    std::string line;if(!std::getline(in,line))throw std::runtime_error("Empty CSV: "+path);
    auto header=parseCsv(line);if(!header.empty()&&!header[0].empty()&&header[0][0]=='\xef') { if(header[0].size()>=3)header[0].erase(0,3); }
    for(const auto& key:required)if(std::find(header.begin(),header.end(),key)==header.end())throw std::runtime_error("Missing column '"+key+"' in "+path);
    std::vector<Fields> rows;std::size_t lineNo=1;
    while(std::getline(in,line)){++lineNo;if(line.empty())continue;auto cells=parseCsv(line);if(cells.size()!=header.size())throw std::runtime_error("CSV column count at line "+std::to_string(lineNo)+" in "+path);Fields row;for(std::size_t i=0;i<header.size();++i)row[header[i]]=cells[i];rows.push_back(std::move(row));}
    return rows;
}
double number(const std::string& s){std::size_t pos=0;double x=std::stod(s,&pos);if(pos!=s.size()||!std::isfinite(x))throw std::runtime_error("Invalid numeric value: "+s);return x;}
std::int64_t integer(const std::string& s){std::size_t pos=0;auto x=std::stoll(s,&pos);if(pos!=s.size())throw std::runtime_error("Invalid integer value: "+s);return x;}
void example(Report& r,const std::string& s){if(r.examples.size()<15)r.examples.push_back(s);}
bool close(double a,double b){return std::abs(a-b)<=1e-8*std::max({1.0,std::abs(a),std::abs(b)});}
}
Report inspect(const std::string& universe_csv,const std::string& decision_audit_csv,std::size_t expected_universe){
    if(expected_universe<5)throw std::runtime_error("Expected universe must contain at least five symbols");
    auto rows=readCsv(universe_csv,{"decision_time","symbol","mean_return","stddev_return","forecast_return","forecast_enabled","score","last_input_time","forecast_last_input_time"});
    Report r; r.rows=rows.size();std::map<std::int64_t,std::vector<Candidate>> byTime;std::set<std::string> symbols;
    for(const auto& f:rows){Candidate c;try{
        c.decision_time=integer(f.at("decision_time"));c.symbol=f.at("symbol");
        c.mean_return=number(f.at("mean_return"));c.stddev_return=number(f.at("stddev_return"));c.forecast_return=number(f.at("forecast_return"));
        c.forecast_enabled=(f.at("forecast_enabled")=="1");if(f.at("forecast_enabled")!="0"&&f.at("forecast_enabled")!="1")throw std::runtime_error("forecast_enabled must be 0/1");
        c.score=number(f.at("score"));c.last_input_time=integer(f.at("last_input_time"));c.forecast_last_input_time=integer(f.at("forecast_last_input_time"));
        if(c.symbol.empty())throw std::runtime_error("Empty symbol");
    }catch(const std::exception& e){++r.invalid_numbers;example(r,std::string("Invalid row: ")+e.what());continue;}
        if(c.last_input_time>c.decision_time){++r.future_inputs;example(r,"Future statistical input for "+c.symbol+" at "+std::to_string(c.decision_time));}
        if(c.forecast_enabled&&c.forecast_last_input_time>c.decision_time){++r.future_forecasts;example(r,"Future forecast input for "+c.symbol+" at "+std::to_string(c.decision_time));}
        symbols.insert(c.symbol);byTime[c.decision_time].push_back(c);
    }
    r.unique_symbols=symbols.size();r.decisions=byTime.size();
    std::map<std::int64_t,std::vector<Candidate>> ranked;
    std::vector<std::string> priorTop;std::map<std::string,double> priorScores;bool hasPrior=false;
    for(auto& [t,candidates]:byTime){
        std::set<std::string> seen;for(const auto& c:candidates)if(!seen.insert(c.symbol).second){++r.duplicates;example(r,"Duplicate symbol "+c.symbol+" at "+std::to_string(t));}
        if(seen.size()!=expected_universe){++r.incomplete_universe;example(r,"Expected "+std::to_string(expected_universe)+" symbols at "+std::to_string(t)+", got "+std::to_string(seen.size()));}
        std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){if(a.score!=b.score)return a.score>b.score;return a.symbol<b.symbol;});
        if(candidates.size()>=2&&close(candidates.front().score,candidates.back().score))++r.score_ties;
        std::vector<std::string> top;for(std::size_t i=0;i<std::min(std::size_t(5),candidates.size());++i)top.push_back(candidates[i].symbol);
        std::map<std::string,double> scores;for(const auto& c:candidates)scores[c.symbol]=c.score;
        if(hasPrior){if(scores==priorScores)++r.static_score_decisions;if(top==priorTop)++r.static_top5_decisions;}
        hasPrior=true;priorTop=top;priorScores=scores;ranked[t]=candidates;
    }
    if(!decision_audit_csv.empty()){
        auto audits=readCsv(decision_audit_csv,{"decision_time","slot_0_symbol","slot_0_score","slot_1_symbol","slot_1_score","slot_2_symbol","slot_2_score","slot_3_symbol","slot_3_score","slot_4_symbol","slot_4_score"});
        std::set<std::int64_t> visited;
        for(const auto& f:audits){const auto t=integer(f.at("decision_time"));auto it=ranked.find(t);if(it==ranked.end()){++r.missing_audit_decisions;example(r,"No universe trace for audit time "+std::to_string(t));continue;}visited.insert(t);
            const auto& c=it->second;
            if(c.size()<5)continue;
            // Ranking ties are inherently ambiguous without a documented production tie-breaker.
            bool anyTie=false;for(std::size_t i=1;i<c.size();++i)if(close(c[i-1].score,c[i].score)){anyTie=true;break;}
            std::set<std::string> selected;
            for(std::size_t i=0;i<5;++i){const auto suffix=std::to_string(i);const auto symbol=f.at("slot_"+suffix+"_symbol");selected.insert(symbol);
                auto found=std::find_if(c.begin(),c.end(),[&](const Candidate& x){return x.symbol==symbol;});
                if(found==c.end()){++r.top5_mapping_mismatches;example(r,"Unknown selected symbol "+symbol+" at "+std::to_string(t));continue;}
                try{if(!close(found->score,number(f.at("slot_"+suffix+"_score")))){++r.top5_score_mismatches;example(r,"Score mismatch for "+symbol+" at "+std::to_string(t));}}catch(const std::exception&){++r.invalid_numbers;}
                if(!anyTie&&c[i].symbol!=symbol){++r.ranking_mismatches;example(r,"Wrong rank at slot "+suffix+" time "+std::to_string(t));}
            }
            if(selected.size()!=5){++r.top5_mapping_mismatches;example(r,"Duplicate Top-5 selection at "+std::to_string(t));}
            // When ties exist, the top five can differ legitimately; flag only scores/symbol validity.
        }
        for(const auto& [t,c]:ranked)if(!visited.count(t))example(r,"Universe trace has no decision audit at "+std::to_string(t));
    }
    return r;
}
std::string summary(const Report& r){std::ostringstream o;o<<"D9F4CF_RANKING_SOURCE_AUDIT rows="<<r.rows<<" decisions="<<r.decisions<<" symbols="<<r.unique_symbols<<" invalid="<<r.invalid_numbers<<" future_inputs="<<r.future_inputs<<" future_forecasts="<<r.future_forecasts<<" duplicates="<<r.duplicates<<" incomplete_universe="<<r.incomplete_universe<<" all_equal_score_decisions="<<r.score_ties<<" unchanged_score_transitions="<<r.static_score_decisions<<" unchanged_top5_transitions="<<r.static_top5_decisions<<" missing_trace="<<r.missing_audit_decisions<<" ranking_mismatches="<<r.ranking_mismatches<<" mapping_mismatches="<<r.top5_mapping_mismatches<<" score_mismatches="<<r.top5_score_mismatches<<" integrity="<<(r.integrityPass()?"PASS":"FAIL");return o.str();}
}
