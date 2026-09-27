#include "devai/market/MultiTimeframeSynchronizer.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace devai::market
{

namespace
{

constexpr std::int64_t ONE_MINUTE_SECONDS     = 60;
constexpr std::int64_t FIVE_MINUTE_SECONDS    = 5 * 60;
constexpr std::int64_t FIFTEEN_MINUTE_SECONDS = 15 * 60;

}


// =========================================================
// Constructor
// =========================================================

MultiTimeframeSynchronizer::
MultiTimeframeSynchronizer(
    MultiTimeframeSynchronizerConfig config
)
    : config_(std::move(config))
{
    if (config_.one_minute_latency_seconds < 0 ||
        config_.five_minute_latency_seconds < 0 ||
        config_.fifteen_minute_latency_seconds < 0)
    {
        throw std::invalid_argument(
            "Latency cannot be negative."
        );
    }
}


// =========================================================
// Historical Preparation
// =========================================================

std::vector<TimedCandle>
MultiTimeframeSynchronizer::prepareHistorical(
    const std::vector<Candle>& candles,
    std::int64_t timeframe_seconds,
    std::int64_t latency_seconds
) const
{
    if (config_.mode ==
        AvailabilityMode::LIVE_RECEIPT)
    {
        throw std::logic_error(
            "prepareHistorical() cannot be used in "
            "LIVE_RECEIPT mode. Actual received_at "
            "timestamps are required."
        );
    }

    if (timeframe_seconds <= 0)
    {
        throw std::invalid_argument(
            "Timeframe duration must be positive."
        );
    }

    std::vector<TimedCandle> result;

    result.reserve(candles.size());


    for (const Candle& candle : candles)
    {
        if (!candle.valid())
        {
            continue;
        }

        const std::int64_t completed_at =
            candle.timestamp +
            timeframe_seconds;


        std::int64_t received_at =
            completed_at;


        if (config_.mode ==
            AvailabilityMode::SIMULATED_LATENCY)
        {
            received_at += latency_seconds;
        }


        result.push_back(
            TimedCandle{
                candle,
                completed_at,
                received_at
            }
        );
    }


    // -----------------------------------------------------
    // IMPORTANT:
    //
    // Sort by received_at because strategy availability
    // depends on when information actually became visible,
    // not merely the candle's market timestamp.
    //
    // timestamp is used as a deterministic tie-breaker.
    // -----------------------------------------------------

    std::sort(
        result.begin(),
        result.end(),
        [](const TimedCandle& lhs,
           const TimedCandle& rhs)
        {
            if (lhs.received_at != rhs.received_at)
            {
                return
                    lhs.received_at <
                    rhs.received_at;
            }

            return
                lhs.candle.timestamp <
                rhs.candle.timestamp;
        }
    );


    return result;
}


// =========================================================
// Prepare 1-Minute
// =========================================================

std::vector<TimedCandle>
MultiTimeframeSynchronizer::prepareOneMinute(
    const std::vector<Candle>& candles
) const
{
    return prepareHistorical(
        candles,
        ONE_MINUTE_SECONDS,
        config_.one_minute_latency_seconds
    );
}


// =========================================================
// Prepare 5-Minute
// =========================================================

std::vector<TimedCandle>
MultiTimeframeSynchronizer::prepareFiveMinute(
    const std::vector<Candle>& candles
) const
{
    return prepareHistorical(
        candles,
        FIVE_MINUTE_SECONDS,
        config_.five_minute_latency_seconds
    );
}


// =========================================================
// Prepare 15-Minute
// =========================================================

std::vector<TimedCandle>
MultiTimeframeSynchronizer::prepareFifteenMinute(
    const std::vector<Candle>& candles
) const
{
    return prepareHistorical(
        candles,
        FIFTEEN_MINUTE_SECONDS,
        config_.fifteen_minute_latency_seconds
    );
}


// =========================================================
// Live Timed Candle
// =========================================================

TimedCandle
MultiTimeframeSynchronizer::makeLiveTimedCandle(
    const Candle& candle,
    std::int64_t timeframe_seconds,
    std::int64_t received_at
) const
{
    if (config_.mode !=
        AvailabilityMode::LIVE_RECEIPT)
    {
        throw std::logic_error(
            "makeLiveTimedCandle() requires "
            "LIVE_RECEIPT mode."
        );
    }


    if (!candle.valid())
    {
        throw std::invalid_argument(
            "Cannot create TimedCandle from "
            "an invalid candle."
        );
    }


    if (timeframe_seconds <= 0)
    {
        throw std::invalid_argument(
            "Timeframe duration must be positive."
        );
    }


    const std::int64_t completed_at =
        candle.timestamp +
        timeframe_seconds;


    // -----------------------------------------------------
    // Critical anti-lookahead rule.
    //
    // A candle cannot legitimately arrive before it has
    // actually completed.
    // -----------------------------------------------------

    if (received_at < completed_at)
    {
        throw std::invalid_argument(
            "received_at cannot be earlier than "
            "candle completion time."
        );
    }


    return TimedCandle{
        candle,
        completed_at,
        received_at
    };
}


// =========================================================
// Latest Available Candle
// =========================================================

std::optional<TimedCandle>
MultiTimeframeSynchronizer::latestAvailable(
    const std::vector<TimedCandle>& candles,
    std::int64_t decision_time
)
{
    const TimedCandle* best = nullptr;


    // -----------------------------------------------------
    // Do NOT simply return the last received candle.
    //
    // With live data it is possible for an older candle to
    // arrive late. We therefore choose the AVAILABLE candle
    // having the greatest market timestamp.
    // -----------------------------------------------------

    for (const TimedCandle& timed : candles)
    {
        if (timed.received_at > decision_time)
        {
            continue;
        }


        // Secondary protection:
        // even malformed input must not expose an
        // unfinished candle.
        if (timed.completed_at > decision_time)
        {
            continue;
        }


        if (best == nullptr ||
            timed.candle.timestamp >
                best->candle.timestamp)
        {
            best = &timed;
        }
    }


    if (best == nullptr)
    {
        return std::nullopt;
    }


    return *best;
}


// =========================================================
// Synchronize At Decision Time
// =========================================================

MultiTimeframeState
MultiTimeframeSynchronizer::stateAt(
    std::int64_t decision_time,
    const std::vector<TimedCandle>& one_minute,
    const std::vector<TimedCandle>& five_minute,
    const std::vector<TimedCandle>& fifteen_minute
) const
{
    MultiTimeframeState state;

    state.decision_time =
        decision_time;


    state.one_minute =
        latestAvailable(
            one_minute,
            decision_time
        );


    state.five_minute =
        latestAvailable(
            five_minute,
            decision_time
        );


    state.fifteen_minute =
        latestAvailable(
            fifteen_minute,
            decision_time
        );


    return state;
}

} // namespace devai::market