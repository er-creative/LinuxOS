#include "devai/market/FileHistoricalDataProvider.hpp"

#include <stdexcept>
#include <utility>

namespace devai::market
{

FileHistoricalDataProvider::FileHistoricalDataProvider(
    std::filesystem::path source_folder)
    : source_folder_(
        std::move(source_folder))
{
    if (source_folder_.empty())
    {
        throw std::invalid_argument(
            "Historical source folder cannot be empty."
        );
    }
}


std::vector<Candle>
FileHistoricalDataProvider::fetchOneMinute(
    const std::string& symbol,
    std::int64_t from_timestamp,
    std::int64_t to_timestamp)
{
    if (symbol.empty())
    {
        throw std::invalid_argument(
            "Symbol cannot be empty."
        );
    }

    if (from_timestamp > to_timestamp)
    {
        throw std::invalid_argument(
            "Invalid historical-data time range."
        );
    }


    MarketDataLoader loader(
        source_folder_
    );


    const std::string filename =
        symbol + "_1min.txt";


    const auto result =
        loader.loadFile(
            filename
        );


    std::vector<Candle> selected;


    for (const Candle& candle :
         result.candles)
    {
        if (candle.symbol != symbol)
        {
            continue;
        }


        if (
            candle.timestamp <
            from_timestamp
        )
        {
            continue;
        }


        if (
            candle.timestamp >
            to_timestamp
        )
        {
            continue;
        }


        selected.push_back(
            candle
        );
    }


    return selected;
}

}