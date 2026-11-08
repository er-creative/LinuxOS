// D9F.4C-F3D: read-only reconciliation of the F3C full-session CSV.
// Does not train models, place orders, or modify the production replay.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Row {
    std::int64_t decision_time{}, next_time{};
    int action{};
    std::string desired, held_after;
    double before{}, after{}, gross_reward{}, net_reward{}, selected_reward{}, costs{};
    bool terminal{};
};
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const auto pos = s.find(',', start);
        if (pos == std::string::npos) { fields.push_back(s.substr(start)); break; }
        fields.push_back(s.substr(start, pos-start));
        start = pos + 1;
    }
    return fields;
}
double parseDouble(const std::string& s) {
    std::size_t used = 0;
    const double x = std::stod(s, &used);
    require(used == s.size() && std::isfinite(x), "Invalid floating-point field");
    return x;
}
std::int64_t parseInt64(const std::string& s) {
    std::size_t used = 0;
    const auto x = std::stoll(s, &used);
    require(used == s.size(), "Invalid integer field");
    return x;
}
bool near(double a, double b) {
    return std::abs(a-b) <= 1e-8 * std::max({1.0, std::abs(a), std::abs(b)});
}
int minuteIST(std::int64_t t) {
    const auto local = ((t+19800)%86400+86400)%86400;
    return static_cast<int>(local/60);
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Usage: run_dev_f3d_pnl_reconciliation <F3C.csv>");
        std::ifstream in(argv[1]);
        require(static_cast<bool>(in), "Cannot open F3C CSV");
        std::string line;
        require(static_cast<bool>(std::getline(in, line)), "Missing CSV header");
        require(line == "decision_time,next_decision_time,manager_action,desired_symbol,held_after,"
                        "equity_before,equity_after_mark,gross_reward,net_reward,selected_reward,"
                        "execution_costs,terminal", "Unexpected F3C CSV header");
        std::vector<Row> rows;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            const auto f = split(line);
            require(f.size() == 12, "Malformed CSV row " + std::to_string(rows.size()+2));
            Row r{};
            r.decision_time = parseInt64(f[0]); r.next_time = parseInt64(f[1]);
            r.action = static_cast<int>(parseInt64(f[2]));
            r.desired = f[3]; r.held_after = f[4];
            r.before = parseDouble(f[5]); r.after = parseDouble(f[6]);
            r.gross_reward = parseDouble(f[7]); r.net_reward = parseDouble(f[8]);
            r.selected_reward = parseDouble(f[9]); r.costs = parseDouble(f[10]);
            const auto terminal = parseInt64(f[11]);
            require(terminal == 0 || terminal == 1, "Invalid terminal flag");
            r.terminal = terminal == 1;
            require(r.action >= 0 && r.action <= 6, "Manager action outside 0..6");
            require(r.before > 0 && r.after > 0 && r.costs >= 0, "Invalid equity or cost");
            require(r.next_time-r.decision_time == 60, "Non-minute step");
            if (!rows.empty()) {
                require(!rows.back().terminal, "Rows after terminal decision");
                require(r.decision_time == rows.back().next_time, "Missing/duplicate minute");
                require(near(r.before, rows.back().after), "Equity discontinuity");
            }
            rows.push_back(std::move(r));
        }
        require(rows.size() == 360, "Expected 360 decision rows");
        require(minuteIST(rows.front().decision_time) == 9*60+16, "Incorrect session start");
        require(minuteIST(rows.back().decision_time) == 15*60+15, "Incorrect session end");
        require(rows.back().terminal && rows.back().held_after.empty(), "Terminal not flat");
        double gross = 0, net = 0, costs = 0;
        std::size_t opens = 0, exits = 0, switches = 0, holds = 0, waits = 0;
        std::size_t active_minutes = 0, nonzero_cost_minutes = 0;
        std::string held_before;
        for (const auto& r : rows) {
            const double step_net = r.after-r.before;
            net += step_net; gross += step_net+r.costs; costs += r.costs;
            if (r.costs > 0) ++nonzero_cost_minutes;
            if (!r.held_after.empty()) ++active_minutes;
            if (held_before.empty() && r.held_after.empty()) ++waits;
            else if (held_before.empty()) ++opens;
            else if (r.held_after.empty()) ++exits;
            else if (held_before != r.held_after) ++switches;
            else ++holds;
            const int minute = minuteIST(r.decision_time);
            if (minute >= 14*60+45 && r.held_after != held_before && !r.held_after.empty())
                require(false, "Entry/switch at or after 14:45 IST");
            if (minute >= 15*60+15)
                require(r.held_after.empty(), "Position after 15:15 IST");
            held_before = r.held_after;
        }
        const double initial = rows.front().before, final = rows.back().after;
        require(near(net, final-initial), "Net P&L/equity mismatch");
        require(near(gross-costs, net), "Gross/net/cost mismatch");
        std::cout << std::fixed << std::setprecision(4)
                  << "D9F.4C-F3D — READ-ONLY F3C CSV RECONCILIATION\n"
                  << "Decisions               : " << rows.size() << '\n'
                  << "Initial equity          : " << initial << '\n'
                  << "Final equity            : " << final << '\n'
                  << "Gross P&L (cost-added)  : " << gross << '\n'
                  << "Aggregate costs         : " << costs << '\n'
                  << "Net P&L                 : " << net << '\n'
                  << "Net return (%)          : " << 100.0*net/initial << '\n'
                  << "Open transitions        : " << opens << '\n'
                  << "Exit transitions        : " << exits << '\n'
                  << "Switch transitions      : " << switches << '\n'
                  << "Hold transitions        : " << holds << '\n'
                  << "Flat wait transitions   : " << waits << '\n'
                  << "Minutes ending invested : " << active_minutes << '\n'
                  << "Minutes with costs      : " << nonzero_cost_minutes << '\n'
                  << "F3D CSV RECONCILIATION: PASS\n"
                  << "NOTE: Cost breakdown, individual legs and allocation require ledger-level audit.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "F3D CSV RECONCILIATION: FAILED: " << e.what() << '\n';
        return 1;
    }
}
