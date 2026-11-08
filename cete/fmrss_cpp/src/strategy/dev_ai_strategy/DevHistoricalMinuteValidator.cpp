#include "devai/strategy/dev_ai_strategy/DevHistoricalMinuteValidator.hpp"
#include <algorithm>
#include <cctype>
#include <iterator>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
namespace {
// Gregorian civil date -> ordinal days, independent of local timezone/DST.
long long civilDays(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + doe;
}
bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
bool parseDate(const std::string& s, long long& day) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    try {
        const int y = std::stoi(s.substr(0,4));
        const int m = std::stoi(s.substr(5,2));
        const int d = std::stoi(s.substr(8,2));
        if (y < 1900 || y > 2200 || m < 1 || m > 12 || d < 1) return false;
        static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (d > days[m] + (m == 2 && leap(y) ? 1 : 0)) return false;
        day = civilDays(y, static_cast<unsigned>(m), static_cast<unsigned>(d));
        return true;
    } catch (...) { return false; }
}
bool parseTime(const std::string& s, int& minute) {
    if ((s.size() != 5 && s.size() != 8) || s[2] != ':' || (s.size() == 8 && s[5] != ':')) return false;
    try {
        const int h = std::stoi(s.substr(0,2));
        const int m = std::stoi(s.substr(3,2));
        const int sec = s.size() == 8 ? std::stoi(s.substr(6,2)) : 0;
        if (h < 0 || h > 23 || m < 0 || m > 59 || sec != 0) return false;
        minute = h * 60 + m;
        return true;
    } catch (...) { return false; }
}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream stream(line);
    std::string cell;
    while (std::getline(stream, cell, ',')) {
        const auto begin = cell.find_first_not_of(" \t\r\n");
        const auto end = cell.find_last_not_of(" \t\r\n");
        out.push_back(begin == std::string::npos ? "" : cell.substr(begin, end - begin + 1));
    }
    return out;
}
bool number(const std::string& s, double& x) {
    try { std::size_t n = 0; x = std::stod(s, &n); return n == s.size() && std::isfinite(x); }
    catch (...) { return false; }
}
using MinuteSet = std::set<long long>;
struct Loaded {
    MinuteSet minutes;
    std::set<long long> days;
    std::map<long long, std::pair<int,int>> session_by_day;
    DevHistoricalSymbolValidation report;
};
std::pair<int,int> sessionFor(const std::string& date, const DevHistoricalMinuteValidationConfig& config) {
    const auto it = config.session_overrides.find(date);
    return it == config.session_overrides.end()
        ? std::make_pair(config.session_start_minute, config.session_end_minute) : it->second;
}
Loaded load(const DevHistoricalMinuteInput& input, const DevHistoricalMinuteValidationConfig& config) {
    std::ifstream file(input.file_path);
    if (!file) throw std::runtime_error("Cannot open: " + input.file_path);
    Loaded result;
    result.report.symbol = input.symbol;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const auto columns = split(line);
        if (!columns.empty() && columns[0] == "Symbol") continue;
        ++result.report.rows;
        long long day = 0;
        int minute = 0;
        double o = 0, h = 0, l = 0, c = 0, v = 0;
        const auto session = columns.size() >= 2 ? sessionFor(columns[1], config)
                                                  : std::make_pair(config.session_start_minute, config.session_end_minute);
        if (columns.size() != 8 || columns[0] != input.symbol ||
            !parseDate(columns[1], day) || !parseTime(columns[2], minute) ||
            !number(columns[3], o) || !number(columns[4], h) ||
            !number(columns[5], l) || !number(columns[6], c) ||
            !number(columns[7], v) || o <= 0 || h <= 0 || l <= 0 || c <= 0 || v < 0 ||
            h < std::max(o,c) || l > std::min(o,c) || h < l ||
            (config.reject_out_of_session && (minute < session.first || minute >= session.second))) {
            ++result.report.invalid_rows;
            continue;
        }
        const long long key = day * 1440 + minute;
        if (!result.minutes.insert(key).second) ++result.report.duplicate_minutes;
        result.days.insert(day);
        result.session_by_day[day] = session;
    }
    for (const auto day : result.days) {
        const auto session = result.session_by_day.at(day);
        std::size_t missing = 0;
        for (int minute = session.first; minute < session.second; ++minute)
            if (!result.minutes.count(day * 1440 + minute)) ++missing;
        result.report.missing_session_minutes += missing;
        if (missing) ++result.report.partial_sessions;
        else ++result.report.complete_sessions;
    }
    return result;
}
MinuteSet completeWindows(const Loaded& loaded, int width) {
    MinuteSet windows;
    for (const auto day : loaded.days) {
        const auto session = loaded.session_by_day.at(day);
        for (int start = session.first; start + width <= session.second; start += width) {
            bool complete = true;
            for (int offset = 0; offset < width; ++offset)
                if (!loaded.minutes.count(day * 1440 + start + offset)) { complete = false; break; }
            // Window key represents completion boundary, never its first candle.
            if (complete) windows.insert(day * 1440 + start + width);
        }
    }
    return windows;
}
} // namespace
DevHistoricalMinuteValidator::DevHistoricalMinuteValidator(DevHistoricalMinuteValidationConfig config)
    : config_(std::move(config)) {
    for (const auto& [date, session] : config_.session_overrides) {
        long long day = 0;
        if (!parseDate(date, day) || session.first < 0 || session.second > 1440 ||
            session.second <= session.first || (session.second - session.first) % 15 != 0 ||
            session.first % 5 != 0)
            throw std::invalid_argument("Invalid special-session override: " + date);
    }
    if (config_.benchmark_symbol.empty() || config_.session_start_minute < 0 ||
        config_.session_end_minute > 1440 || config_.session_end_minute <= config_.session_start_minute ||
        (config_.session_end_minute - config_.session_start_minute) % 15 != 0)
        throw std::invalid_argument("Invalid benchmark or session configuration");
}
DevHistoricalMinuteValidationReport DevHistoricalMinuteValidator::validate(
    const std::vector<DevHistoricalMinuteInput>& inputs) const {
    if (inputs.size() < 2) throw std::invalid_argument("Require benchmark and at least one stock");
    std::unordered_set<std::string> names;
    bool benchmark = false;
    for (const auto& input : inputs) {
        if (input.symbol.empty() || input.file_path.empty() || !names.insert(input.symbol).second)
            throw std::invalid_argument("Invalid or duplicate input symbol/path");
        benchmark |= input.symbol == config_.benchmark_symbol;
    }
    if (!benchmark) throw std::invalid_argument("Benchmark input is missing");
    DevHistoricalMinuteValidationReport report;
    bool first = true;
    MinuteSet common5, common15;
    for (const auto& input : inputs) {
        auto loaded = load(input, config_);
        auto windows5 = completeWindows(loaded, 5);
        auto windows15 = completeWindows(loaded, 15);
        loaded.report.complete_5m_windows = windows5.size();
        loaded.report.complete_15m_windows = windows15.size();
        const auto& r = loaded.report;
        if (r.rows == 0 || r.invalid_rows || r.duplicate_minutes) {
            report.issues.push_back(input.symbol + ": empty input, invalid rows or duplicate minutes");
        }
        if (r.missing_session_minutes)
            report.issues.push_back(input.symbol + ": partial sessions (" +
                std::to_string(r.missing_session_minutes) + " missing expected minutes)");
        if (first) { common5 = std::move(windows5); common15 = std::move(windows15); first = false; }
        else {
            MinuteSet intersection5, intersection15;
            std::set_intersection(common5.begin(),common5.end(),windows5.begin(),windows5.end(),
                                  std::inserter(intersection5,intersection5.end()));
            std::set_intersection(common15.begin(),common15.end(),windows15.begin(),windows15.end(),
                                  std::inserter(intersection15,intersection15.end()));
            common5 = std::move(intersection5);
            common15 = std::move(intersection15);
        }
        report.symbols.push_back(std::move(loaded.report));
    }
    report.synchronized_5m_windows = common5.size();
    report.synchronized_15m_windows = common15.size();
    if (!report.synchronized_5m_windows || !report.synchronized_15m_windows)
        report.issues.push_back("No fully synchronized completed 5m/15m windows across universe");
    report.all_sessions_complete = true;
    bool structurally_valid = true;
    for (const auto& r : report.symbols) {
        if (r.rows == 0 || r.invalid_rows || r.duplicate_minutes) structurally_valid = false;
        if (r.missing_session_minutes) report.all_sessions_complete = false;
    }
    report.passed = structurally_valid && report.synchronized_5m_windows > 0 &&
                    report.synchronized_15m_windows > 0;
    return report;
}
} // namespace devai::strategy
