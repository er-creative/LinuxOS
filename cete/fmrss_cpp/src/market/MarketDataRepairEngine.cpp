#include "devai/market/MarketDataRepairEngine.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace devai::market
{

namespace
{

constexpr std::int64_t ONE_MINUTE_SECONDS = 60;


// ============================================================================
// Local Time
// ============================================================================

std::tm toLocalTime(
    std::int64_t timestamp)
{
    const std::time_t raw =
        static_cast<std::time_t>(
            timestamp
        );

    std::tm result{};

#if defined(_WIN32)

    localtime_s(
        &result,
        &raw
    );

#else

    localtime_r(
        &raw,
        &result
    );

#endif

    return result;
}


// ============================================================================
// Timestamp -> Date/Time String
// ============================================================================

std::string formatDate(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(timestamp);

    std::ostringstream output;

    output
        << std::setfill('0')
        << std::setw(4)
        << value.tm_year + 1900
        << "-"
        << std::setw(2)
        << value.tm_mon + 1
        << "-"
        << std::setw(2)
        << value.tm_mday;

    return output.str();
}


std::string formatTime(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(timestamp);

    std::ostringstream output;

    output
        << std::setfill('0')
        << std::setw(2)
        << value.tm_hour
        << ":"
        << std::setw(2)
        << value.tm_min
        << ":"
        << std::setw(2)
        << value.tm_sec;

    return output.str();
}


// ============================================================================
// Key used for duplicate detection
// ============================================================================

struct CandleKey
{
    std::string symbol;
    std::int64_t timestamp{0};

    bool operator<(
        const CandleKey& other) const noexcept
    {
        if (symbol != other.symbol)
        {
            return symbol < other.symbol;
        }

        return timestamp < other.timestamp;
    }
};

} // anonymous namespace


// ============================================================================
// Same Trading Date
// ============================================================================

bool MarketDataRepairEngine::sameTradingDate(
    std::int64_t left,
    std::int64_t right)
{
    const std::tm left_time =
        toLocalTime(left);

    const std::tm right_time =
        toLocalTime(right);

    return
        left_time.tm_year == right_time.tm_year &&
        left_time.tm_mon  == right_time.tm_mon &&
        left_time.tm_mday == right_time.tm_mday;
}


// ============================================================================
// Regular Session
//
// Valid candle START times:
//
// 09:15 ... 15:29
// ============================================================================

bool MarketDataRepairEngine::insideRegularSession(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(timestamp);

    const int minutes =
        value.tm_hour * 60 +
        value.tm_min;

    constexpr int SESSION_START =
        9 * 60 + 15;

    constexpr int SESSION_LAST =
        15 * 60 + 29;

    return
        minutes >= SESSION_START &&
        minutes <= SESSION_LAST;
}


// ============================================================================
// Validate Downloaded Candle
// ============================================================================

bool MarketDataRepairEngine::validReplacement(
    const Candle& candle,
    const std::string& expected_symbol)
{
    if (!candle.valid())
    {
        return false;
    }


    if (candle.symbol != expected_symbol)
    {
        return false;
    }


    if (!insideRegularSession(
            candle.timestamp))
    {
        return false;
    }


    if (
        candle.timestamp %
        ONE_MINUTE_SECONDS != 0
    )
    {
        return false;
    }


    if (
        !std::isfinite(candle.open) ||
        !std::isfinite(candle.high) ||
        !std::isfinite(candle.low) ||
        !std::isfinite(candle.close)
    )
    {
        return false;
    }


    if (
        candle.open <= 0.0 ||
        candle.high <= 0.0 ||
        candle.low <= 0.0 ||
        candle.close <= 0.0
    )
    {
        return false;
    }


    if (
        candle.high < candle.open ||
        candle.high < candle.close ||
        candle.low > candle.open ||
        candle.low > candle.close ||
        candle.high < candle.low
    )
    {
        return false;
    }


    return true;
}


// ============================================================================
// Find Internal Gaps
//
// IMPORTANT:
//
// This deliberately finds ONLY missing candles between two existing candles
// on the SAME trading date.
//
// It does not treat:
//
//   * overnight
//   * weekends
//   * holidays
//   * late session starts
//   * early session endings
//
// as internal gaps.
// ============================================================================

std::vector<MissingRange>
MarketDataRepairEngine::findInternalGaps(
    const std::string& symbol,
    const std::vector<Candle>& candles) const
{
    std::vector<Candle> ordered;

    ordered.reserve(
        candles.size()
    );


    // ========================================================================
    // Keep only requested symbol
    // ========================================================================

    for (const Candle& candle :
         candles)
    {
        if (
            candle.symbol == symbol &&
            insideRegularSession(
                candle.timestamp)
        )
        {
            ordered.push_back(
                candle
            );
        }
    }


    // ========================================================================
    // Sort
    // ========================================================================

    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const Candle& left,
           const Candle& right)
        {
            return
                left.timestamp <
                right.timestamp;
        }
    );


    // ========================================================================
    // Remove duplicate timestamps
    // ========================================================================

    ordered.erase(
        std::unique(
            ordered.begin(),
            ordered.end(),
            [](const Candle& left,
               const Candle& right)
            {
                return
                    left.timestamp ==
                    right.timestamp;
            }
        ),
        ordered.end()
    );


    std::vector<MissingRange> gaps;


    if (ordered.size() < 2)
    {
        return gaps;
    }


    // ========================================================================
    // Detect gaps
    // ========================================================================

    for (
        std::size_t index = 1;
        index < ordered.size();
        ++index
    )
    {
        const Candle& previous =
            ordered[index - 1];

        const Candle& current =
            ordered[index];


        // Never bridge different trading dates.

        if (
            !sameTradingDate(
                previous.timestamp,
                current.timestamp)
        )
        {
            continue;
        }


        const std::int64_t difference =
            current.timestamp -
            previous.timestamp;


        if (
            difference <=
            ONE_MINUTE_SECONDS
        )
        {
            continue;
        }


        if (
            difference %
            ONE_MINUTE_SECONDS != 0
        )
        {
            continue;
        }


        const std::size_t missing =
            static_cast<std::size_t>(
                difference /
                ONE_MINUTE_SECONDS -
                1
            );


        if (missing == 0)
        {
            continue;
        }


        MissingRange gap;

        gap.symbol =
            symbol;

        gap.first_missing_timestamp =
            previous.timestamp +
            ONE_MINUTE_SECONDS;

        gap.last_missing_timestamp =
            current.timestamp -
            ONE_MINUTE_SECONDS;

        gap.missing_minutes =
            missing;


        gaps.push_back(
            std::move(gap)
        );
    }


    return gaps;
}


// ============================================================================
// Repair
//
// Existing data wins.
//
// Downloaded candles are inserted ONLY when their timestamp is currently
// missing.
//
// We never overwrite an existing market candle automatically.
// ============================================================================

RepairResult MarketDataRepairEngine::repair(
    const std::string& symbol,
    const std::vector<Candle>& existing,
    const std::vector<Candle>& downloaded,
    std::vector<Candle>& repaired) const
{
    RepairResult result;

    result.symbol =
        symbol;

    result.original_candles =
        existing.size();

    result.replacement_candles_received =
        downloaded.size();


    // ========================================================================
    // Find original gaps
    // ========================================================================

    result.gaps =
        findInternalGaps(
            symbol,
            existing
        );


    result.gaps_found =
        result.gaps.size();


    for (const MissingRange& gap :
         result.gaps)
    {
        result.missing_candles_requested +=
            gap.missing_minutes;
    }


    // ========================================================================
    // Create exact set of timestamps that are allowed to be repaired.
    //
    // This is critical:
    //
    // A provider response may contain extra candles around the requested
    // interval. Those extra candles must NOT silently modify our database.
    // ========================================================================

    std::set<std::int64_t>
        required_timestamps;


    for (const MissingRange& gap :
         result.gaps)
    {
        for (
            std::int64_t timestamp =
                gap.first_missing_timestamp;

            timestamp <=
                gap.last_missing_timestamp;

            timestamp +=
                ONE_MINUTE_SECONDS
        )
        {
            required_timestamps.insert(
                timestamp
            );
        }
    }


    // ========================================================================
    // Start with existing candles
    // ========================================================================

    repaired =
        existing;


    std::set<CandleKey> existing_keys;


    for (const Candle& candle :
         repaired)
    {
        existing_keys.insert(
            CandleKey{
                candle.symbol,
                candle.timestamp
            }
        );
    }


    // ========================================================================
    // Add downloaded replacements
    // ========================================================================

    for (const Candle& candle :
         downloaded)
    {
        if (
            !validReplacement(
                candle,
                symbol)
        )
        {
            ++result.replacement_candles_rejected;

            continue;
        }


        // Only accept timestamps that were actually missing.

        if (
            required_timestamps.find(
                candle.timestamp
            ) ==
            required_timestamps.end()
        )
        {
            ++result.replacement_candles_rejected;

            continue;
        }


        const CandleKey key{
            candle.symbol,
            candle.timestamp
        };


        // Existing candle always wins.

        if (
            existing_keys.find(key) !=
            existing_keys.end()
        )
        {
            ++result.replacement_candles_rejected;

            continue;
        }


        repaired.push_back(
            candle
        );


        existing_keys.insert(
            key
        );


        ++result.replacement_candles_accepted;
    }


    // ========================================================================
    // Sort repaired data
    // ========================================================================

    std::sort(
        repaired.begin(),
        repaired.end(),
        [](const Candle& left,
           const Candle& right)
        {
            if (left.symbol != right.symbol)
            {
                return
                    left.symbol <
                    right.symbol;
            }

            return
                left.timestamp <
                right.timestamp;
        }
    );


    // ========================================================================
    // Defensive duplicate removal
    // ========================================================================

    const std::size_t before_unique =
        repaired.size();


    repaired.erase(
        std::unique(
            repaired.begin(),
            repaired.end(),
            [](const Candle& left,
               const Candle& right)
            {
                return
                    left.symbol ==
                        right.symbol &&
                    left.timestamp ==
                        right.timestamp;
            }
        ),
        repaired.end()
    );


    result.duplicates_removed =
        before_unique -
        repaired.size();


    result.final_candles =
        repaired.size();


    result.changed =
        result.replacement_candles_accepted > 0;


    // ========================================================================
    // Verify gaps after repair
    // ========================================================================

    const auto remaining_gaps =
        findInternalGaps(
            symbol,
            repaired
        );


    result.success =
        remaining_gaps.empty();


    return result;
}


// ============================================================================
// Atomic Writer
//
// Writes:
//
//     SYMBOL_1min.txt.tmp
//
// first.
//
// Only after the entire file is written successfully do we rename it over the
// destination.
//
// This prevents a partially written market-data file if the program fails
// during output.
// ============================================================================

void MarketDataRepairEngine::writeAtomic(
    const std::filesystem::path& destination,
    const std::vector<Candle>& candles) const
{
    if (destination.empty())
    {
        throw std::invalid_argument(
            "Destination path is empty."
        );
    }


    const std::filesystem::path temporary =
        destination.string() +
        ".tmp";


    // ========================================================================
    // Write temporary file
    // ========================================================================

    {
        std::ofstream output(
            temporary,
            std::ios::trunc
        );


        if (!output.is_open())
        {
            throw std::runtime_error(
                "Unable to open temporary file: " +
                temporary.string()
            );
        }


        output
            << "Symbol,Date,Time,Open,High,Low,Close,Volume\n";


        output
            << std::fixed
            << std::setprecision(2);


        for (const Candle& candle :
             candles)
        {
            output
                << candle.symbol
                << ","
                << formatDate(
                    candle.timestamp)
                << ","
                << formatTime(
                    candle.timestamp)
                << ","
                << candle.open
                << ","
                << candle.high
                << ","
                << candle.low
                << ","
                << candle.close
                << ","
                << candle.volume
                << '\n';
        }


        output.flush();


        if (!output.good())
        {
            throw std::runtime_error(
                "Failed while writing temporary file: " +
                temporary.string()
            );
        }
    }


    // ========================================================================
    // Replace destination
    //
    // On Linux, rename within the same filesystem is atomic.
    // ========================================================================

    std::error_code error;


    std::filesystem::rename(
        temporary,
        destination,
        error
    );


    if (error)
    {
        // Some environments may reject replacement if destination exists.
        // Preserve the original until we know the temporary file is valid.

        const std::filesystem::path backup =
            destination.string() +
            ".bak";


        std::error_code backup_error;


        std::filesystem::remove(
            backup,
            backup_error
        );


        if (
            std::filesystem::exists(
                destination)
        )
        {
            std::filesystem::rename(
                destination,
                backup,
                backup_error
            );


            if (backup_error)
            {
                std::filesystem::remove(
                    temporary
                );

                throw std::runtime_error(
                    "Unable to create backup before replacement: " +
                    backup_error.message()
                );
            }
        }


        std::error_code replacement_error;


        std::filesystem::rename(
            temporary,
            destination,
            replacement_error
        );


        if (replacement_error)
        {
            // Attempt restoration.

            if (
                std::filesystem::exists(
                    backup)
            )
            {
                std::error_code restore_error;

                std::filesystem::rename(
                    backup,
                    destination,
                    restore_error
                );
            }


            throw std::runtime_error(
                "Unable to replace market-data file: " +
                replacement_error.message()
            );
        }


        std::filesystem::remove(
            backup,
            backup_error
        );
    }
}

}