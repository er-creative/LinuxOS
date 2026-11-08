#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: replay_probe <decision_epoch>\n";
        return 2;
    }

    try {
        devai::strategy::HistoricalCETEReplayConfig config{};

        config.data_folder =
            "/home/devinderjeet/fmrss_cpp/data/validation/"
            "phase319_3a/kite_downloads/minute";

        config.benchmark_symbol = "NIFTY%2050";
        config.minimum_universe_size = 10;
        config.maximum_stock_count = 10;

        devai::strategy::HistoricalCETEReplay replay(config);

        const auto result = replay.replayAt(std::stoll(argv[1]));

        std::cout
            << "Snapshot time: " << result.snapshot.decision_time << '\n'
            << "Stocks: " << result.snapshot.stock_count << '\n'
            << "Integrity: " << result.snapshot.numerical_integrity << '\n'
            << "Replay passed: " << result.stats.passed() << '\n';

        return result.stats.passed() ? 0 : 3;
    }
    catch (const std::exception& e) {
        std::cerr << "Replay failed: " << e.what() << '\n';
        return 1;
    }
}