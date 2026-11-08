// CETE H4.7 Stage 5E.2K.5 — read-only frozen-ledger trade efficiency audit.
// Compile: g++ -std=c++20 -O2 -Wall -Wextra -Werror tools/h47_trade_efficiency_audit.cpp -o build/h47_trade_efficiency_audit
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <ctime>

namespace fs = std::filesystem;
using Row = std::map<std::string, std::string>;

static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::string cell;
    bool quoted = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') {
            if (quoted && i + 1 < s.size() && s[i + 1] == '"') { cell += '"'; ++i; }
            else quoted = !quoted;
        } else if (c == ',' && !quoted) { out.push_back(cell); cell.clear(); }
        else cell += c;
    }
    if (quoted) throw std::runtime_error("Unterminated CSV quoted field");
    out.push_back(cell);
    return out;
}
static std::string field(const Row& r, const std::string& k) {
    auto it = r.find(k);
    if (it == r.end()) throw std::runtime_error("Missing field: " + k);
    return it->second;
}
static double num(const Row& r, const std::string& k) {
    auto s = field(r, k);
    if (s.empty()) return 0.0;
    std::size_t n = 0;
    double x = std::stod(s, &n);
    if (n != s.size() || !std::isfinite(x)) throw std::runtime_error("Invalid numeric value: " + k);
    return x;
}
static std::int64_t integer(const Row& r, const std::string& k) {
    auto s = field(r, k);
    if (s.empty()) throw std::runtime_error("Missing integer value: " + k);
    std::size_t n = 0;
    auto x = std::stoll(s, &n);
    if (n != s.size()) throw std::runtime_error("Invalid integer: " + k);
    return x;
}
static std::string ist(std::int64_t epoch) {
    std::time_t t = static_cast<std::time_t>(epoch + 19800); // IST = UTC+05:30, no DST
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    if (!gmtime_r(&t, &tm)) throw std::runtime_error("Invalid timestamp");
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S+05:30");
    return ss.str();
}
static int ist_minute(std::int64_t epoch) {
    std::int64_t m = (epoch + 19800) / 60;
    int v = static_cast<int>(m % 1440);
    return v < 0 ? v + 1440 : v;
}
struct Trade {
    std::string symbol;
    std::int64_t entry{}, exit{}, qty{};
    double entry_price{}, exit_price{}, entry_brokerage{}, exit_brokerage{};
    double entry_slippage{}, exit_slippage{}, entry_other{}, exit_other{};
    double gross{}, cost{}, net{};
    double minutes() const { return static_cast<double>(exit - entry) / 60.0; }
};
static std::vector<Row> read_rows(const fs::path& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open input: " + path.string());
    std::string line;
    if (!std::getline(f, line)) throw std::runtime_error("Empty input");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto header = split_csv(line);
    const std::vector<std::string> required = {"decision_time","previous_symbol","desired_symbol","held_symbol","quantity","cash","equity_before","equity_after","gross_pnl","brokerage","slippage","other_costs","net_pnl","drawdown","executed_legs"};
    for (const auto& k : required)
        if (std::find(header.begin(), header.end(), k) == header.end()) throw std::runtime_error("Missing required CSV column: " + k);
    std::vector<Row> rows;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto cols = split_csv(line);
        if (cols.size() != header.size()) throw std::runtime_error("CSV column count mismatch on data row " + std::to_string(rows.size() + 1));
        Row row;
        for (std::size_t i = 0; i < header.size(); ++i) row.emplace(header[i], cols[i]);
        rows.push_back(std::move(row));
    }
    if (rows.empty()) throw std::runtime_error("No decision records");
    return rows;
}
static void run(const fs::path& input, const fs::path& output) {
    const auto rows = read_rows(input);
    const double capital = num(rows.front(), "equity_before");
    double prev_cash = capital, gross_sum = 0, cost_sum = 0, net_sum = 0;
    std::int64_t prev_qty = 0, prev_time = 0, legs_sum = 0;
    std::string prev_symbol;
    bool active = false;
    Trade position;
    std::vector<Trade> completed;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        const auto t = integer(r, "decision_time");
        const auto qty = integer(r, "quantity");
        const auto legs = integer(r, "executed_legs");
        const auto symbol = field(r, "held_symbol");
        const double cash = num(r, "cash");
        const double brokerage = num(r, "brokerage");
        const double slip = num(r, "slippage");
        const double other = num(r, "other_costs");
        const double cost = brokerage + slip + other;
        const double gross = num(r, "gross_pnl");
        const double net = num(r, "net_pnl");
        const auto fail = [&](const std::string& why) { throw std::runtime_error("Row " + std::to_string(i + 1) + ": " + why); };
        if (t <= prev_time) fail("non-increasing timestamp");
        if (qty < 0 || legs < 0) fail("negative quantity/legs");
        if ((qty == 0) != symbol.empty()) fail("position symbol/quantity mismatch");
        if (std::abs(gross - cost - net) > 1e-5) fail("row P&L reconciliation mismatch");
        if (legs > 1) fail("multi-leg row requires native per-leg C++ instrumentation");
        gross_sum += gross; cost_sum += cost; net_sum += net; legs_sum += legs;
        if (prev_qty == 0 && qty > 0) {
            if (legs != 1 || active) fail("entry leg/position mismatch");
            const double price = (prev_cash - cash - cost) / static_cast<double>(qty);
            if (!(price > 0) || !std::isfinite(price)) fail("invalid inferred entry price");
            position = {};
            position.symbol = symbol; position.entry = t; position.qty = qty;
            position.entry_price = price; position.entry_brokerage = brokerage;
            position.entry_slippage = slip; position.entry_other = other;
            active = true;
        } else if (prev_qty > 0 && qty == 0) {
            if (legs != 1 || !active || position.qty != prev_qty || position.symbol != prev_symbol) fail("exit leg/position mismatch");
            const double price = (cash - prev_cash + cost) / static_cast<double>(prev_qty);
            if (!(price > 0) || !std::isfinite(price)) fail("invalid inferred exit price");
            position.exit = t; position.exit_price = price;
            position.exit_brokerage = brokerage; position.exit_slippage = slip; position.exit_other = other;
            position.cost = position.entry_brokerage + position.entry_slippage + position.entry_other + cost;
            position.gross = static_cast<double>(prev_qty) * (price - position.entry_price);
            position.net = position.gross - position.cost;
            completed.push_back(position);
            active = false;
        } else if (prev_qty > 0 && qty > 0) {
            if (qty != prev_qty || symbol != prev_symbol || legs != 0) fail("position changed while invested; requires per-leg instrumentation");
        } else if (legs != 0) fail("unexpected execution while flat");
        prev_cash = cash; prev_qty = qty; prev_symbol = symbol; prev_time = t;
    }
    if (active) throw std::runtime_error("Ledger ends with an unclosed position");
    const double equity_change = num(rows.back(), "equity_after") - capital;
    if (std::abs(equity_change - net_sum) > 0.01) throw std::runtime_error("Equity/net P&L reconciliation failed");
    const double trade_gross = std::accumulate(completed.begin(), completed.end(), 0.0, [](double v, const Trade& t) { return v + t.gross; });
    const double trade_net = std::accumulate(completed.begin(), completed.end(), 0.0, [](double v, const Trade& t) { return v + t.net; });
    if (std::abs(trade_gross - gross_sum) > 0.02 || std::abs(trade_net - net_sum) > 0.02)
        throw std::runtime_error("Trade-level and ledger-level P&L do not reconcile; no output written");
    if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
    std::ofstream out(output);
    if (!out) throw std::runtime_error("Cannot create output: " + output.string());
    out << "symbol,entry_epoch,entry_time_ist,exit_epoch,exit_time_ist,quantity,entry_price,exit_price,holding_minutes,gross_trade_pnl,entry_brokerage,exit_brokerage,entry_slippage,exit_slippage,entry_other_costs,exit_other_costs,total_cost,net_trade_pnl,exit_reason_class\n";
    out << std::setprecision(12);
    std::map<std::string, std::vector<Trade>> groups;
    std::vector<double> durations;
    int short_trades = 0, gross_wins = 0, net_wins = 0;
    for (const auto& t : completed) {
        const char* reason = ist_minute(t.exit) >= 915 ? "SQUARE_OFF_TIME_CANDIDATE" : "POLICY_OR_OTHER_EXIT_UNVERIFIED";
        out << t.symbol << ',' << t.entry << ',' << ist(t.entry) << ',' << t.exit << ',' << ist(t.exit) << ',' << t.qty << ','
            << t.entry_price << ',' << t.exit_price << ',' << t.minutes() << ',' << t.gross << ','
            << t.entry_brokerage << ',' << t.exit_brokerage << ',' << t.entry_slippage << ',' << t.exit_slippage << ','
            << t.entry_other << ',' << t.exit_other << ',' << t.cost << ',' << t.net << ',' << reason << '\n';
        groups[t.symbol].push_back(t);
        durations.push_back(t.minutes());
        short_trades += t.minutes() <= 5;
        gross_wins += t.gross > 0;
        net_wins += t.net > 0;
    }
    out.close();
    if (!out) throw std::runtime_error("Failed writing output CSV");
    std::sort(durations.begin(), durations.end());
    const double median = durations.empty() ? 0 : (durations.size() % 2 ? durations[durations.size()/2] : (durations[durations.size()/2-1] + durations[durations.size()/2])/2);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== H4.7 STAGE 5E.2K.5 — C++ TRADE EFFICIENCY ===\n";
    std::cout << "Decision rows: " << rows.size() << " | Completed round trips: " << completed.size() << '\n';
    std::cout << "Executed legs: " << legs_sum << '\n';
    std::cout << "Gross P&L: INR " << gross_sum << " | Costs: INR " << cost_sum << " | Net P&L: INR " << net_sum << '\n';
    std::cout << "Equity reconciliation error: INR " << std::abs(equity_change - net_sum) << '\n';
    std::cout << "Completed-trade gross P&L: INR " << trade_gross << " | net P&L: INR " << trade_net << '\n';
    std::cout << "Median holding time: " << median << " minutes | Trades <= 5 minutes: " << short_trades << '\n';
    std::cout << "Gross profitable: " << gross_wins << " | Net profitable: " << net_wins << '\n';
    std::cout << "Symbol | trades | gross P&L | costs | net P&L\n";
    for (const auto& [symbol, trades] : groups) {
        double g=0, c=0, n=0;
        for (const auto& t : trades) { g+=t.gross; c+=t.cost; n+=t.net; }
        std::cout << symbol << " | " << trades.size() << " | " << g << " | " << c << " | " << n << '\n';
    }
    std::cout << "Trade detail CSV: " << output << '\n';
    std::cout << "NOTE: Exit reasons cannot be proven from the current ledger; 15:15 classification is only a time-based candidate.\n";
    std::cout << "NOTE: Reconstructed prices are inferred from cash movements; slippage is modeled as a separate cost.\n";
}
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <frozen_validation_zerodha.csv> <trade_efficiency.csv>\n";
        return 2;
    }
    try { run(argv[1], argv[2]); }
    catch (const std::exception& e) { std::cerr << "[CETE][5E2K5][ERROR] " << e.what() << '\n'; return 1; }
}
