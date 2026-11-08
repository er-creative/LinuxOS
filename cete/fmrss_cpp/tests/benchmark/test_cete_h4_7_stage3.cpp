#include "devai/benchmark/DevRealHistoricalExecution_H47.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    using devai::benchmark::h47::exchangeKeyToUtcH47;
    try {
        // 2026-01-01 civil ordinal = 739922, at 09:15 IST.
        const std::int64_t day = 739922;
        const auto utc = exchangeKeyToUtcH47(day * 1440 + 555);
        const auto minute = ((utc + 19800) % 86400) / 60;
        if (minute != 555 || utc % 60 != 0)
            throw std::runtime_error("UTC conversion incorrect");
        bool rejected = false;
        try { (void)exchangeKeyToUtcH47(0); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("invalid key accepted");
        std::cout << "CETE H4.7 STAGE 3 TIME CONVERTER TEST PASS\n"
                  << "Real historical file execution: NOT RUN IN THIS TEST\n"
                  << "Production ranking/encoder: NOT VERIFIED\n"
                  << "D9F4C execution: NOT RUN\n";
    } catch (const std::exception& e) {
        std::cerr << "H4.7 Stage3 FAIL: " << e.what() << '\n';
        return 1;
    }
}
