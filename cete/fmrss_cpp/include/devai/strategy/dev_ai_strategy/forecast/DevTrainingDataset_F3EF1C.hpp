#pragma once
#include "devai/strategy/dev_ai_strategy/forecast/DevRealMarketForwardReturns_F3EF1B.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace devai::strategy::f3ef1c {
namespace fs = std::filesystem;
using devai::strategy::f3ef1::MinuteClose;
using devai::strategy::f3ef1::kHorizons;

// These are explicitly named baseline causal statistics, not the full CETE feature-engine output.
// Each timestamp is a 1-minute candle START. Its close is available at epoch_seconds+60.
struct Features {
    std::int64_t epoch{};
    int session{};
    double close{}, mean20{}, std20{}, z20{}, return1{}, nifty_return1{};
};
struct Label {
    int session{};
    std::array<bool,5> valid{};
    std::array<double,5> value{};
};
struct Counts { std::size_t rows{}, skipped_warmup{}, skipped_missing_label{}, skipped_missing_nifty{}, skipped_nontradable{}; };

inline bool finite(double v) { return std::isfinite(v); }

// Strict parsing of F1B's documented 13-column CSV schema.
inline std::unordered_map<std::int64_t,Label> load_labels(const fs::path& path,
                                                          const std::string& symbol) {
    std::ifstream in(path);
    if(!in) throw std::runtime_error("Cannot open labels: "+path.string());
    std::string line;
    if(!std::getline(in,line)) throw std::runtime_error("Empty labels: "+path.string());
    const std::string expected="symbol,epoch_seconds,session_id,valid_1m,return_1m,valid_3m,return_3m,valid_5m,return_5m,valid_10m,return_10m,valid_15m,return_15m";
    if(line!=expected) throw std::runtime_error("Unexpected label schema: "+path.string());
    std::unordered_map<std::int64_t,Label> result;
    while(std::getline(in,line)) {
        if(line.empty()) continue;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string item;
        while(std::getline(ss,item,',')) fields.push_back(item);
        // Trailing empty CSV field is allowed.
        if(fields.size()==12 && !line.empty() && line.back()==',') fields.emplace_back();
        if(fields.size()!=13 || fields[0]!=symbol) throw std::runtime_error("Malformed label row: "+path.string());
        const auto epoch=std::stoll(fields[1]);
        Label label;
        label.session=std::stoi(fields[2]);
        for(int i=0;i<5;++i) {
            const auto& flag=fields[3+2*i];
            if(flag!="0" && flag!="1") throw std::runtime_error("Invalid label flag");
            label.valid[i]=(flag=="1");
            if(label.valid[i]) {
                if(fields[4+2*i].empty()) throw std::runtime_error("Missing valid return");
                label.value[i]=std::stod(fields[4+2*i]);
                if(!finite(label.value[i])) throw std::runtime_error("Non-finite return label");
            } else if(!fields[4+2*i].empty()) throw std::runtime_error("Invalid label has nonempty return");
        }
        if(!result.emplace(epoch,label).second) throw std::runtime_error("Duplicate label timestamp");
    }
    return result;
}

// Calculate causal rolling 20-candle mean and population standard deviation.
// Reset at each trading session and on missing minute gaps; no future candles.
inline std::vector<Features> make_features(const std::vector<MinuteClose>& raw) {
    const auto bars=devai::strategy::f3ef1b::adapt_candles(raw);
    std::vector<Features> out;
    out.reserve(bars.size());
    std::deque<double> window;
    int last_session=-1;
    std::int64_t last_epoch=-1;
    double previous_close=0;
    for(const auto& bar:bars) {
        if(bar.session_id!=last_session || bar.epoch_seconds-last_epoch!=60) {
            window.clear(); previous_close=0;
        }
        if(!finite(bar.close) || bar.close<=0) throw std::runtime_error("Invalid source close");
        const double ret=previous_close>0 ? bar.close/previous_close-1.0 : std::numeric_limits<double>::quiet_NaN();
        window.push_back(bar.close);
        if(window.size()>20) window.pop_front();
        if(window.size()==20 && finite(ret)) {
            double mean=0, variance=0;
            for(double v:window) mean+=v;
            mean/=20.0;
            for(double v:window) variance+=(v-mean)*(v-mean);
            variance/=20.0;
            const double sd=std::sqrt(variance);
            out.push_back({bar.epoch_seconds,bar.session_id,bar.close,mean,sd,
                           sd>1e-12?(bar.close-mean)/sd:0.0,ret,0.0});
        }
        last_session=bar.session_id;
        last_epoch=bar.epoch_seconds;
        previous_close=bar.close;
    }
    return out;
}

// NIFTY's own 1-minute return at the SAME candle-start timestamp is available
// at the same decision time (candle close). Missing NIFTY observations are rejected.
inline std::unordered_map<std::int64_t,double> make_nifty_returns(const std::vector<MinuteClose>& raw) {
    const auto bars=devai::strategy::f3ef1b::adapt_candles(raw);
    std::unordered_map<std::int64_t,double> result;
    int last_session=-1;
    std::int64_t last_epoch=-1;
    double previous_close=0;
    for(const auto& b:bars) {
        if(b.session_id!=last_session || b.epoch_seconds-last_epoch!=60) previous_close=0;
        if(previous_close>0 && finite(b.close) && b.close>0)
            result.emplace(b.epoch_seconds,b.close/previous_close-1.0);
        last_session=b.session_id; last_epoch=b.epoch_seconds; previous_close=b.close;
    }
    return result;
}

// No forward labels are required to be valid simultaneously: each horizon retains
// its own mask, allowing horizon-specific training without silently dropping rows.
inline Counts write_dataset(const fs::path& path, const std::string& symbol,
    const std::vector<MinuteClose>& bars,
    const std::unordered_map<std::int64_t,Label>& labels,
    const std::unordered_map<std::int64_t,double>& nifty_returns) {
    if(symbol=="NIFTY%2050" || symbol=="NIFTY 50") throw std::runtime_error("Benchmark is not a tradable stock");
    const auto features=make_features(bars);
    if(!path.parent_path().empty()) fs::create_directories(path.parent_path());
    std::ofstream out(path);
    if(!out) throw std::runtime_error("Cannot write: "+path.string());
    out << "symbol,candle_start_epoch,decision_epoch,session_id,close,mean20,std20,z20,return_1m_past,nifty_return_1m_past";
    for(int h:kHorizons) out<<",valid_"<<h<<"m,target_return_"<<h<<"m";
    out<<'\n'<<std::setprecision(17);
    Counts counts;
    counts.skipped_warmup=bars.size()-features.size();
    for(const auto& f:features) {
        const auto n=nifty_returns.find(f.epoch);
        if(n==nifty_returns.end()) {++counts.skipped_missing_nifty;continue;}
        const auto l=labels.find(f.epoch);
        if(l==labels.end() || l->second.session!=f.session) {++counts.skipped_missing_label;continue;}
        const auto& label=l->second;
        // Conservative: at least one target horizon must be valid.
        bool any=false;
        for(bool v:label.valid) any|=v;
        if(!any) {++counts.skipped_missing_label;continue;}
        if(!finite(n->second)) throw std::runtime_error("Non-finite NIFTY feature");
        out<<symbol<<','<<f.epoch<<','<<f.epoch+60<<','<<f.session<<','
           <<f.close<<','<<f.mean20<<','<<f.std20<<','<<f.z20<<','<<f.return1<<','<<n->second;
        for(int i=0;i<5;++i) {
            out<<','<<(label.valid[i]?1:0)<<',';
            if(label.valid[i]) out<<label.value[i];
        }
        out<<'\n'; ++counts.rows;
    }
    if(!out) throw std::runtime_error("Failed writing: "+path.string());
    return counts;
}
} // namespace devai::strategy::f3ef1c
