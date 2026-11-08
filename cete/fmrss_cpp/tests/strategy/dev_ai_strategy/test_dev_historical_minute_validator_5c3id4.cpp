#include "devai/strategy/dev_ai_strategy/DevHistoricalMinuteValidator.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace devai::strategy;
namespace fs = std::filesystem;
static void writeData(const fs::path& path, const std::string& symbol, int omit = -1) {
    std::ofstream out(path);
    out << "Symbol,Date,Time,Open,High,Low,Close,Volume\n";
    for (int m = 9 * 60 + 15; m < 15 * 60 + 30; ++m) {
        if (m == omit) continue;
        const int h = m / 60, min = m % 60;
        out << symbol << ",2026-10-12,";
        if (h < 10) out << '0';
        out << h << ':';
        if (min < 10) out << '0';
        out << min << ":00,100,101,99,100,1000\n";
    }
}
int main() {
    const fs::path root = fs::temp_directory_path() / "cete_5c3id4_test";
    fs::create_directories(root);
    const auto bench = root / "benchmark.txt";
    const auto stock = root / "stock.txt";
    writeData(bench, "NIFTY 50");
    writeData(stock, "RELIANCE");
    const std::vector<DevHistoricalMinuteInput> inputs{{"NIFTY 50",bench.string()}, {"RELIANCE",stock.string()}};
    DevHistoricalMinuteValidator validator;
    auto clean = validator.validate(inputs);
    assert(clean.passed);
    assert(clean.all_sessions_complete);
    assert(clean.synchronized_5m_windows == 75);
    assert(clean.synchronized_15m_windows == 25);
    writeData(stock, "RELIANCE", 9 * 60 + 20);
    auto broken = validator.validate(inputs);
    assert(broken.passed);
    assert(!broken.all_sessions_complete);
    assert(broken.symbols[1].missing_session_minutes == 1);
    assert(broken.synchronized_5m_windows == 74);
    assert(broken.synchronized_15m_windows == 24);
    // Explicit special session: 13:45 to 14:45, not a normal full trading day.
    auto writeSpecial = [](const fs::path& path, const std::string& symbol) {
        std::ofstream out(path);
        for (int minute = 13 * 60 + 45; minute < 14 * 60 + 45; ++minute) {
            const int hh = minute / 60, mm = minute % 60;
            out << symbol << ",2025-10-21,";
            if (hh < 10) out << '0';
            out << hh << ':';
            if (mm < 10) out << '0';
            out << mm << ",100,101,99,100,0.0\n";
        }
    };
    writeSpecial(bench, "NIFTY 50");
    writeSpecial(stock, "RELIANCE");
    const auto special = validator.validate(inputs);
    assert(special.passed && special.all_sessions_complete);
    assert(special.synchronized_5m_windows == 12);
    assert(special.synchronized_15m_windows == 4);
    assert(special.symbols[0].missing_session_minutes == 0);
    fs::remove_all(root);
    std::cout << "PASS 5C-3I-D4 real minute validation contract\n";
}
