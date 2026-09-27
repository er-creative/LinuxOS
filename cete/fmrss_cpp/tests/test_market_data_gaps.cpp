#include "devai/market/Candle.hpp"
#include "devai/market/MarketDataLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace devai::market;


// ============================================================================
// Configuration
// ============================================================================

const std::filesystem::path SYMBOLS_FILE =
    "config/shares.txt";

const std::filesystem::path DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";


// Regular NSE cash-market session.
//
// Candle timestamps are candle START times:
//
// First expected candle : 09:15
// Last expected candle  : 15:29
//
// 15:29 candle represents 15:29 -> 15:30.
//
// IMPORTANT:
//
// This validator distinguishes:
//
// 1. INTERNAL GAP
//    Missing candle between two candles that actually exist.
//
// 2. LATE START
//    First candle of session is later than 09:15.
//
// 3. EARLY END
//    Last candle of session is earlier than 15:29.
//
// Therefore a file ending at 15:14 will NOT generate 15 individual
// internal-gap errors. It will generate one EARLY END diagnostic.
// ============================================================================

constexpr int SESSION_START_HOUR = 9;
constexpr int SESSION_START_MINUTE = 15;

constexpr int SESSION_END_HOUR = 15;
constexpr int SESSION_END_MINUTE = 30;

constexpr std::int64_t ONE_MINUTE_SECONDS = 60;

constexpr int EXPECTED_CANDLES_PER_FULL_SESSION = 375;


// ============================================================================
// Date Key
// ============================================================================

struct DateKey
{
    int year{0};
    int month{0};
    int day{0};

    [[nodiscard]]
    bool operator<(const DateKey& other) const noexcept
    {
        if (year != other.year)
        {
            return year < other.year;
        }

        if (month != other.month)
        {
            return month < other.month;
        }

        return day < other.day;
    }

    [[nodiscard]]
    bool operator==(const DateKey& other) const noexcept
    {
        return
            year == other.year &&
            month == other.month &&
            day == other.day;
    }
};


// ============================================================================
// Internal Gap
// ============================================================================

struct InternalGap
{
    DateKey date;

    std::int64_t previous_timestamp{0};
    std::int64_t next_timestamp{0};

    std::int64_t first_missing_timestamp{0};
    std::int64_t last_missing_timestamp{0};

    std::size_t missing_minutes{0};
};


// ============================================================================
// Session Result
// ============================================================================

struct SessionResult
{
    DateKey date;

    std::size_t candle_count{0};

    std::int64_t first_timestamp{0};
    std::int64_t last_timestamp{0};

    bool late_start{false};
    bool early_end{false};

    std::size_t late_start_minutes{0};
    std::size_t early_end_minutes{0};

    std::size_t internal_gap_count{0};
    std::size_t internal_missing_minutes{0};

    std::size_t invalid_ohlc{0};
    std::size_t duplicate_timestamps{0};
    std::size_t out_of_session{0};

    std::vector<InternalGap> internal_gaps;

    [[nodiscard]]
    bool structurallyClean() const noexcept
    {
        return
            internal_gap_count == 0 &&
            invalid_ohlc == 0 &&
            duplicate_timestamps == 0 &&
            out_of_session == 0;
    }

    [[nodiscard]]
    bool fullRegularSession() const noexcept
    {
        return
            structurallyClean() &&
            !late_start &&
            !early_end &&
            candle_count ==
                EXPECTED_CANDLES_PER_FULL_SESSION;
    }
};


// ============================================================================
// Symbol Result
// ============================================================================

struct SymbolResult
{
    std::string symbol;

    bool file_found{true};
    bool timestamps_sorted{true};
    bool passed{false};

    std::size_t total_candles{0};

    std::size_t sessions{0};
    std::size_t full_sessions{0};
    std::size_t partial_sessions{0};

    std::size_t internal_gap_count{0};
    std::size_t internal_missing_minutes{0};

    std::size_t late_start_sessions{0};
    std::size_t early_end_sessions{0};

    std::size_t invalid_ohlc{0};
    std::size_t duplicate_timestamps{0};
    std::size_t out_of_session{0};

    std::size_t loader_invalid_rows{0};
    std::size_t loader_duplicate_rows{0};
    std::size_t loader_skipped_rows{0};

    std::vector<SessionResult> session_results;

    std::string error_message;
};


// ============================================================================
// Trim
// ============================================================================

std::string trim(std::string value)
{
    const auto first =
        std::find_if_not(
            value.begin(),
            value.end(),
            [](unsigned char character)
            {
                return std::isspace(character);
            }
        );

    const auto last =
        std::find_if_not(
            value.rbegin(),
            value.rend(),
            [](unsigned char character)
            {
                return std::isspace(character);
            }
        ).base();

    if (first >= last)
    {
        return {};
    }

    return std::string(first, last);
}


// ============================================================================
// Load Symbols
// ============================================================================

std::vector<std::string> loadSymbols(
    const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error(
            "Symbols file not found: " +
            path.string()
        );
    }

    std::ifstream input(path);

    if (!input.is_open())
    {
        throw std::runtime_error(
            "Unable to open symbols file: " +
            path.string()
        );
    }

    std::vector<std::string> symbols;

    std::string line;

    while (std::getline(input, line))
    {
        line = trim(line);

        if (line.empty())
        {
            continue;
        }

        if (line.front() == '#')
        {
            continue;
        }

        symbols.push_back(line);
    }

    if (symbols.empty())
    {
        throw std::runtime_error(
            "No symbols found in: " +
            path.string()
        );
    }

    return symbols;
}


// ============================================================================
// Timestamp Helpers
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


DateKey dateFromTimestamp(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(timestamp);

    return DateKey{
        value.tm_year + 1900,
        value.tm_mon + 1,
        value.tm_mday
    };
}


std::string formatDate(
    const DateKey& date)
{
    std::ostringstream output;

    output
        << std::setfill('0')
        << std::setw(4)
        << date.year
        << "-"
        << std::setw(2)
        << date.month
        << "-"
        << std::setw(2)
        << date.day;

    return output.str();
}


std::string formatTimestamp(
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
        << value.tm_mday
        << " "
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


std::string formatTimeOnly(
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
        << value.tm_min;

    return output.str();
}


std::int64_t makeTimestamp(
    const DateKey& date,
    int hour,
    int minute)
{
    std::tm value{};

    value.tm_year =
        date.year - 1900;

    value.tm_mon =
        date.month - 1;

    value.tm_mday =
        date.day;

    value.tm_hour =
        hour;

    value.tm_min =
        minute;

    value.tm_sec =
        0;

    value.tm_isdst =
        -1;

    const std::time_t result =
        std::mktime(&value);

    if (result == static_cast<std::time_t>(-1))
    {
        throw std::runtime_error(
            "Unable to create timestamp for " +
            formatDate(date)
        );
    }

    return static_cast<std::int64_t>(
        result
    );
}


std::int64_t sessionStart(
    const DateKey& date)
{
    return makeTimestamp(
        date,
        SESSION_START_HOUR,
        SESSION_START_MINUTE
    );
}


std::int64_t sessionEnd(
    const DateKey& date)
{
    return makeTimestamp(
        date,
        SESSION_END_HOUR,
        SESSION_END_MINUTE
    );
}


std::int64_t lastExpectedCandleStart(
    const DateKey& date)
{
    return
        sessionEnd(date) -
        ONE_MINUTE_SECONDS;
}


// ============================================================================
// Session Boundary
// ============================================================================

bool isInsideRegularSession(
    std::int64_t timestamp)
{
    const DateKey date =
        dateFromTimestamp(
            timestamp
        );

    return
        timestamp >= sessionStart(date) &&
        timestamp < sessionEnd(date);
}


// ============================================================================
// OHLC Validation
// ============================================================================

bool validOHLC(
    const Candle& candle)
{
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
        candle.high < candle.close
    )
    {
        return false;
    }


    if (
        candle.low > candle.open ||
        candle.low > candle.close
    )
    {
        return false;
    }


    if (candle.high < candle.low)
    {
        return false;
    }


    return true;
}


// ============================================================================
// Group Candles By Date
// ============================================================================

std::map<DateKey, std::vector<const Candle*>>
groupByDate(
    const std::vector<Candle>& candles)
{
    std::map<
        DateKey,
        std::vector<const Candle*>
    > result;


    for (const Candle& candle :
         candles)
    {
        result[
            dateFromTimestamp(
                candle.timestamp
            )
        ].push_back(
            &candle
        );
    }


    return result;
}


// ============================================================================
// Analyze One Session
// ============================================================================

SessionResult analyzeSession(
    const DateKey& date,
    const std::vector<const Candle*>& candles)
{
    SessionResult result;

    result.date =
        date;


    if (candles.empty())
    {
        return result;
    }


    // ========================================================================
    // Collect valid in-session candles
    // ========================================================================

    std::vector<const Candle*> in_session;

    in_session.reserve(
        candles.size()
    );


    std::set<std::int64_t> unique_timestamps;


    for (const Candle* candle :
         candles)
    {
        if (candle == nullptr)
        {
            continue;
        }


        // --------------------------------------------------------------------
        // OHLC validation
        // --------------------------------------------------------------------

        if (!validOHLC(*candle))
        {
            ++result.invalid_ohlc;
        }


        // --------------------------------------------------------------------
        // Session boundary validation
        // --------------------------------------------------------------------

        if (!isInsideRegularSession(
                candle->timestamp))
        {
            ++result.out_of_session;

            continue;
        }


        // --------------------------------------------------------------------
        // Duplicate timestamp validation
        // --------------------------------------------------------------------

        const auto insertion =
            unique_timestamps.insert(
                candle->timestamp
            );


        if (!insertion.second)
        {
            ++result.duplicate_timestamps;

            continue;
        }


        in_session.push_back(
            candle
        );
    }


    // ========================================================================
    // Sort session candles
    // ========================================================================

    std::sort(
        in_session.begin(),
        in_session.end(),
        [](const Candle* left,
           const Candle* right)
        {
            return
                left->timestamp <
                right->timestamp;
        }
    );


    result.candle_count =
        in_session.size();


    if (in_session.empty())
    {
        return result;
    }


    result.first_timestamp =
        in_session.front()->timestamp;

    result.last_timestamp =
        in_session.back()->timestamp;


    // ========================================================================
    // Late Start
    // ========================================================================

    const std::int64_t expected_start =
        sessionStart(date);


    if (
        result.first_timestamp >
        expected_start
    )
    {
        result.late_start =
            true;

        result.late_start_minutes =
            static_cast<std::size_t>(
                (
                    result.first_timestamp -
                    expected_start
                ) /
                ONE_MINUTE_SECONDS
            );
    }


    // ========================================================================
    // Early End
    //
    // Last legal candle start should be 15:29.
    //
    // Example:
    //
    // Last candle = 15:14
    //
    // Missing tail:
    //
    // 15:15 ... 15:29 = 15 minutes
    //
    // This is NOT classified as an internal gap.
    // ========================================================================

    const std::int64_t expected_last =
        lastExpectedCandleStart(date);


    if (
        result.last_timestamp <
        expected_last
    )
    {
        result.early_end =
            true;

        result.early_end_minutes =
            static_cast<std::size_t>(
                (
                    expected_last -
                    result.last_timestamp
                ) /
                ONE_MINUTE_SECONDS
            );
    }


    // ========================================================================
    // Internal Gap Detection
    //
    // Only inspect timestamps BETWEEN two real candles.
    //
    // Example:
    //
    // 10:20
    // 10:21
    // 10:24
    //
    // Difference = 180 seconds.
    //
    // Missing:
    //
    // 10:22
    // 10:23
    //
    // Internal gap count   = 1
    // Missing minutes      = 2
    //
    // Session boundaries are deliberately NOT handled here.
    // ========================================================================

    for (
        std::size_t index = 1;
        index < in_session.size();
        ++index
    )
    {
        const std::int64_t previous =
            in_session[index - 1]->timestamp;

        const std::int64_t current =
            in_session[index]->timestamp;


        const std::int64_t difference =
            current - previous;


        if (
            difference <=
            ONE_MINUTE_SECONDS
        )
        {
            continue;
        }


        // --------------------------------------------------------------------
        // Only whole-minute discontinuities count as minute-data gaps.
        // --------------------------------------------------------------------

        if (
            difference %
            ONE_MINUTE_SECONDS != 0
        )
        {
            continue;
        }


        const std::size_t missing_minutes =
            static_cast<std::size_t>(
                difference /
                ONE_MINUTE_SECONDS -
                1
            );


        if (missing_minutes == 0)
        {
            continue;
        }


        InternalGap gap;

        gap.date =
            date;

        gap.previous_timestamp =
            previous;

        gap.next_timestamp =
            current;

        gap.first_missing_timestamp =
            previous +
            ONE_MINUTE_SECONDS;

        gap.last_missing_timestamp =
            current -
            ONE_MINUTE_SECONDS;

        gap.missing_minutes =
            missing_minutes;


        result.internal_gaps.push_back(
            gap
        );


        ++result.internal_gap_count;

        result.internal_missing_minutes +=
            missing_minutes;
    }


    return result;
}


// ============================================================================
// Analyze One Symbol
// ============================================================================

SymbolResult analyzeSymbol(
    const std::string& symbol)
{
    SymbolResult result;

    result.symbol =
        symbol;


    try
    {
        const std::filesystem::path file_path =
            DATA_FOLDER /
            (symbol + "_1min.txt");


        if (!std::filesystem::exists(
                file_path))
        {
            result.file_found =
                false;

            result.error_message =
                "Data file not found: " +
                file_path.string();

            return result;
        }


        // ====================================================================
        // Load through existing MarketDataLoader
        // ====================================================================

        MarketDataLoader loader(
            DATA_FOLDER
        );


        const auto load_result =
            loader.loadFile(
                file_path.filename().string()
            );


        const std::vector<Candle>& candles =
            load_result.candles;


        result.total_candles =
            candles.size();

        result.loader_invalid_rows =
            load_result.invalid_rows;

        result.loader_duplicate_rows =
            load_result.duplicate_rows;

        result.loader_skipped_rows =
            load_result.skipped_rows;


        if (candles.empty())
        {
            result.error_message =
                "No valid candles loaded.";

            return result;
        }


        // ====================================================================
        // Timestamp Ordering
        // ====================================================================

        for (
            std::size_t index = 1;
            index < candles.size();
            ++index
        )
        {
            if (
                candles[index].timestamp <
                candles[index - 1].timestamp
            )
            {
                result.timestamps_sorted =
                    false;

                break;
            }
        }


        // ====================================================================
        // Group by actual dates present in file
        //
        // No calendar dates are synthesized here.
        //
        // Therefore weekends/full holidays are not automatically considered
        // missing.
        // ====================================================================

        const auto sessions =
            groupByDate(
                candles
            );


        result.sessions =
            sessions.size();


        // ====================================================================
        // Analyze Sessions
        // ====================================================================

        for (const auto& entry :
             sessions)
        {
            const DateKey& date =
                entry.first;

            const std::vector<const Candle*>&
                session_candles =
                    entry.second;


            SessionResult session =
                analyzeSession(
                    date,
                    session_candles
                );


            result.internal_gap_count +=
                session.internal_gap_count;

            result.internal_missing_minutes +=
                session.internal_missing_minutes;

            result.invalid_ohlc +=
                session.invalid_ohlc;

            result.duplicate_timestamps +=
                session.duplicate_timestamps;

            result.out_of_session +=
                session.out_of_session;


            if (session.late_start)
            {
                ++result.late_start_sessions;
            }


            if (session.early_end)
            {
                ++result.early_end_sessions;
            }


            if (session.fullRegularSession())
            {
                ++result.full_sessions;
            }
            else
            {
                ++result.partial_sessions;
            }


            result.session_results.push_back(
                std::move(session)
            );
        }


        // ====================================================================
        // PASS / CHECK policy
        //
        // IMPORTANT:
        //
        // Late-start and early-end sessions are reported as PARTIAL sessions,
        // but they do NOT automatically fail structural data quality.
        //
        // Why?
        //
        // Because your current historical files contain many sessions ending
        // at 15:14. We need to distinguish that source-data coverage issue
        // from corruption inside the available data.
        //
        // Internal gaps DO cause CHECK because they can invalidate 5m/15m
        // aggregation inside the available interval.
        // ====================================================================

        result.passed =
            result.file_found &&
            result.timestamps_sorted &&
            result.loader_invalid_rows == 0 &&
            result.loader_duplicate_rows == 0 &&
            result.internal_gap_count == 0 &&
            result.invalid_ohlc == 0 &&
            result.duplicate_timestamps == 0 &&
            result.out_of_session == 0;


        if (
            !result.passed &&
            result.error_message.empty()
        )
        {
            result.error_message =
                "Structural data-quality issue detected.";
        }
    }
    catch (const std::exception& error)
    {
        result.passed =
            false;

        result.error_message =
            error.what();
    }


    return result;
}


// ============================================================================
// Print Main Symbol Row
// ============================================================================

void printSymbolRow(
    const SymbolResult& result)
{
    std::cout
        << std::left
        << std::setw(15)
        << result.symbol

        << std::right
        << std::setw(11)
        << result.total_candles

        << std::setw(10)
        << result.sessions

        << std::setw(10)
        << result.internal_gap_count

        << std::setw(12)
        << result.internal_missing_minutes

        << std::setw(11)
        << result.late_start_sessions

        << std::setw(11)
        << result.early_end_sessions

        << std::setw(10)
        << result.invalid_ohlc

        << std::setw(10)
        << (
            result.loader_duplicate_rows +
            result.duplicate_timestamps
        )

        << std::setw(11)
        << (result.passed ? "PASS" : "CHECK")

        << '\n';
}


// ============================================================================
// Print Internal Gaps
// ============================================================================

void printInternalGaps(
    const SymbolResult& result)
{
    if (result.internal_gap_count == 0)
    {
        return;
    }


    std::cout
        << "\n"
        << "================================================================================\n"
        << result.symbol
        << " - INTERNAL GAPS\n"
        << "================================================================================\n";


    for (const SessionResult& session :
         result.session_results)
    {
        if (session.internal_gaps.empty())
        {
            continue;
        }


        std::cout
            << '\n'
            << formatDate(
                session.date
            )
            << '\n';


        for (const InternalGap& gap :
             session.internal_gaps)
        {
            std::cout
                << "  Previous candle : "
                << formatTimestamp(
                    gap.previous_timestamp
                )
                << '\n'

                << "  First missing   : "
                << formatTimestamp(
                    gap.first_missing_timestamp
                )
                << '\n'

                << "  Last missing    : "
                << formatTimestamp(
                    gap.last_missing_timestamp
                )
                << '\n'

                << "  Next candle     : "
                << formatTimestamp(
                    gap.next_timestamp
                )
                << '\n'

                << "  Missing minutes : "
                << gap.missing_minutes
                << '\n'

                << "  ----------------------------------------------------------\n";
        }
    }
}


// ============================================================================
// Print Partial Sessions
// ============================================================================

void printPartialSessions(
    const SymbolResult& result)
{
    bool printed_header =
        false;


    for (const SessionResult& session :
         result.session_results)
    {
        if (
            !session.late_start &&
            !session.early_end
        )
        {
            continue;
        }


        if (!printed_header)
        {
            std::cout
                << "\n"
                << "================================================================================\n"
                << result.symbol
                << " - PARTIAL SESSION BOUNDARIES\n"
                << "================================================================================\n";

            printed_header =
                true;
        }


        std::cout
            << formatDate(
                session.date
            )
            << "  Count="
            << session.candle_count;


        if (session.first_timestamp != 0)
        {
            std::cout
                << "  First="
                << formatTimeOnly(
                    session.first_timestamp
                );
        }


        if (session.last_timestamp != 0)
        {
            std::cout
                << "  Last="
                << formatTimeOnly(
                    session.last_timestamp
                );
        }


        if (session.late_start)
        {
            std::cout
                << "  LateStart="
                << session.late_start_minutes
                << "m";
        }


        if (session.early_end)
        {
            std::cout
                << "  EarlyEnd="
                << session.early_end_minutes
                << "m";
        }


        if (session.internal_missing_minutes > 0)
        {
            std::cout
                << "  InternalMissing="
                << session.internal_missing_minutes
                << "m";
        }


        std::cout
            << '\n';
    }
}


// ============================================================================
// Print Structural Problems
// ============================================================================

void printStructuralProblems(
    const SymbolResult& result)
{
    if (
        result.invalid_ohlc == 0 &&
        result.loader_duplicate_rows == 0 &&
        result.duplicate_timestamps == 0 &&
        result.out_of_session == 0 &&
        result.timestamps_sorted
    )
    {
        return;
    }


    std::cout
        << "\n"
        << "================================================================================\n"
        << result.symbol
        << " - OTHER DATA QUALITY ISSUES\n"
        << "================================================================================\n"

        << "Invalid OHLC            : "
        << result.invalid_ohlc
        << '\n'

        << "Loader duplicate rows   : "
        << result.loader_duplicate_rows
        << '\n'

        << "Session duplicate times : "
        << result.duplicate_timestamps
        << '\n'

        << "Out-of-session candles  : "
        << result.out_of_session
        << '\n'

        << "Timestamps sorted       : "
        << (
            result.timestamps_sorted
                ? "YES"
                : "NO"
        )
        << '\n';
}

} // anonymous namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    try
    {
        const std::vector<std::string> symbols =
            loadSymbols(
                SYMBOLS_FILE
            );


        std::cout
            << "\n"
            << "==============================================================================================================\n"
            << "MARKET DATA GAP / QUALITY VALIDATION - VERSION 2\n"
            << "==============================================================================================================\n"
            << "Symbols file : "
            << SYMBOLS_FILE
            << '\n'
            << "Data folder  : "
            << DATA_FOLDER
            << '\n'
            << "Symbols      : "
            << symbols.size()
            << '\n'
            << "Session      : 09:15 -> 15:30\n"
            << "Full session : "
            << EXPECTED_CANDLES_PER_FULL_SESSION
            << " x 1-minute candles\n"
            << "==============================================================================================================\n\n";


        // ====================================================================
        // Table Header
        // ====================================================================

        std::cout
            << std::left
            << std::setw(15)
            << "Symbol"

            << std::right
            << std::setw(11)
            << "Candles"

            << std::setw(10)
            << "Sessions"

            << std::setw(10)
            << "IntGaps"

            << std::setw(12)
            << "IntMissing"

            << std::setw(11)
            << "LateStart"

            << std::setw(11)
            << "EarlyEnd"

            << std::setw(10)
            << "BadOHLC"

            << std::setw(10)
            << "Duplicate"

            << std::setw(11)
            << "Result"

            << '\n';


        std::cout
            << std::string(
                110,
                '-'
            )
            << '\n';


        // ====================================================================
        // Analyze Symbols
        // ====================================================================

        std::vector<SymbolResult> results;

        results.reserve(
            symbols.size()
        );


        for (const std::string& symbol :
             symbols)
        {
            SymbolResult result =
                analyzeSymbol(
                    symbol
                );


            printSymbolRow(
                result
            );


            results.push_back(
                std::move(result)
            );
        }


        // ====================================================================
        // Totals
        // ====================================================================

        std::size_t passed_symbols = 0;
        std::size_t check_symbols = 0;

        std::size_t total_candles = 0;
        std::size_t total_sessions = 0;

        std::size_t total_full_sessions = 0;
        std::size_t total_partial_sessions = 0;

        std::size_t total_internal_gaps = 0;
        std::size_t total_internal_missing = 0;

        std::size_t total_late_starts = 0;
        std::size_t total_early_ends = 0;

        std::size_t total_bad_ohlc = 0;
        std::size_t total_duplicates = 0;
        std::size_t total_out_of_session = 0;


        for (const SymbolResult& result :
             results)
        {
            if (result.passed)
            {
                ++passed_symbols;
            }
            else
            {
                ++check_symbols;
            }


            total_candles +=
                result.total_candles;

            total_sessions +=
                result.sessions;

            total_full_sessions +=
                result.full_sessions;

            total_partial_sessions +=
                result.partial_sessions;

            total_internal_gaps +=
                result.internal_gap_count;

            total_internal_missing +=
                result.internal_missing_minutes;

            total_late_starts +=
                result.late_start_sessions;

            total_early_ends +=
                result.early_end_sessions;

            total_bad_ohlc +=
                result.invalid_ohlc;

            total_duplicates +=
                result.loader_duplicate_rows +
                result.duplicate_timestamps;

            total_out_of_session +=
                result.out_of_session;
        }


        // ====================================================================
        // Detailed Diagnostics
        // ====================================================================

        for (const SymbolResult& result :
             results)
        {
            printInternalGaps(
                result
            );

            printPartialSessions(
                result
            );

            printStructuralProblems(
                result
            );
        }


        // ====================================================================
        // Summary
        // ====================================================================

        std::cout
            << "\n"
            << "==============================================================================================================\n"
            << "SUMMARY\n"
            << "==============================================================================================================\n"

            << "Symbols tested             : "
            << results.size()
            << '\n'

            << "Symbols structurally clean : "
            << passed_symbols
            << '\n'

            << "Symbols requiring check    : "
            << check_symbols
            << '\n'

            << "Total candles              : "
            << total_candles
            << '\n'

            << "Sessions analyzed          : "
            << total_sessions
            << '\n'

            << "Full regular sessions      : "
            << total_full_sessions
            << '\n'

            << "Partial sessions           : "
            << total_partial_sessions
            << '\n'

            << "Internal gap events        : "
            << total_internal_gaps
            << '\n'

            << "Internal missing minutes   : "
            << total_internal_missing
            << '\n'

            << "Late-start sessions        : "
            << total_late_starts
            << '\n'

            << "Early-end sessions         : "
            << total_early_ends
            << '\n'

            << "Invalid OHLC candles       : "
            << total_bad_ohlc
            << '\n'

            << "Duplicate timestamps       : "
            << total_duplicates
            << '\n'

            << "Out-of-session candles     : "
            << total_out_of_session
            << '\n';


        // ====================================================================
        // Interpretation
        // ====================================================================

        std::cout
            << "\n"
            << "==============================================================================================================\n"
            << "INTERPRETATION\n"
            << "==============================================================================================================\n"

            << "Internal gap = missing minute(s) BETWEEN two existing candles.\n"
            << "Late start   = first available candle is later than 09:15.\n"
            << "Early end    = last available candle is earlier than 15:29.\n"
            << "Partial session boundaries are reported separately and do not automatically fail structural validation.\n"
            << "==============================================================================================================\n";


        // ====================================================================
        // Final Result
        // ====================================================================

        if (check_symbols == 0)
        {
            std::cout
                << "STRUCTURAL MARKET DATA VALIDATION PASSED\n"
                << "==============================================================================================================\n";

            return 0;
        }


        std::cout
            << "STRUCTURAL MARKET DATA VALIDATION FOUND ISSUES\n"
            << "==============================================================================================================\n";

        return 2;
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