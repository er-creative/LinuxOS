#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace devai::strategy;

int main() {
    const auto path = std::filesystem::temp_directory_path() /
                      "cete_319c_price_test.txt";

    {
        std::ofstream file(path);
        assert(file.is_open());
        file << "Symbol,Date,Time,Open,High,Low,Close,Volume\n"
             << "AAA,2026-10-07,09:15:00,100,101,99,100,200\n"
             << "AAA,2026-10-07,09:16:00,101,102,100,101,200\n"
             << "AAA,2026-10-07,09:17:00,102,103,101,102,200\n";
        file.flush();
        assert(file.good());
    }

    DevHistoricalCloseStore store;
    store.loadMinuteFile(path, "AAA");

    // 09:16 IST is 03:46 UTC. CETE historical timestamps are Unix seconds.
    const auto utc_day = std::chrono::sys_days{
        std::chrono::year{2026} /
        std::chrono::month{10} /
        std::chrono::day{7}
    };
    const auto decision_time = utc_day + std::chrono::hours{9}
                              + std::chrono::minutes{16}
                              - std::chrono::minutes{330};
    const std::int64_t origin =
        std::chrono::duration_cast<std::chrono::seconds>(
            decision_time.time_since_epoch()).count();

    // At 09:16, the 09:15–09:16 candle has completed.
    assert(store.completedClose("AAA", origin) == 100.0);
    // At 09:17, the 09:16–09:17 candle has completed.
    assert(store.completedClose("AAA", origin + 60) == 101.0);

    bool rejected = false;
    try {
        (void)store.completedClose("AAA", origin + 240);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    std::filesystem::remove(path);
    std::cout << "3.19C completed-candle and stale-price checks: PASSED\n";
    return 0;
}
