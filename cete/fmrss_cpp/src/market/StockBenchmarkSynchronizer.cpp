#include "devai/market/StockBenchmarkSynchronizer.hpp"

#include <stdexcept>
#include <string>

namespace devai::market
{

// ============================================================================
// Validate basic snapshot integrity
// ============================================================================

void StockBenchmarkSynchronizer::validateSnapshot(
    const MarketSnapshot& snapshot,
    const char* role)
{
    if (snapshot.symbol.empty())
    {
        throw std::invalid_argument(
            std::string(role) +
            " snapshot has an empty symbol."
        );
    }

    if (snapshot.decision_time <= 0)
    {
        throw std::invalid_argument(
            std::string(role) +
            " snapshot has an invalid decision time."
        );
    }
}


// ============================================================================
// Stock and benchmark must represent different instruments
// ============================================================================

void StockBenchmarkSynchronizer::validateDifferentSymbols(
    const MarketSnapshot& stock,
    const MarketSnapshot& benchmark)
{
    if (stock.symbol == benchmark.symbol)
    {
        throw std::invalid_argument(
            "Stock and benchmark symbols must be different."
        );
    }
}


// ============================================================================
// Both snapshots MUST have been created for exactly the same decision time.
// ============================================================================

void StockBenchmarkSynchronizer::validateDecisionTime(
    const MarketSnapshot& stock,
    const MarketSnapshot& benchmark)
{
    if (
        stock.decision_time !=
        benchmark.decision_time
    )
    {
        throw std::invalid_argument(
            "Stock and benchmark decision times do not match."
        );
    }
}


// ============================================================================
// Prevent accidental cross-day pairing.
// ============================================================================

void StockBenchmarkSynchronizer::validateTradingDate(
    const MarketSnapshot& stock,
    const MarketSnapshot& benchmark)
{
    if (
        stock.trading_date !=
        benchmark.trading_date
    )
    {
        throw std::invalid_argument(
            "Stock and benchmark trading dates do not match."
        );
    }
}


// ============================================================================
// Strict timeframe alignment
//
// If BOTH instruments have a completed candle for a timeframe, their market
// timestamps must match.
//
// Example at 09:30:
//
// RELIANCE 1m = 09:29
// NIFTY    1m = 09:29
//
// RELIANCE 5m = 09:25
// NIFTY    5m = 09:25
//
// RELIANCE 15m = 09:15
// NIFTY    15m = 09:15
//
// If only one side has a candle, we do not throw here. The resulting pair
// simply will not be fully synchronized.
// ============================================================================

void StockBenchmarkSynchronizer::validateTimeframeAlignment(
    const std::optional<Candle>& stock_candle,
    const std::optional<Candle>& benchmark_candle,
    const char* timeframe_name)
{
    if (
        !stock_candle.has_value() ||
        !benchmark_candle.has_value()
    )
    {
        return;
    }

    if (
        stock_candle->timestamp !=
        benchmark_candle->timestamp
    )
    {
        throw std::runtime_error(
            std::string(
                "Stock/NIFTY "
            ) +
            timeframe_name +
            " candle timestamps are not aligned."
        );
    }
}


// ============================================================================
// Synchronize
// ============================================================================

StockBenchmarkSnapshot
StockBenchmarkSynchronizer::synchronize(
    const MarketSnapshot& stock,
    const MarketSnapshot& benchmark) const
{
    validateSnapshot(
        stock,
        "Stock"
    );

    validateSnapshot(
        benchmark,
        "Benchmark"
    );

    validateDifferentSymbols(
        stock,
        benchmark
    );

    validateDecisionTime(
        stock,
        benchmark
    );

    validateTradingDate(
        stock,
        benchmark
    );


    // ========================================================================
    // Session phase must also agree.
    // ========================================================================

    if (
        stock.session_phase !=
        benchmark.session_phase
    )
    {
        throw std::runtime_error(
            "Stock and benchmark session phases do not match."
        );
    }


    // ========================================================================
    // Check individual timeframe timestamps.
    // ========================================================================

    validateTimeframeAlignment(
        stock.one_minute,
        benchmark.one_minute,
        "1-minute"
    );

    validateTimeframeAlignment(
        stock.five_minute,
        benchmark.five_minute,
        "5-minute"
    );

    validateTimeframeAlignment(
        stock.fifteen_minute,
        benchmark.fifteen_minute,
        "15-minute"
    );


    // ========================================================================
    // Build synchronized pair.
    // ========================================================================

    StockBenchmarkSnapshot result;

    result.stock =
        stock;

    result.benchmark =
        benchmark;

    result.decision_time =
        stock.decision_time;


    return result;
}

}