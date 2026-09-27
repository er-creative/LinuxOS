#pragma once

#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace devai::features
{

class PriceReturnFeatureEngine
{
public:
    explicit PriceReturnFeatureEngine(
        std::string symbol,
        std::size_t maximum_history = 64
    );

    [[nodiscard]]
    PriceReturnFeatures update(
        const devai::market::MarketSnapshot& snapshot
    );

    void reset() noexcept;

    [[nodiscard]]
    const std::string& symbol() const noexcept
    {
        return symbol_;
    }

    [[nodiscard]]
    std::size_t oneMinuteHistorySize() const noexcept
    {
        return one_minute_history_.size();
    }

    [[nodiscard]]
    std::size_t fiveMinuteHistorySize() const noexcept
    {
        return five_minute_history_.size();
    }

    [[nodiscard]]
    std::size_t fifteenMinuteHistorySize() const noexcept
    {
        return fifteen_minute_history_.size();
    }

private:
    std::string symbol_;

    std::size_t maximum_history_{64};

    std::deque<devai::market::Candle>
        one_minute_history_;

    std::deque<devai::market::Candle>
        five_minute_history_;

    std::deque<devai::market::Candle>
        fifteen_minute_history_;

    std::optional<std::int64_t>
        last_decision_time_;


    // ------------------------------------------------------
    // History
    // ------------------------------------------------------

    static bool appendIfNew(
        std::deque<devai::market::Candle>& history,
        const std::optional<devai::market::Candle>& candle,
        std::size_t maximum_history
    );


    // ------------------------------------------------------
    // Returns
    // ------------------------------------------------------

    static double candleReturn(
        const devai::market::Candle& candle
    );

    static double closeToCloseReturn(
        const std::deque<devai::market::Candle>& history
    );

    static double logReturn(
        const std::deque<devai::market::Candle>& history
    );

    static double nBarReturn(
        const std::deque<devai::market::Candle>& history,
        std::size_t bars
    );


    // ------------------------------------------------------
    // Candle geometry
    // ------------------------------------------------------

    static double candleBodyPercent(
        const devai::market::Candle& candle
    );

    static double candleRangePercent(
        const devai::market::Candle& candle
    );

    static double closeLocation(
        const devai::market::Candle& candle
    );
};

}