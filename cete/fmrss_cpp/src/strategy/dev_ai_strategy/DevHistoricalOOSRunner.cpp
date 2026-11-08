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
    std::map<std::int64_t,double> parsed, opening;
    std::string line;
    while (std::getline(in,line)) {
        if (line.empty()) continue;
        auto v=split(line);
        if (v.size()<8) throw std::runtime_error("3.19C: malformed row in " + file.string());
        if (v[0]=="Symbol") continue;
        if (v[0]!=symbol) throw std::runtime_error("3.19C: symbol mismatch in " + file.string());
        const auto t=parseIST(v[1],v[2]);
        const double open=std::stod(v[3]);
        ensure(std::isfinite(open) && open>0, "3.19C: invalid open");
        const double close=std::stod(v[6]);
        ensure(std::isfinite(close) && close>0, "3.19C: invalid close");
        opening.emplace(t,open);
        if (!parsed.emplace(t,close).second)
            throw std::runtime_error("3.19C: duplicate candle timestamp");
    }
    ensure(!parsed.empty(), "3.19C: no candles loaded");
    prices_[symbol]=std::move(parsed);
    opens_[symbol]=std::move(opening);
}
void DevHistoricalCloseStore::loadMinuteFolder(
    const std::filesystem::path& folder, const std::vector<std::string>& symbols) {
    for (const auto& symbol : symbols)
        loadMinuteFile(folder / (symbol+"_1min.txt"),symbol);
}
double DevHistoricalCloseStore::openingPrice(
    const std::string& symbol, std::int64_t t) const {
    const auto it=opens_.find(symbol);
    if (it==opens_.end()) throw std::runtime_error("3.19C: missing open symbol: "+symbol);
    const auto bar=it->second.find(t);

    if (bar == it->second.end()) {
        using namespace std::chrono;

        const auto ist = sys_seconds{seconds{t}} + minutes{330};
        const auto day = floor<days>(ist);
        const year_month_day ymd{day};

        const hh_mm_ss time{ist - day};

        std::ostringstream msg;
        msg << "3.19C: missing exact decision candle open"
            << " symbol=" << symbol
            << " epoch=" << t
            << " IST="
            << int(ymd.year()) << '-'
            << std::setw(2) << std::setfill('0')
            << unsigned(ymd.month()) << '-'
            << std::setw(2) << unsigned(ymd.day()) << ' '
            << std::setw(2) << time.hours().count() << ':'
            << std::setw(2) << time.minutes().count();

        throw std::runtime_error(msg.str());
    }


    return bar->second;
}

double DevHistoricalCloseStore::completedClose(
    const std::string& symbol, std::int64_t t) const {
    const auto it=prices_.find(symbol);
    if (it==prices_.end()) throw std::runtime_error("3.19C: missing price symbol: "+symbol);
    // Candle stamped at 09:15 closes at 09:16. Never use an unfinished candle.
    const auto& series=it->second;
    // The candle stamped t-60 completes exactly at decision time t.
    // Never silently substitute an older bar when this candle is missing.
    const auto last=series.find(t-60);
    if(last==series.end())
        throw std::runtime_error("3.19C: missing exact completed 1-minute candle for "+symbol);
    return last->second;
}
DevHistoricalOOSRunner::DevHistoricalOOSRunner(DevOOSLedgerConfig config)
    : DevHistoricalOOSRunner(config, DevHistoricalRiskLimits_H47_6C1G2B1{}) {}
DevHistoricalOOSRunner::DevHistoricalOOSRunner(
    DevOOSLedgerConfig config, DevHistoricalRiskLimits_H47_6C1G2B1 risk_limits)
    : config_(config), risk_limits_(risk_limits) {}
DevHistoricalOOSReport DevHistoricalOOSRunner::run(
    const DevHistoricalTrainingDataset& dataset,
    const DevOOSPriceProvider& prices,
    const DevFrozenManagerPolicy& policy) const {
    DevHistoricalTrainingDatasetValidator::validate(dataset);
    ensure(!dataset.records.empty(), "3.19C: empty OOS dataset");
    ensure(static_cast<bool>(prices), "3.19C: missing historical price provider");
    ensure(static_cast<bool>(policy), "3.19C: missing frozen manager policy");
    if (config_.allow_intraday_short)
        throw std::invalid_argument("6C1G2B1: cash-equity replay is LONG only");
    if (config_.short_square_off_hour_ist != 15 || config_.short_square_off_minute_ist != 15)
        throw std::invalid_argument("6C1G2B1: mandatory 15:15 IST cutoff");
    DevRealHistoricalMultiStockOOS ledger(config_);
    DevHistoricalExecutionRiskGate_H47_6C1G2B1 risk_gate(risk_limits_);
    DevHistoricalOOSReport report;
    report.initial_equity=config_.initial_capital;
    std::int64_t previous=0;
    for (const auto& record: dataset.records) {
        ensure(record.decision_time>previous, "3.19C: nonchronological OOS records");
        previous=record.decision_time;
        const auto before=ledger.state();
        const double marked_price = before.quantity != 0
            ? prices(before.held_symbol, record.decision_time) : 0.0;
        const auto portfolio=DevRealHistoricalMultiStockOOS::portfolioAt(
            before, marked_price, record.decision_time, config_.initial_capital);
        auto point=DevRealHistoricalMultiStockOOS::toDecisionPoint(record,portfolio);
        const double marked_equity = before.cash +
            static_cast<double>(before.quantity) * marked_price;
        const bool risk_permits = risk_gate.observe(record.decision_time, marked_equity);
        auto desired = risk_permits ? policy(point,before) : std::string{};
        if (desired.rfind("SHORT:", 0) == 0)
            throw std::invalid_argument("6C1G2B1: SHORT policy instruction forbidden");
        // Enforce session and entry cutoff independently of the policy.
        const auto local_minutes=((record.decision_time+19800)%86400+86400)%86400/60;
        const auto cutoff=config_.short_square_off_hour_ist*60+config_.short_square_off_minute_ist;
        if (local_minutes >= cutoff || local_minutes < 9*60+15 ||
            (local_minutes >= 14*60+45 && desired != before.held_symbol))
            desired.clear();
        const std::string target_symbol = desired;
        if (!target_symbol.empty() && target_symbol!=before.held_symbol) {
            bool selected=false;
            for (const auto& candidate: point.candidates)
                selected |= candidate.assignment.symbol==target_symbol;
            ensure(selected, "3.19C: policy selected stock outside Top-5");
        }
        auto execution=ledger.execute(record.decision_time,desired,prices);
        const auto after=ledger.state();
        ensure(std::fabs(after.cash+static_cast<double>(after.quantity)*
               (after.quantity ? prices(after.held_symbol,record.decision_time):0.0)
               -after.equity)<1e-6,"3.19C: equity reconciliation failed");
        ensure(after.cash-after.restricted_short_proceeds>=-1e-7,"3.19C: invalid holdings/cash");
        ensure(after.quantity >= 0, "6C1G2B1: negative cash-equity position");
        ensure(std::fabs(execution.net_pnl - (execution.gross_pnl -
               execution.brokerage - execution.slippage - execution.other_costs)) < 1e-6,
               "6C1G2B1: step P&L/cost mismatch");
        ensure(std::fabs(execution.equity_after - execution.equity_before -
               execution.net_pnl) < 1e-6,
               "6C1G2B1: step equity/P&L mismatch");
        if (local_minutes >= cutoff)
            ensure(after.quantity == 0, "6C1G2B1: mandatory liquidation failed");
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
