#include "devai/benchmark/DevBenchmarkContract.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
#include <sstream>
namespace devai::benchmark {
namespace {
std::string jsonString(const std::string& s) {
    std::string o="\"";
    for (unsigned char c : s) {
        if (c=='"'||c=='\\') { o+='\\'; o+=char(c); }
        else if(c=='\n') o+="\\n";
        else if(c=='\r') o+="\\r";
        else if(c=='\t') o+="\\t";
        else if(c<32) throw std::runtime_error("control character in contract");
        else o+=char(c);
    }
    return o+'"';
}
std::string arr(const std::vector<std::string>& a) {
    std::string s="[";
    for (const auto& v:a) { if(s.size()>1)s+=",";s+=jsonString(v); }
    return s+"]";
}
}
std::vector<std::string> readUniverse(const std::filesystem::path& path) {
    std::ifstream f(path); if(!f)throw std::runtime_error("cannot read universe: "+path.string());
    std::vector<std::string> out;std::set<std::string> seen;std::string line;
    while(std::getline(f,line)) {
        const auto hash=line.find('#');if(hash!=std::string::npos)line.resize(hash);
        const auto a=line.find_first_not_of(" \t\r");if(a==std::string::npos)continue;
        const auto b=line.find_last_not_of(" \t\r");line=line.substr(a,b-a+1);
        if(line.find_first_of(" \t,/:\\")!=std::string::npos)throw std::runtime_error("invalid universe symbol: "+line);
        if(!seen.insert(line).second)throw std::runtime_error("duplicate universe symbol: "+line);
        out.push_back(line);
    }
    return out;
}
void DevBenchmarkContract::validate() const {
    if(contract_version!="CETE_H1_V1"||candidates.size()!=4||
       candidates!=std::vector<std::string>{"candidate_0_sequential_td","maskable_ppo","qr_dqn","discrete_cql"})
        throw std::runtime_error("unexpected benchmark candidates/version");
    if(universe.size()!=10||std::set<std::string>(universe.begin(),universe.end()).size()!=10)
        throw std::runtime_error("benchmark requires exactly ten distinct symbols");
    if(!std::isfinite(capital_per_manager)||capital_per_manager<=0||
       !std::isfinite(allocation_fraction)||allocation_fraction<=0||allocation_fraction>1)
        throw std::runtime_error("invalid capital/allocation");
    if(data_start_utc.empty()||data_end_utc.empty()||data_start_utc>=data_end_utc||
       feature_schema_id.empty()||feature_schema_sha256.empty()||split_manifest_sha256.empty()||
       execution_model_id.empty()||costs_model_id.empty())
        throw std::runtime_error("benchmark evidence fields not yet configured");
    if(!allow_long||!allow_short||live_orders)throw std::runtime_error("H1 must cover both directions with LIVE disabled");
}
std::string DevBenchmarkContract::toJson() const {
    std::ostringstream o;
    o<<"{\n  \"contract_version\": "<<jsonString(contract_version)
     <<",\n  \"candidates\": "<<arr(candidates)
     <<",\n  \"universe\": "<<arr(universe)
     <<",\n  \"data_start_utc\": "<<jsonString(data_start_utc)
     <<",\n  \"data_end_utc\": "<<jsonString(data_end_utc)
     <<",\n  \"feature_schema_id\": "<<jsonString(feature_schema_id)
     <<",\n  \"feature_schema_sha256\": "<<jsonString(feature_schema_sha256)
     <<",\n  \"split_manifest_sha256\": "<<jsonString(split_manifest_sha256)
     <<",\n  \"execution_model_id\": "<<jsonString(execution_model_id)
     <<",\n  \"costs_model_id\": "<<jsonString(costs_model_id)
     <<",\n  \"capital_per_manager\": "<<capital_per_manager
     <<",\n  \"allocation_fraction\": "<<allocation_fraction
     <<",\n  \"seed\": "<<seed
     <<",\n  \"allow_long\": "<<(allow_long?"true":"false")
     <<",\n  \"allow_short\": "<<(allow_short?"true":"false")
     <<",\n  \"live_orders\": "<<(live_orders?"true":"false")<<"\n}\n";
    return o.str();
}
void DevBenchmarkContract::write(const std::filesystem::path& path) const {
    validate();if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    if(std::filesystem::exists(path))throw std::runtime_error("contract already exists; immutable: "+path.string());
    std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot write contract");
    f<<toJson();if(!f)throw std::runtime_error("contract write failed");
}
}
