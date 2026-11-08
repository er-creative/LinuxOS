#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
namespace {
std::vector<std::string> split(std::string line) {
    std::replace(line.begin(), line.end(), '\t', ',');
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) fields.push_back(field);
    return fields;
}
std::int64_t parseIST(const std::string& date, const std::string& time) {
    int y=0, m=0, d=0, hh=0, mm=0, sec=0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d)!=3 ||
        std::sscanf(time.c_str(), "%d:%d:%d", &hh, &mm, &sec)<2)
        throw std::runtime_error("3.19C: invalid candle date/time");
    using namespace std::chrono;
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(m)},
                             day{static_cast<unsigned>(d)}};
    if (!ymd.ok() || hh>23 || mm>59 || sec>59 || hh<0 || mm<0 || sec<0)
        throw std::runtime_error("3.19C: invalid IST candle timestamp");
    // CETE timestamps are Unix UTC seconds; source files use Asia/Kolkata.
    return (sys_days{ymd}.time_since_epoch() + hours{hh} + minutes{mm}
            + seconds{sec} - minutes{330}).count();
}
void ensure(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}
void DevHistoricalCloseStore::loadMinuteFile(
    const std::filesystem::path& file, const std::string& symbol) {
    ensure(!symbol.empty(), "3.19C: empty symbol");
    std::ifstream in(file);
    if (!in) throw std::runtime_error("3.19C: cannot open " + file.string());
    std::map<std::int64_t,double> parsed;
    std::string line;
    while (std::getline(in,line)) {
        if (line.empty()) continue;
        auto v=split(line);
        if (v.size()<8) throw std::runtime_error("3.19C: malformed row in " + file.string());
        if (v[0]=="Symbol") continue;
        if (v[0]!=symbol) throw std::runtime_error("3.19C: symbol mismatch in " + file.string());
        const auto t=parseIST(v[1],v[2]);
        const double close=std::stod(v[6]);
        ensure(std::isfinite(close) && close>0, "3.19C: invalid close");
        if (!parsed.emplace(t,close).second)
            throw std::runtime_error("3.19C: duplicate candle timestamp");
    }
    ensure(!parsed.empty(), "3.19C: no candles loaded");
    prices_[symbol]=std::move(parsed);
}
void DevHistoricalCloseStore::loadMinuteFolder(
    const std::filesystem::path& folder, const std::vector<std::string>& symbols) {
    for (const auto& symbol : symbols)
        loadMinuteFile(folder / (symbol+"_1min.txt"),symbol);
}
double DevHistoricalCloseStore::completedClose(
    const std::string& symbol, std::int64_t t) const {
    const auto it=prices_.find(symbol);
    if (it==prices_.end()) throw std::runtime_error("3.19C: missing price symbol: "+symbol);
    // Candle stamped at 09:15 closes at 09:16. Never use an unfinished candle.
    const auto& series=it->second;
    const auto upper=series.upper_bound(t-60);
    if (upper==series.begin()) throw std::runtime_error("3.19C: no completed candle");
    const auto last=std::prev(upper);
    if (t-last->first>60)
        throw std::runtime_error("3.19C: stale/missing 1-minute candle for "+symbol);
    return last->second;
}
DevHistoricalOOSRunner::DevHistoricalOOSRunner(DevOOSLedgerConfig config)
    : config_(config) {}
DevHistoricalOOSReport DevHistoricalOOSRunner::run(
    const DevHistoricalTrainingDataset& dataset,
    const DevOOSPriceProvider& prices,
    const DevFrozenManagerPolicy& policy) const {
    DevHistoricalTrainingDatasetValidator::validate(dataset);
    ensure(!dataset.records.empty(), "3.19C: empty OOS dataset");
    ensure(static_cast<bool>(prices), "3.19C: missing historical price provider");
    ensure(static_cast<bool>(policy), "3.19C: missing frozen manager policy");
    DevRealHistoricalMultiStockOOS ledger(config_);
    DevHistoricalOOSReport report;
    report.initial_equity=config_.initial_capital;
    std::int64_t previous=0;
    for (const auto& record: dataset.records) {
        ensure(record.decision_time>previous, "3.19C: nonchronological OOS records");
        previous=record.decision_time;
        DevManagerPortfolioState portfolio{};
        portfolio.numerical_integrity=true;
        // IMPORTANT: DevManagerPortfolioState fields must be populated from
        // current ledger by the project's real inference adapter. The policy
        // additionally receives the authoritative ledger state directly.
        auto point=DevRealHistoricalMultiStockOOS::toDecisionPoint(record,portfolio);
        const auto before=ledger.state();
        const auto desired=policy(point,before);
        if (!desired.empty() && desired!=before.held_symbol) {
            bool selected=false;
            for (const auto& candidate: point.candidates)
                selected |= candidate.assignment.symbol==desired;
            ensure(selected, "3.19C: policy selected stock outside Top-5");
        }
        auto execution=ledger.execute(record.decision_time,desired,prices);
        const auto after=ledger.state();
        ensure(std::fabs(after.cash+static_cast<double>(after.quantity)*
               (after.quantity ? prices(after.held_symbol,record.decision_time):0.0)
               -after.equity)<1e-6,"3.19C: equity reconciliation failed");
        ensure(after.quantity>=0 && after.cash>=-1e-7,"3.19C: invalid holdings/cash");
        if (!before.held_symbol.empty() && !after.held_symbol.empty() &&
            before.held_symbol!=after.held_symbol) ++report.rotations;
        report.executed_legs+=execution.executed_legs;
        report.max_drawdown=std::max(report.max_drawdown,execution.drawdown);
        report.steps.push_back({record.decision_time,before.held_symbol,desired,
            after.held_symbol,after.quantity,after.cash,execution});
    }
    report.final_equity=ledger.state().equity;
    report.net_pnl=report.final_equity-report.initial_equity;
    report.net_return=report.net_pnl/report.initial_equity;
    return report;
}
void DevHistoricalOOSRunner::writeCsv(
    const std::filesystem::path& path, const DevHistoricalOOSReport& report) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("3.19C: cannot write CSV");
    out << "decision_time,previous_symbol,desired_symbol,held_symbol,quantity,cash,"
           "equity_before,equity_after,gross_pnl,brokerage,slippage,other_costs,"
           "net_pnl,drawdown,executed_legs\n";
    out << std::setprecision(12);
    for (const auto& s: report.steps) {
        const auto& e=s.execution;
        out << s.decision_time<<','<<s.previous_symbol<<','<<s.desired_symbol<<','
            <<s.held_symbol<<','<<s.quantity<<','<<s.cash<<','<<e.equity_before<<','
            <<e.equity_after<<','<<e.gross_pnl<<','<<e.brokerage<<','<<e.slippage<<','
            <<e.other_costs<<','<<e.net_pnl<<','<<e.drawdown<<','<<e.executed_legs<<'\n';
    }
}
}
