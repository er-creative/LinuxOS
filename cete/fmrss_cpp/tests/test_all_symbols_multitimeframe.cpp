#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace devai::market;


// =========================================================
// Configuration
// =========================================================

const std::filesystem::path SYMBOLS_FILE =
    "config/shares.txt";

const std::filesystem::path DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";


// =========================================================
// Symbol Test Result
// =========================================================

struct SymbolTestResult
{
    std::string symbol;

    std::size_t one_minute_count{0};
    std::size_t five_minute_count{0};
    std::size_t fifteen_minute_count{0};

    std::size_t invalid_rows{0};
    std::size_t duplicate_rows{0};
    std::size_t skipped_rows{0};

    std::size_t incomplete_5m{0};
    std::size_t incomplete_15m{0};

    std::size_t lookahead_1m{0};
    std::size_t lookahead_5m{0};
    std::size_t lookahead_15m{0};

    bool symbol_matches{true};
    bool file_found{true};
    bool passed{false};

    std::string error_message;

    [[nodiscard]]
    std::size_t totalLookAheadViolations() const noexcept
    {
        return
            lookahead_1m +
            lookahead_5m +
            lookahead_15m;
    }
};


// =========================================================
// Trim
// =========================================================

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

    return std::string(
        first,
        last
    );
}


// =========================================================
// Load Symbols
// =========================================================

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

        // Allow comments in shares.txt.
        if (line.front() == '#')
        {
            continue;
        }

        symbols.push_back(
            line
        );
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


// =========================================================
// Build Data Filename
// =========================================================

std::filesystem::path dataFileForSymbol(
    const std::string& symbol)
{
    return
        DATA_FOLDER /
        (symbol + "_1min.txt");
}


// =========================================================
// Verify Symbol Content
// =========================================================

bool allCandlesMatchSymbol(
    const std::vector<Candle>& candles,
    const std::string& expected_symbol)
{
    return std::all_of(
        candles.begin(),
        candles.end(),
        [&expected_symbol](const Candle& candle)
        {
            return
                candle.symbol ==
                expected_symbol;
        }
    );
}


// =========================================================
// Look-Ahead Verification
// =========================================================

bool hasLookAhead(
    const std::optional<TimedCandle>& timed,
    std::int64_t decision_time)
{
    if (!timed.has_value())
    {
        return false;
    }

    if (timed->completed_at > decision_time)
    {
        return true;
    }

    if (timed->received_at > decision_time)
    {
        return true;
    }

    return false;
}


// =========================================================
// Verify Entire Decision Stream
//
// Optimized O(N) implementation.
//
// IMPORTANT:
// The TimedCandle vectors returned by prepareOneMinute(),
// prepareFiveMinute(), and prepareFifteenMinute() are ordered
// by received_at.
//
// Since historical decision time also moves forward, we can
// use forward-only cursors instead of calling stateAt() for
// every 1-minute candle.
//
// This prevents the O(N^2) slowdown seen with large files.
// =========================================================

void verifyDecisionStream(
    const std::vector<TimedCandle>& timed_1m,
    const std::vector<TimedCandle>& timed_5m,
    const std::vector<TimedCandle>& timed_15m,
    const std::vector<Candle>& one_minute,
    SymbolTestResult& result)
{
    // -----------------------------------------------------
    // Forward-only indices for each timeframe
    // -----------------------------------------------------

    std::size_t index_1m = 0;
    std::size_t index_5m = 0;
    std::size_t index_15m = 0;


    // -----------------------------------------------------
    // Latest legally available candle for each timeframe
    // -----------------------------------------------------

    std::optional<TimedCandle> latest_1m;
    std::optional<TimedCandle> latest_5m;
    std::optional<TimedCandle> latest_15m;


    // -----------------------------------------------------
    // Walk through every real 1-minute candle.
    //
    // Example:
    //
    // Candle timestamp = 09:15
    // Candle covers     = 09:15 -> 09:16
    // Decision time     = 09:16
    //
    // Therefore the 09:15 candle becomes usable only when
    // decision_time reaches 09:16.
    // -----------------------------------------------------

    for (const Candle& candle : one_minute)
    {
        const std::int64_t decision_time =
            candle.timestamp + 60;


        // =================================================
        // 1-MINUTE STREAM
        // =================================================

        while (
            index_1m < timed_1m.size() &&
            timed_1m[index_1m].received_at <= decision_time
        )
        {
            const TimedCandle& candidate =
                timed_1m[index_1m];


            // Candle must also be fully completed.
            if (candidate.completed_at <= decision_time)
            {
                // Keep the candle with the newest market
                // timestamp.
                //
                // This also protects us if an older candle
                // happens to arrive late in future live
                // implementations.

                if (
                    !latest_1m.has_value() ||
                    candidate.candle.timestamp >
                        latest_1m->candle.timestamp
                )
                {
                    latest_1m =
                        candidate;
                }
            }


            ++index_1m;
        }


        // =================================================
        // 5-MINUTE STREAM
        // =================================================

        while (
            index_5m < timed_5m.size() &&
            timed_5m[index_5m].received_at <= decision_time
        )
        {
            const TimedCandle& candidate =
                timed_5m[index_5m];


            if (candidate.completed_at <= decision_time)
            {
                if (
                    !latest_5m.has_value() ||
                    candidate.candle.timestamp >
                        latest_5m->candle.timestamp
                )
                {
                    latest_5m =
                        candidate;
                }
            }


            ++index_5m;
        }


        // =================================================
        // 15-MINUTE STREAM
        // =================================================

        while (
            index_15m < timed_15m.size() &&
            timed_15m[index_15m].received_at <= decision_time
        )
        {
            const TimedCandle& candidate =
                timed_15m[index_15m];


            if (candidate.completed_at <= decision_time)
            {
                if (
                    !latest_15m.has_value() ||
                    candidate.candle.timestamp >
                        latest_15m->candle.timestamp
                )
                {
                    latest_15m =
                        candidate;
                }
            }


            ++index_15m;
        }


        // =================================================
        // LOOK-AHEAD VALIDATION
        //
        // At this decision time, none of the selected
        // candles may:
        //
        // 1. complete in the future
        // 2. arrive in the future
        //
        // If either condition occurs, the strategy would
        // have access to information that was not yet
        // available at decision_time.
        // =================================================

        if (
            hasLookAhead(
                latest_1m,
                decision_time
            )
        )
        {
            ++result.lookahead_1m;
        }


        if (
            hasLookAhead(
                latest_5m,
                decision_time
            )
        )
        {
            ++result.lookahead_5m;
        }


        if (
            hasLookAhead(
                latest_15m,
                decision_time
            )
        )
        {
            ++result.lookahead_15m;
        }
    }
}


// =========================================================
// Test One Symbol
// =========================================================

SymbolTestResult testSymbol(
    const std::string& symbol)
{
    SymbolTestResult result;

    result.symbol =
        symbol;

    try
    {
        const std::filesystem::path file_path =
            dataFileForSymbol(
                symbol
            );

        if (!std::filesystem::exists(file_path))
        {
            result.file_found = false;

            result.error_message =
                "Data file not found: " +
                file_path.string();

            return result;
        }


        // =================================================
        // Load 1-Minute Data
        // =================================================

        MarketDataLoader loader(
            DATA_FOLDER
        );

        const auto load_result =
            loader.loadFile(
                file_path.filename().string()
            );

        const std::vector<Candle>& one_minute =
            load_result.candles;


        result.one_minute_count =
            one_minute.size();

        result.invalid_rows =
            load_result.invalid_rows;

        result.duplicate_rows =
            load_result.duplicate_rows;

        result.skipped_rows =
            load_result.skipped_rows;


        if (one_minute.empty())
        {
            result.error_message =
                "No valid 1-minute candles loaded.";

            return result;
        }


        // =================================================
        // Validate Symbol
        // =================================================

        result.symbol_matches =
            allCandlesMatchSymbol(
                one_minute,
                symbol
            );


        if (!result.symbol_matches)
        {
            result.error_message =
                "Loaded candle symbol does not match "
                "shares.txt symbol.";

            return result;
        }


        // =================================================
        // Aggregate 1m -> 5m / 15m
        // =================================================

        CandleAggregator aggregator;


        const auto five_result =
            aggregator.aggregate(
                one_minute,
                Timeframe::FIVE_MINUTES
            );


        const auto fifteen_result =
            aggregator.aggregate(
                one_minute,
                Timeframe::FIFTEEN_MINUTES
            );


        result.five_minute_count =
            five_result.candles.size();

        result.fifteen_minute_count =
            fifteen_result.candles.size();

        result.incomplete_5m =
            five_result.incomplete_buckets;

        result.incomplete_15m =
            fifteen_result.incomplete_buckets;


        if (five_result.candles.empty())
        {
            result.error_message =
                "No complete 5-minute candles generated.";

            return result;
        }


        if (fifteen_result.candles.empty())
        {
            result.error_message =
                "No complete 15-minute candles generated.";

            return result;
        }


        // =================================================
        // Prepare Synchronizer
        // =================================================

        MultiTimeframeSynchronizerConfig config;

        config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer synchronizer(
            config
        );


        const auto timed_1m =
            synchronizer.prepareOneMinute(
                one_minute
            );


        const auto timed_5m =
            synchronizer.prepareFiveMinute(
                five_result.candles
            );


        const auto timed_15m =
            synchronizer.prepareFifteenMinute(
                fifteen_result.candles
            );


        // =================================================
        // Verify Entire Historical Stream
        // =================================================

        verifyDecisionStream(
            timed_1m,
            timed_5m,
            timed_15m,
            one_minute,
            result
        );


        // =================================================
        // Determine Pass / Fail
        //
        // IMPORTANT:
        //
        // Incomplete buckets are reported, but they are NOT
        // automatically considered a failure.
        //
        // The aggregator's job is precisely to reject them.
        //
        // We will inspect their cause separately.
        // =================================================

        result.passed =
            result.file_found &&
            result.symbol_matches &&
            result.one_minute_count > 0 &&
            result.five_minute_count > 0 &&
            result.fifteen_minute_count > 0 &&
            result.invalid_rows == 0 &&
            result.duplicate_rows == 0 &&
            result.totalLookAheadViolations() == 0;


        if (!result.passed &&
            result.error_message.empty())
        {
            result.error_message =
                "One or more validation checks failed.";
        }
    }
    catch (const std::exception& error)
    {
        result.passed = false;

        result.error_message =
            error.what();
    }


    return result;
}


// =========================================================
// Print Result Row
// =========================================================

void printResult(
    const SymbolTestResult& result)
{
    const std::string incomplete =
        std::to_string(
            result.incomplete_5m
        ) +
        "/" +
        std::to_string(
            result.incomplete_15m
        );


    std::cout
        << std::left
        << std::setw(15)
        << result.symbol

        << std::right
        << std::setw(12)
        << result.one_minute_count

        << std::setw(12)
        << result.five_minute_count

        << std::setw(12)
        << result.fifteen_minute_count

        << std::setw(14)
        << incomplete

        << std::setw(14)
        << result.totalLookAheadViolations()

        << std::setw(12)
        << (result.passed ? "PASS" : "FAIL")

        << '\n';
}

} // anonymous namespace


// =========================================================
// Main
// =========================================================

int main()
{
    try
    {
        // =================================================
        // Load Universe
        // =================================================

        const std::vector<std::string> symbols =
            loadSymbols(
                SYMBOLS_FILE
            );


        std::cout
            << "\n"
            << "===========================================================================================\n"
            << "ALL-SYMBOL MULTI-TIMEFRAME VALIDATION\n"
            << "===========================================================================================\n"
            << "Symbols file : "
            << SYMBOLS_FILE
            << '\n'
            << "Data folder  : "
            << DATA_FOLDER
            << '\n'
            << "Symbols      : "
            << symbols.size()
            << '\n'
            << "===========================================================================================\n\n";


        std::cout
            << std::left
            << std::setw(15)
            << "Symbol"

            << std::right
            << std::setw(12)
            << "1m"

            << std::setw(12)
            << "5m"

            << std::setw(12)
            << "15m"

            << std::setw(14)
            << "Inc 5m/15m"

            << std::setw(14)
            << "LookAhead"

            << std::setw(12)
            << "Result"

            << '\n';


        std::cout
            << std::string(
                91,
                '-'
            )
            << '\n';


        // =================================================
        // Test Every Symbol
        // =================================================

        std::vector<SymbolTestResult> results;

        results.reserve(
            symbols.size()
        );


        for (const std::string& symbol : symbols)
        {
            std::cout.flush();

            SymbolTestResult result =
                testSymbol(
                    symbol
                );

            printResult(
                result
            );

            results.push_back(
                std::move(result)
            );
        }


        // =================================================
        // Summary
        // =================================================

        std::size_t passed = 0;
        std::size_t failed = 0;

        std::size_t total_lookahead = 0;

        std::size_t total_incomplete_5m = 0;
        std::size_t total_incomplete_15m = 0;


        for (const SymbolTestResult& result :
             results)
        {
            if (result.passed)
            {
                ++passed;
            }
            else
            {
                ++failed;
            }


            total_lookahead +=
                result.totalLookAheadViolations();


            total_incomplete_5m +=
                result.incomplete_5m;


            total_incomplete_15m +=
                result.incomplete_15m;
        }


        std::cout
            << "\n"
            << "===========================================================================================\n"
            << "SUMMARY\n"
            << "===========================================================================================\n"

            << "Symbols tested            : "
            << results.size()
            << '\n'

            << "Symbols passed            : "
            << passed
            << '\n'

            << "Symbols failed            : "
            << failed
            << '\n'

            << "Look-ahead violations     : "
            << total_lookahead
            << '\n'

            << "Incomplete 5m buckets     : "
            << total_incomplete_5m
            << '\n'

            << "Incomplete 15m buckets    : "
            << total_incomplete_15m
            << '\n';


        // =================================================
        // Print Failures
        // =================================================

        if (failed > 0)
        {
            std::cout
                << "\n"
                << "===========================================================================================\n"
                << "FAILED SYMBOLS\n"
                << "===========================================================================================\n";


            for (const SymbolTestResult& result :
                 results)
            {
                if (result.passed)
                {
                    continue;
                }


                std::cout
                    << result.symbol
                    << " : "
                    << result.error_message
                    << '\n';
            }
        }


        // =================================================
        // Final Result
        // =================================================

        std::cout
            << "\n"
            << "===========================================================================================\n";


        if (failed == 0 &&
            total_lookahead == 0)
        {
            std::cout
                << "ALL-SYMBOL MULTI-TIMEFRAME VALIDATION PASSED\n"
                << "===========================================================================================\n";

            return 0;
        }


        std::cout
            << "ALL-SYMBOL MULTI-TIMEFRAME VALIDATION FAILED\n"
            << "===========================================================================================\n";


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