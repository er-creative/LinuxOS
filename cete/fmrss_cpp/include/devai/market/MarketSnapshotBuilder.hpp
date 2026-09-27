#pragma once

#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/SessionState.hpp"

#include <cstdint>
#include <string>

namespace devai::market
{

class MarketSnapshotBuilder
{
public:

    explicit MarketSnapshotBuilder(
        std::string symbol
    );


    [[nodiscard]]
    MarketSnapshot build(
        std::int64_t decision_time,
        MultiTimeframeCursor& cursor,
        SessionState& session
    ) const;


    [[nodiscard]]
    const std::string& symbol() const noexcept
    {
        return symbol_;
    }


private:

    std::string symbol_;


    static void validateCandleSymbol(
        const std::optional<TimedCandle>& candle,
        const std::string& expected_symbol,
        const char* timeframe_name
    );
};

}