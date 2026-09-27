#include "devai/market/FileHistoricalDataProvider.hpp"

#include <cstdlib>
#include <iostream>

using namespace devai::market;


int main(
    int argc,
    char* argv[])
{
    if (argc != 5)
    {
        std::cerr
            << "Usage:\n"
            << argv[0]
            << " <source-folder>"
            << " <symbol>"
            << " <from-epoch>"
            << " <to-epoch>\n";

        return 1;
    }


    const std::filesystem::path folder =
        argv[1];

    const std::string symbol =
        argv[2];

    const std::int64_t from =
        std::stoll(argv[3]);

    const std::int64_t to =
        std::stoll(argv[4]);


    try
    {
        FileHistoricalDataProvider provider(
            folder
        );


        const auto candles =
            provider.fetchOneMinute(
                symbol,
                from,
                to
            );


        std::cout
            << "\n"
            << "============================================\n"
            << "HistoricalDataProvider Test\n"
            << "============================================\n"
            << "Symbol            : "
            << symbol
            << '\n'
            << "Candles returned  : "
            << candles.size()
            << '\n';


        for (const Candle& candle :
             candles)
        {
            std::cout
                << candle.symbol
                << "  "
                << candle.timestamp
                << "  "
                << candle.open
                << "  "
                << candle.high
                << "  "
                << candle.low
                << "  "
                << candle.close
                << "  "
                << candle.volume
                << '\n';
        }


        std::cout
            << "============================================\n";
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ERROR: "
            << error.what()
            << '\n';

        return 1;
    }


    return 0;
}