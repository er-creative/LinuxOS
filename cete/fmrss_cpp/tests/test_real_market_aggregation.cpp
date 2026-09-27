#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using devai::market::Candle;
using devai::market::CandleAggregator;
using devai::market::MarketDataLoader;
using devai::market::Timeframe;


// =========================================================
// Timestamp Formatting
// =========================================================

std::string formatTimestamp(std::int64_t timestamp)
{
    const std::time_t raw_time =
        static_cast<std::time_t>(timestamp);

    std::tm local_tm{};

#if defined(_WIN32)
    localtime_s(&local_tm, &raw_time);
#else
    localtime_r(&raw_time, &local_tm);
#endif

    char buffer[32]{};

    std::strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &local_tm
    );

    return buffer;
}


// =========================================================
// Print One Candle
// =========================================================

void printCandle(
    const std::string& label,
    const Candle& candle)
{
    std::cout
        << std::left
        << std::setw(6)
        << label

        << " "
        << formatTimestamp(candle.timestamp)

        << "  O="
        << std::fixed
        << std::setprecision(2)
        << candle.open

        << "  H="
        << candle.high

        << "  L="
        << candle.low

        << "  C="
        << candle.close

        << "  V="
        << candle.volume

        << '\n';
}


// =========================================================
// Find Candle By Timestamp
// =========================================================

const Candle* findCandle(
    const std::vector<Candle>& candles,
    std::int64_t timestamp)
{
    const auto iterator =
        std::lower_bound(
            candles.begin(),
            candles.end(),
            timestamp,
            [](const Candle& candle,
               std::int64_t value)
            {
                return candle.timestamp < value;
            }
        );

    if (iterator == candles.end() ||
        iterator->timestamp != timestamp)
    {
        return nullptr;
    }

    return &(*iterator);
}


// =========================================================
// Verify One 5-Minute Candle Against Its 1-Minute Sources
// =========================================================

bool verifyFiveMinuteCandle(
    const std::vector<Candle>& one_minute,
    const Candle& five_minute)
{
    constexpr std::int64_t one_minute_seconds = 60;

    const Candle* first =
        findCandle(
            one_minute,
            five_minute.timestamp
        );

    if (first == nullptr)
    {
        return false;
    }

    double expected_high =
        first->high;

    double expected_low =
        first->low;

    std::uint64_t expected_volume = 0;

    const double expected_open =
        first->open;

    double expected_close =
        first->close;

    for (int minute = 0; minute < 5; ++minute)
    {
        const std::int64_t expected_timestamp =
            five_minute.timestamp +
            minute * one_minute_seconds;

        const Candle* source =
            findCandle(
                one_minute,
                expected_timestamp
            );

        if (source == nullptr)
        {
            return false;
        }

        expected_high =
            std::max(
                expected_high,
                source->high
            );

        expected_low =
            std::min(
                expected_low,
                source->low
            );

        expected_close =
            source->close;

        expected_volume +=
            source->volume;
    }

    return
        five_minute.open == expected_open &&
        five_minute.high == expected_high &&
        five_minute.low == expected_low &&
        five_minute.close == expected_close &&
        five_minute.volume == expected_volume;
}


// =========================================================
// Main
// =========================================================

} // anonymous namespace


int main(int argc, char* argv[])
{
    using namespace devai::market;

    try
    {
        // -------------------------------------------------
        // Usage:
        //
        // ./build/test_real_market_aggregation \
        //     /path/to/RELIANCE_1mins.txt
        //
        // Keeping the filename as a command-line argument
        // avoids hard-coding your data directory into C++.
        // -------------------------------------------------

        if (argc != 2)
        {
            std::cerr
                << "Usage:\n\n"
                << "  "
                << argv[0]
                << " <1-minute-market-data-file>\n\n";

            return 1;
        }

        const std::filesystem::path file_path =
            argv[1];

        if (!std::filesystem::exists(file_path))
        {
            throw std::runtime_error(
                "File does not exist: " +
                file_path.string()
            );
        }

        // -------------------------------------------------
        // MarketDataLoader requires a data folder.
        //
        // We use the parent directory and then load the
        // selected file.
        // -------------------------------------------------

        const std::filesystem::path data_folder =
            file_path.parent_path();

        const std::string filename =
            file_path.filename().string();

        MarketDataLoader loader(
            data_folder
        );

        std::cout
            << "\n"
            << "============================================================\n"
            << "REAL MARKET DATA AGGREGATION TEST\n"
            << "============================================================\n"
            << "File : "
            << file_path
            << '\n';


        // =================================================
        // Load Real 1-Minute Data
        // =================================================

        const auto load_result =
            loader.loadFile(filename);

        const std::vector<Candle>& one_minute =
            load_result.candles;

        if (one_minute.empty())
        {
            throw std::runtime_error(
                "No valid 1-minute candles were loaded."
            );
        }

        const std::string symbol =
            one_minute.front().symbol;

        std::cout
            << "Symbol                   : "
            << symbol
            << '\n'

            << "Total data rows          : "
            << load_result.total_rows
            << '\n'

            << "1-minute candles loaded  : "
            << load_result.loaded_rows
            << '\n'

            << "Invalid rows             : "
            << load_result.invalid_rows
            << '\n'

            << "Duplicate rows           : "
            << load_result.duplicate_rows
            << '\n'

            << "Skipped rows             : "
            << load_result.skipped_rows
            << '\n';


        // =================================================
        // Aggregate 1m -> 5m / 15m
        // =================================================

        CandleAggregator aggregator;

        const auto five_minute =
            aggregator.aggregate(
                one_minute,
                Timeframe::FIVE_MINUTES
            );

        const auto fifteen_minute =
            aggregator.aggregate(
                one_minute,
                Timeframe::FIFTEEN_MINUTES
            );


        // =================================================
        // Summary
        // =================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "5-MINUTE AGGREGATION\n"
            << "------------------------------------------------------------\n"

            << "Candidate buckets        : "
            << five_minute.total_buckets
            << '\n'

            << "Completed candles        : "
            << five_minute.completed_buckets
            << '\n'

            << "Incomplete buckets       : "
            << five_minute.incomplete_buckets
            << '\n'

            << "Invalid input candles    : "
            << five_minute.invalid_input_candles
            << '\n'

            << "Duplicate input candles  : "
            << five_minute.duplicate_input_candles
            << '\n';


        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "15-MINUTE AGGREGATION\n"
            << "------------------------------------------------------------\n"

            << "Candidate buckets        : "
            << fifteen_minute.total_buckets
            << '\n'

            << "Completed candles        : "
            << fifteen_minute.completed_buckets
            << '\n'

            << "Incomplete buckets       : "
            << fifteen_minute.incomplete_buckets
            << '\n'

            << "Invalid input candles    : "
            << fifteen_minute.invalid_input_candles
            << '\n'

            << "Duplicate input candles  : "
            << fifteen_minute.duplicate_input_candles
            << '\n';


        // =================================================
        // Display First Five 1-Minute Candles
        // =================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "FIRST 1-MINUTE CANDLES\n"
            << "------------------------------------------------------------\n";

        const std::size_t one_minute_samples =
            std::min<std::size_t>(
                5,
                one_minute.size()
            );

        for (std::size_t i = 0;
             i < one_minute_samples;
             ++i)
        {
            printCandle(
                "1m",
                one_minute[i]
            );
        }


        // =================================================
        // Display First Five 5-Minute Candles
        // =================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "FIRST 5-MINUTE CANDLES\n"
            << "------------------------------------------------------------\n";

        const std::size_t five_minute_samples =
            std::min<std::size_t>(
                5,
                five_minute.candles.size()
            );

        for (std::size_t i = 0;
             i < five_minute_samples;
             ++i)
        {
            printCandle(
                "5m",
                five_minute.candles[i]
            );
        }


        // =================================================
        // Display First Three 15-Minute Candles
        // =================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "FIRST 15-MINUTE CANDLES\n"
            << "------------------------------------------------------------\n";

        const std::size_t fifteen_minute_samples =
            std::min<std::size_t>(
                3,
                fifteen_minute.candles.size()
            );

        for (std::size_t i = 0;
             i < fifteen_minute_samples;
             ++i)
        {
            printCandle(
                "15m",
                fifteen_minute.candles[i]
            );
        }


        // =================================================
        // Automatic OHLCV Verification
        // =================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "AUTOMATIC 5-MINUTE OHLCV VERIFICATION\n"
            << "------------------------------------------------------------\n";

        if (five_minute.candles.empty())
        {
            throw std::runtime_error(
                "No complete 5-minute candles generated."
            );
        }

        std::size_t verified = 0;
        std::size_t failed = 0;

        for (const Candle& candle :
             five_minute.candles)
        {
            if (verifyFiveMinuteCandle(
                    one_minute,
                    candle))
            {
                ++verified;
            }
            else
            {
                ++failed;
            }
        }

        std::cout
            << "Verified                 : "
            << verified
            << '\n'

            << "Failed                   : "
            << failed
            << '\n';


        // =================================================
        // Final Result
        // =================================================

        std::cout
            << "\n"
            << "============================================================\n";

        if (failed == 0)
        {
            std::cout
                << "REAL MARKET AGGREGATION TEST PASSED\n";
        }
        else
        {
            std::cout
                << "REAL MARKET AGGREGATION TEST FAILED\n";
        }

        std::cout
            << "============================================================\n";

        return failed == 0 ? 0 : 2;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "\n[ERROR] "
            << error.what()
            << '\n';

        return 1;
    }
}