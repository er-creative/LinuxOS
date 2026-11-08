#include "devai/config/DevDualManagerActivation.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace devai::config {
namespace {
std::string trim(std::string s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
bool parseBool(const std::string& s) {
    if (s == "true") return true;
    if (s == "false") return false;
    throw std::runtime_error("Expected true/false, got: " + s);
}
double parseDouble(const std::string& s) {
    std::size_t pos = 0;
    const auto value = std::stod(s, &pos);
    if (pos != s.size() || !std::isfinite(value)) throw std::runtime_error("Invalid number: " + s);
    return value;
}
std::size_t parseSize(const std::string& s) {
    std::size_t pos = 0;
    const auto value = std::stoull(s, &pos);
    if (pos != s.size()) throw std::runtime_error("Invalid integer: " + s);
    return static_cast<std::size_t>(value);
}
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v;
    std::stringstream stream(s);
    std::string part;
    while (std::getline(stream, part, ',')) v.push_back(trim(part));
    return v;
}
const std::string& required(const std::map<std::string,std::string>& values, const std::string& key) {
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) throw std::runtime_error("Missing configuration: " + key);
    return it->second;
}
} // namespace
DualManagerActivation loadDualManagerActivation(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open configuration: " + path);
    std::map<std::string,std::string> values;
    std::string section, line;
    std::size_t line_number = 0;
    while (std::getline(file,line)) {
        ++line_number;
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1,line.size()-2));
            if (section.empty()) throw std::runtime_error("Empty section");
            continue;
        }
        const auto equal = line.find('=');
        if (section.empty() || equal == std::string::npos) throw std::runtime_error("Malformed line " + std::to_string(line_number));
        const auto key = section + "." + trim(line.substr(0,equal));
        if (!values.emplace(key,trim(line.substr(equal+1))).second) throw std::runtime_error("Duplicate key: " + key);
    }
    DualManagerActivation out;
    out.shared_model = required(values,"SharedModel.model_path");
    out.technical_supervisor = required(values,"Supervision.technical_supervisor");
    out.portfolio_supervisor = required(values,"Supervision.portfolio_supervisor");
    out.portfolio_capital = parseDouble(required(values,"Supervision.portfolio_capital"));
    out.live_short_execution = parseBool(required(values,"Supervision.live_short_execution"));
    if (out.technical_supervisor != "DevAISupervisor" || out.portfolio_supervisor != "DevinderjeetSupervisor")
        throw std::runtime_error("Unexpected supervisor identity");
    if (out.portfolio_capital <= 0) throw std::runtime_error("Invalid portfolio capital");
    double total = 0;
    std::set<std::string> agent_ids;
    for (std::size_t i=0;i<2;++i) {
        auto& m = out.managers[i];
        m.id = "DevManager_00" + std::to_string(i+1);
        const auto key = m.id + ".";
        m.enabled = parseBool(required(values,key+"enabled"));
        const auto mode = required(values,key+"mode");
        if (mode == "TESTING") m.mode = ExecutionMode::TESTING;
        else if (mode == "LIVE") m.mode = ExecutionMode::LIVE;
        else throw std::runtime_error("Invalid mode for " + m.id + ": " + mode);
        const auto testing = required(values,key+"testing_type");
        if (testing == "BACKTEST") m.testing_type = TestingType::BACKTEST;
        else if (testing == "FORWARD") m.testing_type = TestingType::FORWARD;
        else if (testing == "PAPER") m.testing_type = TestingType::PAPER;
        else throw std::runtime_error("Invalid testing_type for " + m.id + ": " + testing);
        const auto direction = required(values,key+"direction");
        if (direction != (i == 0 ? "LONG" : "SHORT")) throw std::runtime_error("Incorrect direction for " + m.id);
        m.direction = i == 0 ? Direction::LONG : Direction::SHORT;
        m.capital = parseDouble(required(values,key+"capital"));
        m.allocation_percent = parseDouble(required(values,key+"allocation_percent"));
        m.top_k = parseSize(required(values,key+"top_k"));
        m.universe_file = required(values,key+"universe_file");
        auto agents = split(required(values,key+"agents"));
        if (agents.size() != 5) throw std::runtime_error("Exactly five agents required for " + m.id);
        for (std::size_t a=0;a<5;++a) {
            const auto expected = "DevAgent_" + std::string(a+i*5+1 < 10 ? "00" : "0") + std::to_string(a+i*5+1);
            if (agents[a] != expected || !agent_ids.insert(agents[a]).second) throw std::runtime_error("Agent ownership mismatch: " + agents[a]);
            m.agents[a] = agents[a];
        }
        if (m.capital <= 0 || m.allocation_percent <= 0 || m.allocation_percent > 100 || m.top_k != 5)
            throw std::runtime_error("Invalid manager risk/ranking settings: " + m.id);
        total += m.capital;
        m.agents_active = m.enabled;
        // Both managers are planning-only until existing execution contracts are upgraded and verified.
        m.execution_authorized = false;
    }
    if (total > out.portfolio_capital + 1e-6) throw std::runtime_error("Manager allocations exceed portfolio capital");
    if (out.live_short_execution) throw std::runtime_error("Live SHORT execution unsupported by current CETE supervisor; fail closed");
    return out;
}
std::string modeName(ExecutionMode mode) {
    return mode == ExecutionMode::LIVE ? "LIVE" : "TESTING";
}
std::string testingTypeName(TestingType type) {
    switch (type) {
        case TestingType::BACKTEST: return "BACKTEST";
        case TestingType::FORWARD: return "FORWARD";
        case TestingType::PAPER: return "PAPER";
    }
    throw std::logic_error("Unknown testing type");
}
bool mayEvaluate(const DualManagerActivation& config, const std::string& id) {
    for (const auto& m : config.managers) if (m.id == id) return m.enabled && m.agents_active;
    return false;
}
std::string activationReport(const DualManagerActivation& config) {
    std::ostringstream out;
    out << "CETE dual-manager activation (PLANNING ONLY)\n";
    out << "Shared RL model: " << config.shared_model << "\n";
    out << "Reports to: " << config.technical_supervisor << ", " << config.portfolio_supervisor << "\n";
    for (const auto& m : config.managers) {
        out << m.id << " " << (m.direction == Direction::LONG ? "LONG" : "SHORT")
            << " status=" << (m.enabled ? "ENABLED" : "DISABLED")
            << " mode=" << modeName(m.mode)
            << " testing_type=" << (m.mode == ExecutionMode::TESTING ? testingTypeName(m.testing_type) : "N/A")
            << " agents=" << (m.agents_active ? "ACTIVE" : "INACTIVE")
            << " top_k=" << m.top_k << " capital=" << m.capital
            << " execution=BLOCKED_PENDING_INTEGRATION\n";
    }
    return out.str();
}
} // namespace devai::config
