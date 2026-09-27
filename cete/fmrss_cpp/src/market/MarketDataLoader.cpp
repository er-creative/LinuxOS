#include "devai/market/MarketDataLoader.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace devai::market
{

namespace
{

// ---------------------------------------------------------
// Portable conversion of local calendar time to Unix time.
//
// IMPORTANT:
//
// Your historical Indian market timestamps are interpreted
// as local machine time.
//
// Your machine is configured for Asia/Kolkata, so this
// matches the historical market timestamps.
//
// Later, before production/live deployment, we can replace
// this with explicit exchange-time handling.
// ---------------------------------------------------------

std::int64_t localTimeToUnix(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second)
{
    std::tm tm{};

    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;

    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = second;

    tm.tm_isdst = -1;

    const std::time_t timestamp = std::mktime(&tm);

    if (timestamp == static_cast<std::time_t>(-1))
    {
        throw std::runtime_error(
            "Unable to convert date/time to timestamp."
        );
    }

    return static_cast<std::int64_t>(timestamp);
}


// ---------------------------------------------------------
// Hash key used only for duplicate detection.
//
// We treat:
//
//     same symbol + same timestamp
//
// as the same candle.
// ---------------------------------------------------------

std::string candleKey(const Candle& candle)
{
    return candle.symbol +
           "#" +
           std::to_string(candle.timestamp);
}

} // anonymous namespace


// =========================================================
// Constructor
// =========================================================

MarketDataLoader::MarketDataLoader(
    std::filesystem::path data_folder)
    : data_folder_(std::move(data_folder))
{
    if (data_folder_.empty())
    {
        throw std::invalid_argument(
            "MarketDataLoader: data folder is empty."
        );
    }
}


// =========================================================
// Data Folder
// =========================================================

const std::filesystem::path&
MarketDataLoader::dataFolder() const noexcept
{
    return data_folder_;
}


// =========================================================
// Trim
// =========================================================

std::string
MarketDataLoader::trim(std::string value)
{
    const auto not_space =
        [](unsigned char character)
        {
            return !std::isspace(character);
        };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            not_space
        )
    );

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            not_space
        ).base(),
        value.end()
    );

    return value;
}


// =========================================================
// Split Line
//
// Supports:
//
//     comma-separated
//     tab-separated
//     whitespace-separated
//
// This makes the loader tolerant of the common text formats
// used by the existing market-data files.
// =========================================================

std::vector<std::string>
MarketDataLoader::splitLine(
    const std::string& line)
{
    std::vector<std::string> fields;

    if (line.find(',') != std::string::npos)
    {
        std::stringstream stream(line);
        std::string field;

        while (std::getline(stream, field, ','))
        {
            fields.push_back(trim(field));
        }

        return fields;
    }

    if (line.find('\t') != std::string::npos)
    {
        std::stringstream stream(line);
        std::string field;

        while (std::getline(stream, field, '\t'))
        {
            fields.push_back(trim(field));
        }

        return fields;
    }

    std::stringstream stream(line);
    std::string field;

    while (stream >> field)
    {
        fields.push_back(trim(field));
    }

    return fields;
}


// =========================================================
// Header Detection
// =========================================================

bool
MarketDataLoader::looksLikeHeader(
    const std::vector<std::string>& fields)
{
    if (fields.empty())
        return false;

    std::string first = fields.front();

    std::transform(
        first.begin(),
        first.end(),
        first.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(
                std::tolower(c)
            );
        }
    );

    return first == "symbol" ||
           first == "ticker";
}


// =========================================================
// Parse Timestamp
//
// Accepted:
//
// Date:
//     YYYY-MM-DD
//
// Time:
//     HH:MM
//     HH:MM:SS
//
// Example:
//
//     2026-09-21
//     09:15
// =========================================================

std::int64_t
MarketDataLoader::parseTimestamp(
    const std::string& date,
    const std::string& time)
{
    int year   = 0;
    int month  = 0;
    int day    = 0;

    int hour   = 0;
    int minute = 0;
    int second = 0;

    char dash1 = '\0';
    char dash2 = '\0';

    std::stringstream date_stream(date);

    date_stream
        >> year
        >> dash1
        >> month
        >> dash2
        >> day;

    if (!date_stream ||
        dash1 != '-' ||
        dash2 != '-')
    {
        throw std::runtime_error(
            "Invalid date: " + date
        );
    }

    char colon1 = '\0';
    char colon2 = '\0';

    std::stringstream time_stream(time);

    time_stream
        >> hour
        >> colon1
        >> minute;

    if (!time_stream ||
        colon1 != ':')
    {
        throw std::runtime_error(
            "Invalid time: " + time
        );
    }

    // Optional seconds.
    if (time_stream.peek() == ':')
    {
        time_stream >> colon2 >> second;

        if (!time_stream ||
            colon2 != ':')
        {
            throw std::runtime_error(
                "Invalid time: " + time
            );
        }
    }

    if (month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59)
    {
        throw std::runtime_error(
            "Date/time outside valid range."
        );
    }

    return localTimeToUnix(
        year,
        month,
        day,
        hour,
        minute,
        second
    );
}


// =========================================================
// Parse Candle
//
// Expected:
//
// Symbol Date Time Open High Low Close Volume
//
// Exactly eight logical fields.
// =========================================================

Candle
MarketDataLoader::parseCandle(
    const std::vector<std::string>& fields)
{
    if (fields.size() != 8)
    {
        throw std::runtime_error(
            "Expected 8 columns, received " +
            std::to_string(fields.size())
        );
    }

    Candle candle;

    candle.symbol = trim(fields[0]);

    candle.timestamp =
        parseTimestamp(
            trim(fields[1]),
            trim(fields[2])
        );

    candle.open =
        std::stod(trim(fields[3]));

    candle.high =
        std::stod(trim(fields[4]));

    candle.low =
        std::stod(trim(fields[5]));

    candle.close =
        std::stod(trim(fields[6]));

    const double raw_volume =
        std::stod(trim(fields[7]));

    if (raw_volume < 0.0)
    {
        throw std::runtime_error(
            "Negative volume."
        );
    }

    candle.volume =
        static_cast<std::uint64_t>(
            raw_volume
        );

    if (!candle.valid())
    {
        throw std::runtime_error(
            "OHLCV validation failed."
        );
    }

    return candle;
}


// =========================================================
// Load Relative Filename
// =========================================================

MarketDataLoader::LoadResult
MarketDataLoader::loadFile(
    const std::string& filename) const
{
    if (filename.empty())
    {
        throw std::invalid_argument(
            "MarketDataLoader: filename is empty."
        );
    }

    return loadPath(
        data_folder_ / filename
    );
}


// =========================================================
// Load Full Path
// =========================================================

MarketDataLoader::LoadResult
MarketDataLoader::loadPath(
    const std::filesystem::path& file_path) const
{
    if (!std::filesystem::exists(file_path))
    {
        throw std::runtime_error(
            "Market data file not found: " +
            file_path.string()
        );
    }

    if (!std::filesystem::is_regular_file(file_path))
    {
        throw std::runtime_error(
            "Market data path is not a regular file: " +
            file_path.string()
        );
    }

    std::ifstream input(file_path);

    if (!input.is_open())
    {
        throw std::runtime_error(
            "Unable to open market data file: " +
            file_path.string()
        );
    }

    LoadResult result;

    // Reserve a reasonable starting amount.
    //
    // This is only capacity, not actual candle allocation.
    // The vector automatically grows if the file is larger.
    result.candles.reserve(100000);

    std::unordered_set<std::string> seen;

    seen.reserve(100000);

    std::string line;

    std::size_t line_number = 0;

    while (std::getline(input, line))
    {
        ++line_number;

        line = trim(line);

        if (line.empty())
            continue;

        const auto fields =
            splitLine(line);

        if (fields.empty())
            continue;

        if (looksLikeHeader(fields))
            continue;

        ++result.total_rows;

        try
        {
            Candle candle =
                parseCandle(fields);

            const std::string key =
                candleKey(candle);

            if (!seen.insert(key).second)
            {
                ++result.duplicate_rows;
                ++result.skipped_rows;

                continue;
            }

            result.candles.push_back(
                std::move(candle)
            );

            ++result.loaded_rows;
        }
        catch (const std::exception& error)
        {
            ++result.invalid_rows;
            ++result.skipped_rows;

            std::cerr
                << "[MarketDataLoader] "
                << "Skipping line "
                << line_number
                << ": "
                << error.what()
                << '\n';
        }
    }

    // -----------------------------------------------------
    // Sort chronologically.
    //
    // Symbol is included first so the same loader can later
    // safely handle a multi-symbol input file if required.
    // -----------------------------------------------------

    std::sort(
        result.candles.begin(),
        result.candles.end(),
        [](const Candle& lhs,
           const Candle& rhs)
        {
            if (lhs.symbol != rhs.symbol)
            {
                return lhs.symbol <
                       rhs.symbol;
            }

            return lhs.timestamp <
                   rhs.timestamp;
        }
    );

    return result;
}

} // namespace devai::market