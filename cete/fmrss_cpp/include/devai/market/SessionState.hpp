#pragma once

#include "devai/market/MultiTimeframeSynchronizer.hpp"

#include <cstdint>

namespace devai::market
{

enum class SessionPhase
{
    BEFORE_OPEN,
    ACTIVE,
    AFTER_CLOSE
};


struct TradingDate
{
    int year{0};
    int month{0};
    int day{0};

    [[nodiscard]]
    bool operator==(
        const TradingDate& other) const noexcept
    {
        return
            year == other.year &&
            month == other.month &&
            day == other.day;
    }

    [[nodiscard]]
    bool operator!=(
        const TradingDate& other) const noexcept
    {
        return !(*this == other);
    }
};


class SessionState
{
public:

    SessionState(
        int session_open_hour = 9,
        int session_open_minute = 15,
        int session_close_hour = 15,
        int session_close_minute = 30
    );


    // Update session information for a decision timestamp.
    //
    // Returns true if this timestamp belongs to a different
    // calendar trading date than the previously observed timestamp.
    [[nodiscard]]
    bool update(
        std::int64_t decision_time
    );


    [[nodiscard]]
    SessionPhase phase() const noexcept
    {
        return phase_;
    }


    [[nodiscard]]
    bool active() const noexcept
    {
        return phase_ ==
            SessionPhase::ACTIVE;
    }


    [[nodiscard]]
    bool beforeOpen() const noexcept
    {
        return phase_ ==
            SessionPhase::BEFORE_OPEN;
    }


    [[nodiscard]]
    bool afterClose() const noexcept
    {
        return phase_ ==
            SessionPhase::AFTER_CLOSE;
    }


    [[nodiscard]]
    bool initialized() const noexcept
    {
        return initialized_;
    }


    [[nodiscard]]
    const TradingDate& tradingDate() const noexcept
    {
        return trading_date_;
    }


    [[nodiscard]]
    std::int64_t sessionOpenTimestamp() const noexcept
    {
        return session_open_timestamp_;
    }


    [[nodiscard]]
    std::int64_t sessionCloseTimestamp() const noexcept
    {
        return session_close_timestamp_;
    }


    // Remove candles from MultiTimeframeState that do not
    // belong to the current trading session.
    //
    // This is the protection against previous-day carryover.
    void filter(
        MultiTimeframeState& state
    ) const;


    void reset() noexcept;


private:

    int open_hour_;
    int open_minute_;

    int close_hour_;
    int close_minute_;

    bool initialized_{false};

    TradingDate trading_date_{};

    SessionPhase phase_{
        SessionPhase::BEFORE_OPEN
    };

    std::int64_t session_open_timestamp_{0};
    std::int64_t session_close_timestamp_{0};


    [[nodiscard]]
    static TradingDate dateFromTimestamp(
        std::int64_t timestamp
    );


    [[nodiscard]]
    static std::int64_t makeLocalTimestamp(
        const TradingDate& date,
        int hour,
        int minute
    );


    [[nodiscard]]
    bool belongsToCurrentSession(
        const TimedCandle& candle
    ) const noexcept;
};

}