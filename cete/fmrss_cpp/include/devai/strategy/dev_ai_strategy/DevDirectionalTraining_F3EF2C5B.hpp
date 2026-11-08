#pragma once
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace devai::strategy::f3ef2c5b {
inline constexpr std::size_t WAIT=0, KEEP=1, FIRST_AGENT=2, LAST_AGENT=6;
inline constexpr int ENTRY_CUTOFF_IST=14*60+45, FLAT_CUTOFF_IST=15*60+15;
inline int minuteIST(std::int64_t utc_seconds) noexcept {
    auto s=(utc_seconds+19800)%86400;
    if(s<0) s+=86400;
    return static_cast<int>(s/60);
}
inline bool mayEnter(std::int64_t t) noexcept {
    const int m=minuteIST(t);
    return m>=9*60+15 && m<ENTRY_CUTOFF_IST;
}
inline bool mustFlatten(std::int64_t t) noexcept { return minuteIST(t)>=FLAT_CUTOFF_IST; }
inline std::array<bool,4> agentMask(bool invested, bool shortable, bool allow_short, bool entries_allowed) noexcept {
    std::array<bool,4> m{};
    m[0]=true; // HOLD/WAIT
    m[1]=!invested && entries_allowed;
    m[2]=!invested && entries_allowed && allow_short && shortable;
    m[3]=invested;
    return m;
}
inline std::string targetForEntry(const std::string& symbol, std::size_t action) {
    if(symbol.empty()) throw std::invalid_argument("empty candidate symbol");
    if(action==static_cast<std::size_t>(DevAction::LONG)) return symbol;
    if(action==static_cast<std::size_t>(DevAction::SHORT)) return "SHORT:"+symbol;
    throw std::invalid_argument("not a directional entry action");
}
inline std::string preserveTarget(const std::string& symbol, std::int64_t quantity) {
    if(quantity==0) return {};
    if(symbol.empty()) throw std::invalid_argument("held position lacks symbol");
    return quantity<0?"SHORT:"+symbol:symbol;
}
inline double markedEquity(double cash, std::int64_t signed_quantity, double completed_close) {
    return cash+static_cast<double>(signed_quantity)*completed_close;
}
}
