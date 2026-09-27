#include "devai/market/SessionState.hpp"

#include <ctime>
#include <stdexcept>

namespace devai::market
{

namespace
{

std::tm toLocalTime(
    std::int64_t timestamp)
{
    const std::time_t raw =
        static_cast<std::time_t>(
            timestamp
        );

    std::tm result{};

#if defined(_WIN32)

    localtime_s(
        &result,
        &raw
    );

#else

    localtime_r(
        &raw,
        &result
    );

#endif

    return result;
}

}


// ============================================================================
// Constructor
// ============================================================================

SessionState::SessionState(
    int session_open_hour,
    int session_open_minute,
    int session_close_hour,
    int session_close_minute
)
    : open_hour_(
          session_open_hour),
      open_minute_(
          session_open_minute),
      close_hour_(
          session_close_hour),
      close_minute_(
          session_close_minute)
{
    if (
        open_hour_ < 0 ||
        open_hour_ > 23 ||
        close_hour_ < 0 ||
        close_hour_ > 23
    )
    {
        throw std::invalid_argument(
            "Invalid session hour."
        );
    }


    if (
        open_minute_ < 0 ||
        open_minute_ > 59 ||
        close_minute_ < 0 ||
        close_minute_ > 59
    )
    {
        throw std::invalid_argument(
            "Invalid session minute."
        );
    }


    const int open_total =
        open_hour_ * 60 +
        open_minute_;

    const int close_total =
        close_hour_ * 60 +
        close_minute_;


    if (close_total <= open_total)
    {
        throw std::invalid_argument(
            "Session close must be after session open."
        );
    }
}


// ============================================================================
// Timestamp -> trading date
// ============================================================================

TradingDate SessionState::dateFromTimestamp(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(timestamp);


    TradingDate date;

    date.year =
        value.tm_year + 1900;

    date.month =
        value.tm_mon + 1;

    date.day =
        value.tm_mday;


    return date;
}


// ============================================================================
// Build local timestamp
// ============================================================================

std::int64_t SessionState::makeLocalTimestamp(
    const TradingDate& date,
    int hour,
    int minute)
{
    std::tm value{};

    value.tm_year =
        date.year - 1900;

    value.tm_mon =
        date.month - 1;

    value.tm_mday =
        date.day;

    value.tm_hour =
        hour;

    value.tm_min =
        minute;

    value.tm_sec =
        0;

    value.tm_isdst =
        -1;


    const std::time_t timestamp =
        std::mktime(
            &value
        );


    if (timestamp == static_cast<std::time_t>(-1))
    {
        throw std::runtime_error(
            "Unable to construct session timestamp."
        );
    }


    return static_cast<std::int64_t>(
        timestamp
    );
}


// ============================================================================
// Update
// ============================================================================

bool SessionState::update(
    std::int64_t decision_time)
{
    const TradingDate date =
        dateFromTimestamp(
            decision_time
        );


    const bool new_session =
        !initialized_ ||
        date != trading_date_;


    if (new_session)
    {
        trading_date_ =
            date;


        session_open_timestamp_ =
            makeLocalTimestamp(
                trading_date_,
                open_hour_,
                open_minute_
            );


        session_close_timestamp_ =
            makeLocalTimestamp(
                trading_date_,
                close_hour_,
                close_minute_
            );
    }


    if (
        decision_time <
        session_open_timestamp_
    )
    {
        phase_ =
            SessionPhase::BEFORE_OPEN;
    }
    else if (
        decision_time <
        session_close_timestamp_
    )
    {
        phase_ =
            SessionPhase::ACTIVE;
    }
    else
    {
        phase_ =
            SessionPhase::AFTER_CLOSE;
    }


    initialized_ =
        true;


    return new_session;
}


// ============================================================================
// Does candle belong to current trading session?
//
// candle.timestamp is the START timestamp.
//
// Therefore:
//
// 1m valid starts:
//
//     09:15 ... 15:29
//
// 5m valid starts:
//
//     09:15 ... 15:25
//
// 15m valid starts:
//
//     09:15 ... 15:15
//
// A generic session-range check works for all three.
// ============================================================================

bool SessionState::belongsToCurrentSession(
    const TimedCandle& candle) const noexcept
{
    if (!initialized_)
    {
        return false;
    }


    return
        candle.candle.timestamp >=
            session_open_timestamp_ &&

        candle.candle.timestamp <
            session_close_timestamp_;
}


// ============================================================================
// Filter stale state
//
// MultiTimeframeCursor may still contain yesterday's latest candle
// immediately after the date changes.
//
// SessionState removes it.
//
// IMPORTANT:
//
// We do NOT alter the cursor's source streams.
// We only remove stale values from the current state/snapshot.
// ============================================================================

void SessionState::filter(
    MultiTimeframeState& state) const
{
    if (!initialized_)
    {
        state.one_minute.reset();
        state.five_minute.reset();
        state.fifteen_minute.reset();

        return;
    }


    // Outside regular market hours we expose no actionable
    // current-session candle state.
    if (phase_ != SessionPhase::ACTIVE)
    {
        state.one_minute.reset();
        state.five_minute.reset();
        state.fifteen_minute.reset();

        return;
    }


    if (
        state.one_minute.has_value() &&
        !belongsToCurrentSession(
            *state.one_minute)
    )
    {
        state.one_minute.reset();
    }


    if (
        state.five_minute.has_value() &&
        !belongsToCurrentSession(
            *state.five_minute)
    )
    {
        state.five_minute.reset();
    }


    if (
        state.fifteen_minute.has_value() &&
        !belongsToCurrentSession(
            *state.fifteen_minute)
    )
    {
        state.fifteen_minute.reset();
    }
}


// ============================================================================
// Reset
// ============================================================================

void SessionState::reset() noexcept
{
    initialized_ =
        false;

    trading_date_ =
        TradingDate{};

    phase_ =
        SessionPhase::BEFORE_OPEN;

    session_open_timestamp_ =
        0;

    session_close_timestamp_ =
        0;
}

}