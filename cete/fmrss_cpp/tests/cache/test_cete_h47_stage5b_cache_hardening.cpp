#include "devai/cache/HistoricalMathematicalFeatureCache.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Cache = devai::cache::HistoricalMathematicalFeatureCache;

int main() {
    const auto root = fs::temp_directory_path() /
        ("cete_stage5b_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code ec; fs::remove_all(p, ec); } } cleanup{root};
    fs::create_directories(root);
    const auto source = root / "source.txt";
    { std::ofstream os(source); os << "2026-10-16,10:30,100\n"; }
    const auto fp1 = Cache::fingerprint({source});
    { std::ofstream os(source, std::ios::app); os << "2026-10-16,10:31,101\n"; }
    const auto fp2 = Cache::fingerprint({source});
    if (fp1 == fp2) throw std::runtime_error("Source fingerprint did not change");

    Cache cache(root / "cache");
    const std::vector<std::int64_t> requested{100,160};
    Cache::State state{};
    state.stock_symbol = "TEST";
    state.benchmark_symbol = "NIFTY 50";
    state.decision_time = 100;
    Cache::States states{{100, state}}, loaded;
    cache.save("TEST", "NIFTY 50", 160, fp2, requested, states);
    if (!cache.load("TEST", "NIFTY 50", 160, fp2, requested, loaded) || loaded.size()!=1)
        throw std::runtime_error("Valid cache failed to load");
    if (cache.load("TEST", "NIFTY 50", 160, fp1, requested, loaded))
        throw std::runtime_error("Stale source fingerprint accepted");
    if (cache.load("TEST", "NIFTY 50", 160, fp2, {100}, loaded))
        throw std::runtime_error("Wrong requested timestamps accepted");

    fs::path cache_file;
    for (const auto& e : fs::directory_iterator(root / "cache")) {
        if (e.path().extension()==".bin") cache_file=e.path();
    }
    if (cache_file.empty()) throw std::runtime_error("No cache file created");
    {
        std::fstream io(cache_file, std::ios::in | std::ios::out | std::ios::binary);
        char c{}; io.get(c); io.seekp(0); c ^= 0x01; io.put(c);
    }
    if (cache.load("TEST", "NIFTY 50", 160, fp2, requested, loaded))
        throw std::runtime_error("Corrupted cache accepted");
    cache.save("TEST", "NIFTY 50", 160, fp2, requested, states);
    fs::resize_file(cache_file, fs::file_size(cache_file)-3);
    if (cache.load("TEST", "NIFTY 50", 160, fp2, requested, loaded))
        throw std::runtime_error("Truncated cache accepted");
    std::cout << "[H47][STAGE5B][HARDENING] PASS: roundtrip, fingerprint, timestamps, corruption, truncation\n";
}
