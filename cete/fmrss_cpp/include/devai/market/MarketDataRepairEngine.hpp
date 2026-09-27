#pragma once

#include "devai/market/Candle.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace devai::market
{

struct MissingRange
{
    std::string symbol;

    std::int64_t first_missing_timestamp{0};
    std::int64_t last_missing_timestamp{0};

    std::size_t missing_minutes{0};
};


struct RepairResult
{
    std::string symbol;

    std::size_t original_candles{0};

    std::size_t gaps_found{0};
    std::size_t missing_candles_requested{0};

    std::size_t replacement_candles_received{0};
    std::size_t replacement_candles_accepted{0};
    std::size_t replacement_candles_rejected{0};

    std::size_t duplicates_removed{0};

    std::size_t final_candles{0};

    bool changed{false};
    bool success{false};

    std::vector<MissingRange> gaps;
};


class MarketDataRepairEngine
{
public:

    [[nodiscard]]
    std::vector<MissingRange> findInternalGaps(
        const std::string& symbol,
        const std::vector<Candle>& candles
    ) const;


    [[nodiscard]]
    RepairResult repair(
        const std::string& symbol,
        const std::vector<Candle>& existing,
        const std::vector<Candle>& downloaded,
        std::vector<Candle>& repaired
    ) const;


    void writeAtomic(
        const std::filesystem::path& destination,
        const std::vector<Candle>& candles
    ) const;


private:

    [[nodiscard]]
    static bool sameTradingDate(
        std::int64_t left,
        std::int64_t right
    );


    [[nodiscard]]
    static bool insideRegularSession(
        std::int64_t timestamp
    );


    [[nodiscard]]
    static bool validReplacement(
        const Candle& candle,
        const std::string& expected_symbol
    );
};

}