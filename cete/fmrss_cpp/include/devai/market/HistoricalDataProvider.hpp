#pragma once

#include "devai/market/Candle.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace devai::market
{

class HistoricalDataProvider
{
public:
    virtual ~HistoricalDataProvider() = default;

    [[nodiscard]]
    virtual std::vector<Candle> fetchOneMinute(
        const std::string& symbol,
        std::int64_t from_timestamp,
        std::int64_t to_timestamp
    ) = 0;
};

}