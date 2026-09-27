#pragma once

#include "devai/market/MarketSnapshot.hpp"

#include <cstdint>

namespace devai::market
{

struct StockBenchmarkSnapshot
{
    MarketSnapshot stock;
    MarketSnapshot benchmark;

    std::int64_t decision_time{0};

    [[nodiscard]]
    bool sameDecisionTime() const noexcept
    {
        return
            stock.decision_time == decision_time &&
            benchmark.decision_time == decision_time;
    }

    [[nodiscard]]
    bool sameTradingDate() const noexcept
    {
        return
            stock.trading_date ==
            benchmark.trading_date;
    }

    [[nodiscard]]
    bool sessionsActive() const noexcept
    {
        return
            stock.sessionActive() &&
            benchmark.sessionActive();
    }

    [[nodiscard]]
    bool stockFullySynchronized() const noexcept
    {
        return stock.fullySynchronized();
    }

    [[nodiscard]]
    bool benchmarkFullySynchronized() const noexcept
    {
        return benchmark.fullySynchronized();
    }

    [[nodiscard]]
    bool fullySynchronized() const noexcept
    {
        return
            sameDecisionTime() &&
            sameTradingDate() &&
            sessionsActive() &&
            stockFullySynchronized() &&
            benchmarkFullySynchronized();
    }
};

}