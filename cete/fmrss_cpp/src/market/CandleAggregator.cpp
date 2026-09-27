#include "devai/market/CandleAggregator.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace devai::market
{

namespace
{

// =========================================================
// Internal Candle Key
//
// Used to detect duplicate:
//     symbol + timestamp
// =========================================================

std::string candleKey(const Candle& candle)
{
    return candle.symbol +
           "#" +
           std::to_string(candle.timestamp);
}


// =========================================================
// Same Trading Date
//
// Unix timestamps are being interpreted using the same
// local-time convention currently used by MarketDataLoader.
//
// NSE session candles never cross midnight, so checking
// bucket boundaries later is sufficient for the current
// architecture.
// =========================================================

bool sameSymbol(
    const Candle& lhs,
    const Candle& rhs) noexcept
{
    return lhs.symbol == rhs.symbol;
}

} // anonymous namespace


// =========================================================
// Supported Target
// =========================================================

bool CandleAggregator::supportedTarget(
    Timeframe timeframe) noexcept
{
    return timeframe == Timeframe::FIVE_MINUTES ||
           timeframe == Timeframe::FIFTEEN_MINUTES;
}


// =========================================================
// Bucket Start
//
// IMPORTANT:
//
// NSE session begins at 09:15.
//
// Because 09:15 is naturally aligned to both 5-minute
// and 15-minute boundaries:
//
//     09:15 -> 09:15
//     09:19 -> 09:15
//
//     09:20 -> 09:20
//     09:24 -> 09:20
//
//     09:15 -> 09:15  (15m)
//     09:29 -> 09:15  (15m)
//
// Unix epoch division therefore gives the correct bucket
// boundaries for these timeframes.
// =========================================================

std::int64_t CandleAggregator::bucketStart(
    std::int64_t timestamp,
    Timeframe timeframe) noexcept
{
    const std::int64_t interval =
        seconds(timeframe);

    return (timestamp / interval) * interval;
}


// =========================================================
// Aggregate
// =========================================================

CandleAggregator::AggregationResult
CandleAggregator::aggregate(
    const std::vector<Candle>& one_minute_candles,
    Timeframe target_timeframe) const
{
    // -----------------------------------------------------
    // Validate target timeframe
    // -----------------------------------------------------

    if (!supportedTarget(target_timeframe))
    {
        throw std::invalid_argument(
            "CandleAggregator supports only "
            "FIVE_MINUTES and FIFTEEN_MINUTES."
        );
    }

    AggregationResult result;

    if (one_minute_candles.empty())
    {
        return result;
    }

    // -----------------------------------------------------
    // Number of 1-minute candles required for one complete
    // higher-timeframe candle.
    //
    // 5m  -> 5
    // 15m -> 15
    // -----------------------------------------------------

    const std::size_t required_candles =
        static_cast<std::size_t>(
            minutes(target_timeframe)
        );

    const std::int64_t one_minute_seconds =
        seconds(Timeframe::ONE_MINUTE);

    // -----------------------------------------------------
    // Work on a sorted copy.
    //
    // MarketDataLoader already sorts data, but keeping the
    // aggregator independently safe prevents subtle bugs if
    // another source supplies unsorted live/test data later.
    // -----------------------------------------------------

    std::vector<Candle> input;

    input.reserve(one_minute_candles.size());

    std::unordered_set<std::string> seen;

    seen.reserve(one_minute_candles.size());

    for (const Candle& candle : one_minute_candles)
    {
        if (!candle.valid())
        {
            ++result.invalid_input_candles;
            continue;
        }

        const std::string key =
            candleKey(candle);

        if (!seen.insert(key).second)
        {
            ++result.duplicate_input_candles;
            continue;
        }

        input.push_back(candle);
    }

    if (input.empty())
    {
        return result;
    }

    std::sort(
        input.begin(),
        input.end(),
        [](const Candle& lhs,
           const Candle& rhs)
        {
            if (lhs.symbol != rhs.symbol)
            {
                return lhs.symbol < rhs.symbol;
            }

            return lhs.timestamp < rhs.timestamp;
        }
    );

    // -----------------------------------------------------
    // Reserve approximate output capacity.
    // -----------------------------------------------------

    result.candles.reserve(
        input.size() / required_candles + 1
    );

    // -----------------------------------------------------
    // Process each symbol/bucket.
    // -----------------------------------------------------

    std::size_t index = 0;

    while (index < input.size())
    {
        const Candle& first =
            input[index];

        const std::string symbol =
            first.symbol;

        const std::int64_t bucket_start =
            bucketStart(
                first.timestamp,
                target_timeframe
            );

        const std::int64_t bucket_end =
            bucket_start +
            seconds(target_timeframe);

        ++result.total_buckets;

        // -------------------------------------------------
        // Gather all 1-minute candles belonging to this
        // symbol and this higher-timeframe bucket.
        // -------------------------------------------------

        std::size_t end = index;

        while (end < input.size())
        {
            const Candle& current =
                input[end];

            if (!sameSymbol(first, current))
            {
                break;
            }

            if (current.timestamp < bucket_start ||
                current.timestamp >= bucket_end)
            {
                break;
            }

            ++end;
        }

        const std::size_t bucket_size =
            end - index;

        // -------------------------------------------------
        // A complete bucket MUST contain exactly:
        //
        //     5 candles  for 5m
        //     15 candles for 15m
        //
        // But count alone is not enough. We also verify
        // timestamp continuity below.
        // -------------------------------------------------

        bool complete =
            bucket_size == required_candles;

        if (complete)
        {
            for (std::size_t offset = 0;
                 offset < required_candles;
                 ++offset)
            {
                const std::int64_t expected_timestamp =
                    bucket_start +
                    static_cast<std::int64_t>(offset) *
                    one_minute_seconds;

                const std::int64_t actual_timestamp =
                    input[index + offset].timestamp;

                if (actual_timestamp !=
                    expected_timestamp)
                {
                    complete = false;
                    break;
                }
            }
        }

        // -------------------------------------------------
        // Reject incomplete higher-timeframe candles.
        //
        // Example:
        //
        // 09:15
        // 09:16
        // missing 09:17
        // 09:18
        // 09:19
        //
        // This MUST NOT become a valid 5m candle.
        // -------------------------------------------------

        if (!complete)
        {
            ++result.incomplete_buckets;

            index = end;

            continue;
        }

        // -------------------------------------------------
        // Construct OHLCV
        // -------------------------------------------------

        Candle aggregated;

        aggregated.symbol =
            symbol;

        // Timestamp represents START of candle.
        aggregated.timestamp =
            bucket_start;

        aggregated.open =
            input[index].open;

        aggregated.high =
            -std::numeric_limits<double>::infinity();

        aggregated.low =
            std::numeric_limits<double>::infinity();

        aggregated.volume = 0;

        for (std::size_t position = index;
             position < end;
             ++position)
        {
            const Candle& source =
                input[position];

            aggregated.high =
                std::max(
                    aggregated.high,
                    source.high
                );

            aggregated.low =
                std::min(
                    aggregated.low,
                    source.low
                );

            // -------------------------------------------------
            // Protect uint64 volume from overflow.
            // -------------------------------------------------

            if (source.volume >
                std::numeric_limits<std::uint64_t>::max() -
                aggregated.volume)
            {
                throw std::overflow_error(
                    "CandleAggregator: volume overflow for " +
                    symbol
                );
            }

            aggregated.volume +=
                source.volume;
        }

        aggregated.close =
            input[end - 1].close;

        // -------------------------------------------------
        // Final safety validation
        // -------------------------------------------------

        if (!aggregated.valid())
        {
            ++result.incomplete_buckets;

            index = end;

            continue;
        }

        result.candles.push_back(
            std::move(aggregated)
        );

        ++result.completed_buckets;

        index = end;
    }

    return result;
}

} // namespace devai::market