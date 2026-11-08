#pragma once
#include "devai/strategy/dev_ai_strategy/forecast/DevForwardReturnLabels_F3EF1.hpp"
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace devai::strategy::f3ef1b {
namespace fs = std::filesystem;
using devai::strategy::f3ef1::MinuteClose;
using devai::strategy::f3ef1::LabelRow;

// Exchange-local date independent of host TZ: India has UTC+05:30 year-round.
inline int ist_session_id(std::int64_t epoch) {
    const std::time_t t = static_cast<std::time_t>(epoch + 19800);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    if (!gmtime_r(&t, &tm)) throw std::runtime_error("UTC time conversion failed");
#endif
    return (tm.tm_year + 1900)*10000 + (tm.tm_mon+1)*100 + tm.tm_mday;
}
inline int ist_minute_of_day(std::int64_t epoch) {
    std::int64_t s=(epoch+19800)%86400;
    if(s<0) s+=86400;
    return static_cast<int>(s/60);
}
inline std::vector<MinuteClose> adapt_candles(const std::vector<MinuteClose>& input) {
    auto bars=input;
    std::sort(bars.begin(),bars.end(),[](const auto& a,const auto& b){return a.epoch_seconds<b.epoch_seconds;});
    for (std::size_t i=0;i<bars.size();++i) {
        if(i && bars[i].epoch_seconds==bars[i-1].epoch_seconds)
            throw std::runtime_error("duplicate timestamp; repair source data first");
        // NSE regular cash session: 09:15..15:29 candle start; 15:30 excluded.
        const int minute=ist_minute_of_day(bars[i].epoch_seconds);
        if (minute<555 || minute>=930)
            throw std::runtime_error("non-session candle in input; filter before adapt_candles");
        bars[i].session_id=ist_session_id(bars[i].epoch_seconds);
    }
    return bars;
}
inline std::vector<LabelRow> generate(const std::vector<MinuteClose>& bars) {
    return devai::strategy::f3ef1::make_forward_return_labels(adapt_candles(bars));
}
inline void write_csv(const fs::path& path, const std::string& symbol,
                      const std::vector<LabelRow>& rows) {
    if(!path.parent_path().empty()) fs::create_directories(path.parent_path());
    std::ofstream out(path);
    if(!out) throw std::runtime_error("Cannot write: "+path.string());
    out << "symbol,epoch_seconds,session_id";
    for(int h: devai::strategy::f3ef1::kHorizons) out << ",valid_"<<h<<"m,return_"<<h<<"m";
    out << '\n' << std::setprecision(17);
    for(const auto& r: rows) {
        out << symbol << ',' << r.epoch_seconds << ',' << r.session_id;
        for(const auto& h: r.horizons) {
            out << ',' << (h.valid?1:0) << ',';
            if(h.valid) out << h.simple_return;
        }
        out << '\n';
    }
    if(!out) throw std::runtime_error("CSV write failed: "+path.string());
}
inline std::vector<std::string> read_symbols(const fs::path& path) {
    std::ifstream in(path);
    if(!in) throw std::runtime_error("Cannot open symbol universe: "+path.string());
    std::vector<std::string> symbols;
    std::string line;
    while(std::getline(in,line)) {
        const auto comment=line.find('#');
        if(comment!=std::string::npos) line.resize(comment);
        std::istringstream words(line);
        std::string s;
        while(words>>s) {
            if(!s.empty() && s.back()==',') s.pop_back();
            if(s.empty()) continue;
            if(std::find(symbols.begin(),symbols.end(),s)==symbols.end()) symbols.push_back(s);
        }
    }
    if(symbols.empty()) throw std::runtime_error("Empty universe: "+path.string());
    return symbols;
}
} // namespace devai::strategy::f3ef1b
