#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

#include "devai/market/MarketDataLoader.hpp"

namespace
{

void printCandle(
    const devai::market::Candle& candle)
{
    std::time_t raw_time =
        static_cast<std::time_t>(
            candle.timestamp
        );

    std::tm* local =
        std::localtime(&raw_time);

    std::cout
        << candle.symbol
        << " | ";

    if (local != nullptr)
    {
        std::cout
            << std::put_time(
                   local,
                   "%Y-%m-%d %H:%M:%S"
               );
    }
    else
    {
        std::cout << candle.timestamp;
    }

    std::cout
        << " | O=" << candle.open
        << " H=" << candle.high
        << " L=" << candle.low
        << " C=" << candle.close
        << " V=" << candle.volume
        << '\n';
}

} // anonymous namespace


int main(int argc, char* argv[])
{
    using devai::market::MarketDataLoader;

    std::cout
        << "========================================\n"
        << "DevAI MarketDataLoader Test\n"
        << "========================================\n";

    if (argc != 2)
    {
        std::cerr
            << "\nUsage:\n\n"
            << "    ./test_market_data_loader "
            << "<filename>\n\n"
            << "Example:\n\n"
            << "    ./test_market_data_loader "
            << "RELIANCE_5mins.txt\n\n";

        return 1;
    }

    const std::string filename = argv[1];

    try
    {
        MarketDataLoader loader(
            "/home/hadoop/shareMarket_Data"
        );

        std::cout
            << "Data folder : "
            << loader.dataFolder()
            << '\n';

        std::cout
            << "File        : "
            << filename
            << "\n\n";

        auto result =
            loader.loadFile(filename);

        std::cout
            << "----------------------------------------\n"
            << "LOAD SUMMARY\n"
            << "----------------------------------------\n"
            << "Rows read       : "
            << result.total_rows
            << '\n'
            << "Candles loaded  : "
            << result.loaded_rows
            << '\n'
            << "Invalid rows    : "
            << result.invalid_rows
            << '\n'
            << "Duplicate rows  : "
            << result.duplicate_rows
            << '\n'
            << "Skipped rows    : "
            << result.skipped_rows
            << '\n';

        if (result.candles.empty())
        {
            std::cerr
                << "\nFAILED: No candles loaded.\n";

            return 1;
        }

        std::cout
            << "\n----------------------------------------\n"
            << "FIRST CANDLE\n"
            << "----------------------------------------\n";

        printCandle(
            result.candles.front()
        );

        std::cout
            << "\n----------------------------------------\n"
            << "LAST CANDLE\n"
            << "----------------------------------------\n";

        printCandle(
            result.candles.back()
        );

        // -------------------------------------------------
        // Verify chronological ordering.
        // -------------------------------------------------

        for (std::size_t i = 1;
             i < result.candles.size();
             ++i)
        {
            const auto& previous =
                result.candles[i - 1];

            const auto& current =
                result.candles[i];

            if (previous.symbol ==
                    current.symbol &&
                previous.timestamp >=
                    current.timestamp)
            {
                std::cerr
                    << "\nFAILED: Candles are not "
                    << "strictly chronological.\n";

                return 1;
            }
        }

        std::cout
            << "\n========================================\n"
            << "PASS\n"
            << "MarketDataLoader is working.\n"
            << "========================================\n";

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "\nFAILED\n"
            << error.what()
            << '\n';

        return 1;
    }
}