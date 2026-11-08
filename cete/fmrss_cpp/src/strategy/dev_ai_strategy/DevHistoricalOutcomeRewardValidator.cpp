#include "devai/strategy/dev_ai_strategy/DevHistoricalOutcomeRewardValidator.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace devai::strategy {
namespace {
struct Bar { std::int64_t time{}; double open{}, close{}; };
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> v; std::stringstream ss(line); std::string field;
    while (std::getline(ss, field, ',')) {
        auto a = field.find_first_not_of(" \t\r\n\"");
        auto b = field.find_last_not_of(" \t\r\n\"");
        v.push_back(a == std::string::npos ? "" : field.substr(a, b - a + 1));
    }
    return v;
}
std::int64_t epochIST(const std::string& date, const std::string& time) {
    std::tm tm{};
    std::istringstream in(date + " " + time);
    in >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (in.fail()) {
        std::istringstream alt(date + " " + time);
        alt >> std::get_time(&tm, "%Y-%m-%d %H:%M");
        if (alt.fail()) throw std::runtime_error("Invalid candle date/time: " + date + " " + time);
    }
    tm.tm_isdst = 0;
    // CSV timestamps are Indian market wall-clock times (IST = UTC+05:30).
    return static_cast<std::int64_t>(timegm(&tm)) - 19800;
}
std::string dayIST(std::int64_t epoch) {
    std::time_t t = static_cast<std::time_t>(epoch + 19800);
    std::tm tm{}; gmtime_r(&t, &tm);
    char buf[16]; std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}
std::vector<Bar> loadBars(const std::filesystem::path& path, const std::string& symbol) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open historical data: " + path.string());
    std::vector<Bar> bars; std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto f = split(line);
        if (f.size() < 7 || f[0] == "Symbol" || f[0] == "symbol") continue;
        if (f[0] != symbol) throw std::runtime_error("Symbol mismatch in " + path.string());
        Bar b;
        b.time = epochIST(f[1], f[2]);
        b.open = std::stod(f[3]);
        b.close = std::stod(f[6]);
        if (!std::isfinite(b.open) || !std::isfinite(b.close) || b.open <= 0 || b.close <= 0)
            throw std::runtime_error("Invalid OHLC prices: " + path.string());
        if (!bars.empty() && b.time <= bars.back().time)
            throw std::runtime_error("Unsorted/duplicate timestamps: " + path.string());
        bars.push_back(b);
    }
    if (bars.empty()) throw std::runtime_error("No 1m candles: " + path.string());
    return bars;
}
struct Eval { bool available{false}; bool gap{false}; bool session{false};
              std::int64_t exit_time{0}; double entry{0}, exit{0}, gross{0}; };
Eval evaluate(const std::vector<Bar>& bars, std::int64_t decision, std::size_t horizon) {
    Eval e;
    // A decision at 11:00 may execute at the OPEN of the 11:00 candle,
    // never at the 10:59 close unless explicitly modelled as executable.
    auto it = std::lower_bound(bars.begin(), bars.end(), decision,
        [](const Bar& b, std::int64_t t) { return b.time < t; });
    if (it == bars.end() || it->time != decision) return e;
    const auto pos = static_cast<std::size_t>(it - bars.begin());
    if (horizon == 0 || horizon > bars.size() - pos) return e;
    const auto& exit = bars[pos + horizon - 1];
    const auto day = dayIST(decision);
    if (dayIST(exit.time) != day) { e.session = true; return e; }
    for (std::size_t i = pos; i < pos + horizon; ++i) {
        if (dayIST(bars[i].time) != day) { e.session = true; return e; }
        if (bars[i].time != decision + static_cast<std::int64_t>(60 * (i - pos))) {
            e.gap = true; return e;
        }
    }
    e.available = true; e.entry = it->open; e.exit = exit.close;
    e.exit_time = exit.time + 60;
    e.gross = e.exit / e.entry - 1.0;
    return e;
}
} // namespace
DevHistoricalOutcomeRewardValidator::DevHistoricalOutcomeRewardValidator(DevOutcomeRewardConfig config)
    : config_(std::move(config)) {
    if (config_.minute_data_folder.empty() || config_.horizons.empty() ||
        config_.initial_capital <= 0 || config_.maximum_exposure_fraction <= 0 ||
        config_.maximum_exposure_fraction > 1 || config_.transaction_cost_bps_per_side < 0 ||
        config_.slippage_bps_per_side < 0)
        throw std::invalid_argument("Invalid historical outcome/reward configuration");
    std::sort(config_.horizons.begin(), config_.horizons.end());
    config_.horizons.erase(std::unique(config_.horizons.begin(), config_.horizons.end()), config_.horizons.end());
    if (config_.horizons.front() == 0) throw std::invalid_argument("Horizon must be positive");
}
DevOutcomeRewardStats DevHistoricalOutcomeRewardValidator::enrich(
    DevHistoricalTrainingDataset& dataset, const std::filesystem::path& report_csv) const {
    DevHistoricalTrainingDatasetValidator::validate(dataset);
    if (report_csv.has_parent_path()) std::filesystem::create_directories(report_csv.parent_path());
    std::ofstream out(report_csv);
    if (!out) throw std::runtime_error("Cannot create report " + report_csv.string());
    out << "decision_time,agent,symbol,horizon_bars,available,entry_time,exit_time,entry_open,exit_close,gross_return,long_reward_rupees,short_reward_rupees,reason\n";
    out << std::setprecision(15);
    DevOutcomeRewardStats stats; stats.records = dataset.records.size();
    std::unordered_map<std::string, std::vector<Bar>> cache;
    for (auto& record : dataset.records) {
        for (auto& candidate : record.candidates) {
            ++stats.candidates;
            auto found = cache.find(candidate.symbol);
            if (found == cache.end()) {
                auto path = config_.minute_data_folder / (candidate.symbol + "_1min.txt");
                found = cache.emplace(candidate.symbol, loadBars(path, candidate.symbol)).first;
            }
            candidate.outcomes.clear();
            for (const auto horizon : config_.horizons) {
                const auto e = evaluate(found->second, record.decision_time, horizon);
                DevHistoricalOutcome target;
                target.horizon_bars = horizon;
                target.available = e.available;
                // Availability is separate from integrity. An unavailable outcome with
                // zero placeholders is a valid, explicitly missing target.
                target.numerical_integrity = true;
                target.outcome_time = e.available ? e.exit_time : 0;
                target.forward_return = e.available ? e.gross : 0.0;
                candidate.outcomes.push_back(target);
                std::string reason = "ok";
                if (!e.available) {
                    ++stats.unavailable;
                    if (e.gap) { ++stats.rejected_gaps; reason = "missing_minute"; }
                    else if (e.session) { ++stats.rejected_session; reason = "cross_session"; }
                    else { ++stats.rejected_history; reason = "missing_entry_or_future"; }
                } else ++stats.available;
                // One-position-at-a-time counterfactual rewards. Not summed as a portfolio.
                const double budget = config_.initial_capital * config_.maximum_exposure_fraction;
                const double quantity = e.available ?
                    (config_.allow_fractional_shares ? budget / e.entry : std::floor(budget / e.entry)) : 0;
                const double gross_pnl = quantity * (e.exit - e.entry);
                const double gross_short = -gross_pnl;
                const double turnover = quantity * (e.entry + e.exit);
                const double fee = turnover * (config_.transaction_cost_bps_per_side +
                                               config_.slippage_bps_per_side) / 10000.0;
                const double long_reward = e.available ? gross_pnl - fee : 0.0;
                const double short_reward = e.available ? gross_short - fee : 0.0;
                out << record.decision_time << ',' << candidate.agent_name << ',' << candidate.symbol
                    << ',' << horizon << ',' << (e.available ? 1 : 0) << ','
                    << (e.available ? record.decision_time : 0) << ',' << e.exit_time << ','
                    << e.entry << ',' << e.exit << ',' << e.gross << ','
                    << long_reward << ',' << short_reward << ',' << reason << '\n';
            }
        }
    }
    DevHistoricalTrainingDatasetValidator::validate(dataset);
    return stats;
}
} // namespace devai::strategy
