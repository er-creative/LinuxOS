#include "devai/strategy/dev_ai_strategy/DevRealMarketReplayAdapter.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ctime>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    const auto path=std::filesystem::temp_directory_path()/"cete_real_replay_test_1min.txt";
    {
        std::ofstream out(path);
        out<<"Symbol,Date,Time,Open,High,Low,Close,Volume\n";
        out<<"RELIANCE,2026-09-01,09:15:00,100,102,99,101,1000\n";
        out<<"RELIANCE,2026-09-01,09:16:00,105,107,104,106,1000\n";
        out<<"RELIANCE,2026-09-01,09:17:00,108,110,107,109,1000\n";
    }
    DevRealMinuteMarket market;
    market.load("RELIANCE",path);
    assert(market.barCount("RELIANCE")==3);
    bool missing=false;
    try {(void)market.nextExecutionBar("RELIANCE",0);} catch(const std::out_of_range&) {missing=true;}
    assert(missing);
    // 09:16 IST is the first decision boundary after the 09:15 bar completes.
    // Exact UTC epoch corresponding to 2026-09-01 09:16 IST.
    std::tm tm{};tm.tm_year=2026-1900;tm.tm_mon=8;tm.tm_mday=1;
    tm.tm_hour=9;tm.tm_min=16;
    const auto t=static_cast<std::int64_t>(timegm(&tm))-19800;
    assert(market.completedClose("RELIANCE",t)==101.0);
    assert(market.nextExecutionBar("RELIANCE",t).open==105.0);
    bool premature=false;
    try {(void)market.completedClose("RELIANCE",t-60);} catch(const std::out_of_range&) {premature=true;}
    assert(premature);
    std::filesystem::remove(path);
    std::cout<<"PASS real minute replay adapter: IST timestamps, completed-only marks, next-open fills, early-data rejection\n";
}
