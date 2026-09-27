#pragma once

#include "devai/market/Candle.hpp"
#include "devai/market/SessionState.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace devai::market
{

struct MarketSnapshot
{
    std::string symbol;

    std::int64_t decision_time{0};

    TradingDate trading_date{};

    SessionPhase session_phase{
        SessionPhase::BEFORE_OPEN
    };

    bool new_session{false};

    std::optional<Candle> one_minute;
    std::optional<Candle> five_minute;
    std::optional<Candle> fifteen_minute;


    [[nodiscard]]
    bool sessionActive() const noexcept
    {
        return session_phase ==
            SessionPhase::ACTIVE;
    }


    [[nodiscard]]
    bool hasOneMinute() const noexcept
    {
        return one_minute.has_value();
    }


    [[nodiscard]]
    bool hasFiveMinute() const noexcept
    {
        return five_minute.has_value();
    }


    [[nodiscard]]
    bool hasFifteenMinute() const noexcept
    {
        return fifteen_minute.has_value();
    }


    [[nodiscard]]
    bool fullySynchronized() const noexcept
    {
        return
            sessionActive() &&
            one_minute.has_value() &&
            five_minute.has_value() &&
            fifteen_minute.has_value();
    }
};

}