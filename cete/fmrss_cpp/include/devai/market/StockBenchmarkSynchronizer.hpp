#pragma once

#include "devai/market/StockBenchmarkSnapshot.hpp"

#include <optional>

namespace devai::market
{

class StockBenchmarkSynchronizer
{
public:

    [[nodiscard]]
    StockBenchmarkSnapshot synchronize(
        const MarketSnapshot& stock,
        const MarketSnapshot& benchmark
    ) const;


private:

    static void validateSnapshot(
        const MarketSnapshot& snapshot,
        const char* role
    );

    static void validateDifferentSymbols(
        const MarketSnapshot& stock,
        const MarketSnapshot& benchmark
    );

    static void validateDecisionTime(
        const MarketSnapshot& stock,
        const MarketSnapshot& benchmark
    );

    static void validateTradingDate(
        const MarketSnapshot& stock,
        const MarketSnapshot& benchmark
    );

    static void validateTimeframeAlignment(
        const std::optional<Candle>& stock_candle,
        const std::optional<Candle>& benchmark_candle,
        const char* timeframe_name
    );
};

}