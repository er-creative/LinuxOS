#pragma once

#include "devai/market/HistoricalDataProvider.hpp"
#include "devai/market/MarketDataLoader.hpp"

#include <filesystem>

namespace devai::market
{

class FileHistoricalDataProvider final
    : public HistoricalDataProvider
{
public:
    explicit FileHistoricalDataProvider(
        std::filesystem::path source_folder
    );

    [[nodiscard]]
    std::vector<Candle> fetchOneMinute(
        const std::string& symbol,
        std::int64_t from_timestamp,
        std::int64_t to_timestamp
    ) override;

private:
    std::filesystem::path source_folder_;
};

}