#pragma once

#include "devai/market/Candle.hpp"
#include "devai/market/Timeframe.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace devai::market
{

class CandleAggregator
{
public:
    struct AggregationResult
    {
        std::vector<Candle> candles;

        // Number of candidate higher-timeframe buckets seen.
        std::size_t total_buckets{0};

        // Successfully generated candles.
        std::size_t completed_buckets{0};

        // Buckets rejected because one or more 1-minute
        // candles were missing.
        std::size_t incomplete_buckets{0};

        // Invalid input candles encountered.
        std::size_t invalid_input_candles{0};

        // Duplicate 1-minute timestamps encountered.
        std::size_t duplicate_input_candles{0};
    };

    // -----------------------------------------------------
    // Aggregate 1-minute candles into the requested
    // higher timeframe.
    //
    // Supported targets:
    //
    //     FIVE_MINUTES
    //     FIFTEEN_MINUTES
    //
    // Input may contain one or multiple symbols.
    // -----------------------------------------------------

    [[nodiscard]]
    AggregationResult aggregate(
        const std::vector<Candle>& one_minute_candles,
        Timeframe target_timeframe
    ) const;

private:
    [[nodiscard]]
    static bool supportedTarget(
        Timeframe timeframe
    ) noexcept;

    [[nodiscard]]
    static std::int64_t bucketStart(
        std::int64_t timestamp,
        Timeframe timeframe
    ) noexcept;
};

} // namespace devai::market