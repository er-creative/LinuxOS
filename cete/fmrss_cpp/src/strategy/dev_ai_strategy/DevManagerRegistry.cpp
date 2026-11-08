#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <sstream>

namespace fs = std::filesystem;
namespace devai {
namespace {
bool valid_id(const std::string& s) {
    if (s.size() <= 11 || s.rfind("DevManager_", 0) != 0) return false;
    return std::all_of(s.begin()+11, s.end(), [](unsigned char c){return std::isdigit(c);});
}
void write_atomic(const fs::path& target, const std::string& content) {
    const auto tmp = fs::path(target.string()+".tmp");
    { std::ofstream out(tmp, std::ios::trunc); if (!out) throw std::runtime_error("Cannot write "+tmp.string()); out << content; out.flush(); if (!out) throw std::runtime_error("Write failed"); }
    try { fs::rename(tmp, target); } catch (...) { fs::remove(tmp); throw; }
}
}
DevManagerRegistry::DevManagerRegistry(fs::path root):root_(std::move(root)) {fs::create_directories(root_);}
fs::path DevManagerRegistry::manager_path(const std::string& id) const {
    if (!valid_id(id)) throw std::invalid_argument("Invalid manager id");
    return root_/id;
}
std::vector<std::string> DevManagerRegistry::validate_symbols(const std::vector<std::string>& symbols) {
    if (symbols.empty()) throw std::invalid_argument("Share universe must not be empty");
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (auto s : symbols) {
        if (s.empty() || !std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isupper(c)||std::isdigit(c)||c=='_'||c=='&'||c=='-';}))
            throw std::invalid_argument("Invalid symbol: "+s);
        if (seen.insert(s).second) result.push_back(std::move(s));
    }
    return result;
}
void DevManagerRegistry::save(const ManagedUniverse& m) const {
    const auto dir=manager_path(m.manager_id);
    if (!fs::is_directory(dir)) throw std::runtime_error("Manager directory missing: "+m.manager_id);
    std::ostringstream shares;
    for (const auto& s:m.symbols) shares<<s<<'\n';
    write_atomic(dir/"shares.txt", shares.str());
    write_atomic(dir/"manager.conf", std::string("id=")+m.manager_id+"\nactive="+(m.active?"true":"false")+"\n");
}
ManagedUniverse DevManagerRegistry::create_manager(const std::vector<std::string>& symbols) {
    const auto validated=validate_symbols(symbols);
    // Determine next id from existing directories; never reuse a deleted id if IDs are retained as inactive.
    unsigned long long next=1;
    for (const auto& m:list_managers()) next=std::max(next,std::stoull(m.manager_id.substr(11))+1);
    std::ostringstream name; name<<"DevManager_"<<std::setw(3)<<std::setfill('0')<<next;
    ManagedUniverse m{name.str(),true,validated};
    if (!fs::create_directory(manager_path(m.manager_id))) throw std::runtime_error("Manager already exists");
    save(m);
    return m;
}
ManagedUniverse DevManagerRegistry::load_manager(const std::string& id) const {
    const auto dir=manager_path(id);
    std::ifstream shares(dir/"shares.txt"), config(dir/"manager.conf");
    if (!shares || !config) throw std::runtime_error("Manager configuration missing: "+id);
    ManagedUniverse m; m.manager_id=id;
    std::string line;
    while(std::getline(shares,line)) {if (!line.empty() && line.back()=='\r') line.pop_back(); if(!line.empty())m.symbols.push_back(line);}
    m.symbols=validate_symbols(m.symbols);
    bool has_id=false,has_active=false;
    while(std::getline(config,line)) {
        if(line=="id="+id) has_id=true;
        if(line=="active=true") {m.active=true;has_active=true;}
        if(line=="active=false") {m.active=false;has_active=true;}
    }
    if (!has_id||!has_active) throw std::runtime_error("Invalid manager configuration: "+id);
    return m;
}
void DevManagerRegistry::assign_shares(const std::string& id,const std::vector<std::string>& symbols) {
    auto m=load_manager(id); m.symbols=validate_symbols(symbols); save(m);
}
void DevManagerRegistry::set_active(const std::string& id,bool active) {
    auto m=load_manager(id); m.active=active; save(m);
}
std::vector<ManagedUniverse> DevManagerRegistry::list_managers() const {
    std::vector<ManagedUniverse> out;
    for(const auto& e:fs::directory_iterator(root_))
        if(e.is_directory()&&valid_id(e.path().filename().string()))out.push_back(load_manager(e.path().filename().string()));
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.manager_id<b.manager_id;});
    return out;
}
}
