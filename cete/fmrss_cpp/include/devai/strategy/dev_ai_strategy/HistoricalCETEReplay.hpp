#pragma once

#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace devai::strategy
{

struct HistoricalCETEReplayConfig
{
    // Primary Zerodha Kite historical 1-minute data.
    std::filesystem::path data_folder{
        "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/"
        "kite_downloads/minute"
    };

    // Primary Zerodha Kite historical 5-minute data.
    std::filesystem::path five_minute_data_folder{
        "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/"
        "kite_downloads/5minutes"
    };

    // Primary Zerodha Kite historical 15-minute data.
    std::filesystem::path fifteen_minute_data_folder{
        "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/"
        "kite_downloads/15minutes"
    };

    std::string benchmark_symbol{"NIFTY%2050"};
    std::size_t minimum_universe_size{2};
    // 0 = use every discovered stock.
    std::size_t maximum_stock_count{0};
};

struct HistoricalCETEReplayStats
{
    std::size_t discovered_stock_count{0};
    std::size_t stocks_processed{0};
    std::size_t stocks_skipped{0};

    std::size_t lookahead_violations{0};
    std::size_t decision_time_violations{0};
    std::size_t symbol_violations{0};
    std::size_t benchmark_contamination{0};
    std::size_t duplicate_symbol_violations{0};

    std::size_t phase4_exceptions{0};
    std::size_t phase52_exceptions{0};
    std::size_t phase53_exceptions{0};
    std::size_t phase54_exceptions{0};

    [[nodiscard]] bool passed() const noexcept
    {
        return stocks_processed >= 2 &&
               lookahead_violations == 0 &&
               decision_time_violations == 0 &&
               symbol_violations == 0 &&
               benchmark_contamination == 0 &&
               duplicate_symbol_violations == 0 &&
               phase4_exceptions == 0 &&
               phase52_exceptions == 0 &&
               phase53_exceptions == 0 &&
               phase54_exceptions == 0;
    }
};

struct HistoricalCETEReplayPoint
{
    devai::ranking::CrossSectionalUniverseSnapshot snapshot;
    HistoricalCETEReplayStats stats;
};

class HistoricalCETEReplay
{
public:
    explicit HistoricalCETEReplay(HistoricalCETEReplayConfig config = {});

    [[nodiscard]] const HistoricalCETEReplayConfig& config() const noexcept;

    [[nodiscard]] std::vector<std::string> discoverSymbols() const;

    // Earliest common final 1-minute completion time across the selected
    // stock universe and benchmark. Matches CETE Phase-5.5 target-time rule.
    [[nodiscard]] std::int64_t latestCommonDecisionTime() const;

    // Causal CETE Phase-5 universe snapshot; no future outcomes.
    [[nodiscard]] HistoricalCETEReplayPoint replayAt(
        std::int64_t decision_time) const;

private:
    HistoricalCETEReplayConfig config_;
};

} // namespace devai::strategy
