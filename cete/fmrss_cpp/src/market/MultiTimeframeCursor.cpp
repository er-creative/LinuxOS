#include "devai/market/MultiTimeframeCursor.hpp"

#include <stdexcept>
#include <string>

namespace devai::market
{

MultiTimeframeCursor::MultiTimeframeCursor(
    const std::vector<TimedCandle>& one_minute,
    const std::vector<TimedCandle>& five_minute,
    const std::vector<TimedCandle>& fifteen_minute
)
    : one_minute_(one_minute),
      five_minute_(five_minute),
      fifteen_minute_(fifteen_minute)
{
    validateStream(
        one_minute_,
        "1-minute"
    );

    validateStream(
        five_minute_,
        "5-minute"
    );

    validateStream(
        fifteen_minute_,
        "15-minute"
    );
}


// ============================================================================
// Validate prepared stream
//
// MultiTimeframeSynchronizer::prepare*() sorts TimedCandle objects by:
//
//     received_at
//     then candle.timestamp
//
// MultiTimeframeCursor depends on that ordering.
// ============================================================================

void MultiTimeframeCursor::validateStream(
    const std::vector<TimedCandle>& stream,
    const char* stream_name)
{
    for (std::size_t i = 0; i < stream.size(); ++i)
    {
        const TimedCandle& current =
            stream[i];

        if (!current.candle.valid())
        {
            throw std::invalid_argument(
                std::string(stream_name) +
                " stream contains an invalid candle."
            );
        }

        if (current.completed_at > current.received_at)
        {
            throw std::invalid_argument(
                std::string(stream_name) +
                " stream contains a candle received "
                "before completion."
            );
        }

        if (i == 0)
        {
            continue;
        }

        const TimedCandle& previous =
            stream[i - 1];

        if (current.received_at < previous.received_at)
        {
            throw std::invalid_argument(
                std::string(stream_name) +
                " stream is not sorted by received_at."
            );
        }

        if (
            current.received_at == previous.received_at &&
            current.candle.timestamp <
                previous.candle.timestamp
        )
        {
            throw std::invalid_argument(
                std::string(stream_name) +
                " stream is not correctly ordered."
            );
        }
    }
}


// ============================================================================
// Advance one stream
//
// Consume every event that is genuinely available by decision_time.
//
// We retain the candle having the newest MARKET timestamp among all
// available events.
//
// This matters for delayed/out-of-order receipt. An old candle arriving late
// must never replace a newer market candle already known to the strategy.
// ============================================================================

void MultiTimeframeCursor::advanceStream(
    const std::vector<TimedCandle>& stream,
    std::size_t& index,
    std::int64_t decision_time,
    std::optional<TimedCandle>& latest)
{
    while (index < stream.size())
    {
        const TimedCandle& candidate =
            stream[index];

        // Stream is ordered by received_at, so once this event
        // has not arrived, later events cannot be consumed yet.
        if (candidate.received_at > decision_time)
        {
            break;
        }

        // Defensive look-ahead protection.
        //
        // Normally received_at >= completed_at is guaranteed by
        // MultiTimeframeSynchronizer, but we retain this check here.
        if (candidate.completed_at <= decision_time)
        {
            if (
                !latest.has_value() ||
                candidate.candle.timestamp >
                    latest->candle.timestamp
            )
            {
                latest =
                    candidate;
            }
        }

        ++index;
    }
}


// ============================================================================
// Advance all three timeframes
// ============================================================================

void MultiTimeframeCursor::advanceTo(
    std::int64_t decision_time)
{
    if (
        initialized_ &&
        decision_time < state_.decision_time
    )
    {
        throw std::invalid_argument(
            "MultiTimeframeCursor is forward-only: "
            "decision_time cannot move backwards."
        );
    }

    advanceStream(
        one_minute_,
        one_index_,
        decision_time,
        state_.one_minute
    );

    advanceStream(
        five_minute_,
        five_index_,
        decision_time,
        state_.five_minute
    );

    advanceStream(
        fifteen_minute_,
        fifteen_index_,
        decision_time,
        state_.fifteen_minute
    );

    state_.decision_time =
        decision_time;

    initialized_ =
        true;
}


// ============================================================================
// Reset
//
// Does not modify source vectors.
// It simply moves all cursor positions back to the beginning.
// ============================================================================

void MultiTimeframeCursor::reset() noexcept
{
    one_index_ =
        0;

    five_index_ =
        0;

    fifteen_index_ =
        0;

    state_ =
        MultiTimeframeState{};

    initialized_ =
        false;
}

}