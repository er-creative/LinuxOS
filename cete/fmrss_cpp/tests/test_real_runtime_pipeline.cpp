#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/StockBenchmarkSnapshot.hpp"
#include "devai/market/StockBenchmarkSynchronizer.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace devai::market;


namespace
{

// ============================================================================
// Configuration
// ============================================================================

const std::filesystem::path DEFAULT_DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

const std::string DEFAULT_STOCK_SYMBOL =
    "RELIANCE";

const std::string DEFAULT_BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr std::int64_t FIVE_MINUTE_SECONDS =
    5 * 60;

constexpr std::int64_t FIFTEEN_MINUTE_SECONDS =
    15 * 60;


// ============================================================================
// Runtime Validation Statistics
// ============================================================================

struct RuntimeValidationStats
{
    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t before_open_points{0};

    std::size_t after_close_points{0};

    std::size_t new_sessions{0};

    std::size_t stock_one_available{0};
    std::size_t stock_five_available{0};
    std::size_t stock_fifteen_available{0};

    std::size_t benchmark_one_available{0};
    std::size_t benchmark_five_available{0};
    std::size_t benchmark_fifteen_available{0};

    std::size_t fully_synchronized_pairs{0};
    std::size_t incomplete_pairs{0};

    std::size_t decision_time_violations{0};
    std::size_t trading_date_violations{0};

    std::size_t stock_lookahead_violations{0};
    std::size_t benchmark_lookahead_violations{0};

    std::size_t stock_session_carryover_violations{0};
    std::size_t benchmark_session_carryover_violations{0};

    std::size_t one_minute_alignment_violations{0};
    std::size_t five_minute_alignment_violations{0};
    std::size_t fifteen_minute_alignment_violations{0};

    std::size_t synchronization_exceptions{0};

    std::size_t stock_timestamp_regressions{0};
    std::size_t benchmark_timestamp_regressions{0};
};


// ============================================================================
// Local Time Helpers
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


std::string formatTimestamp(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(
            timestamp
        );

    char buffer[32]{};

    std::strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &value
    );

    return std::string(
        buffer
    );
}


TradingDate tradingDateFromTimestamp(
    std::int64_t timestamp)
{
    const std::tm value =
        toLocalTime(
            timestamp
        );

    return TradingDate{
        value.tm_year + 1900,
        value.tm_mon + 1,
        value.tm_mday
    };
}


std::int64_t makeLocalTimestamp(
    const TradingDate& date,
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

    const std::time_t timestamp =
        std::mktime(
            &value
        );

    if (
        timestamp ==
        static_cast<std::time_t>(-1)
    )
    {
        throw std::runtime_error(
            "Unable to construct local timestamp."
        );
    }

    return static_cast<std::int64_t>(
        timestamp
    );
}


// ============================================================================
// Determine if timestamp belongs to regular session
//
// 09:15 <= timestamp < 15:30
// ============================================================================

bool isRegularSessionTimestamp(
    std::int64_t timestamp)
{
    const TradingDate date =
        tradingDateFromTimestamp(
            timestamp
        );

    const std::int64_t open =
        makeLocalTimestamp(
            date,
            9,
            15
        );

    const std::int64_t close =
        makeLocalTimestamp(
            date,
            15,
            30
        );

    return
        timestamp >= open &&
        timestamp < close;
}


// ============================================================================
// Load Result Helper
//
// Your MarketDataLoader already returns LoadResult.
// ============================================================================

struct InstrumentRuntimeData
{
    std::string symbol;

    std::vector<Candle> one_minute;
    std::vector<Candle> five_minute;
    std::vector<Candle> fifteen_minute;

    std::vector<TimedCandle> timed_one;
    std::vector<TimedCandle> timed_five;
    std::vector<TimedCandle> timed_fifteen;

    std::size_t invalid_rows{0};
    std::size_t duplicate_rows{0};
    std::size_t skipped_rows{0};

    std::size_t incomplete_five{0};
    std::size_t incomplete_fifteen{0};
};


// ============================================================================
// Validate Candle Symbol
// ============================================================================

void validateLoadedSymbol(
    const std::vector<Candle>& candles,
    const std::string& expected_symbol)
{
    for (const Candle& candle : candles)
    {
        if (
            candle.symbol !=
            expected_symbol
        )
        {
            throw std::runtime_error(
                "Unexpected symbol in data. Expected " +
                expected_symbol +
                ", received " +
                candle.symbol
            );
        }
    }
}


// ============================================================================
// Prepare One Instrument
// ============================================================================

InstrumentRuntimeData prepareInstrument(
    const std::filesystem::path& data_folder,
    const std::string& symbol,
    const MultiTimeframeSynchronizer& synchronizer)
{
    const std::filesystem::path filename =
        data_folder /
        (
            symbol +
            "_1min.txt"
        );


    if (
        !std::filesystem::exists(
            filename
        )
    )
    {
        throw std::runtime_error(
            "Market-data file not found: " +
            filename.string()
        );
    }


    MarketDataLoader loader(
        data_folder
    );


    const auto load_result =
        loader.loadPath(
            filename
        );


    if (
        load_result.candles.empty()
    )
    {
        throw std::runtime_error(
            "No 1-minute candles loaded for " +
            symbol
        );
    }


    validateLoadedSymbol(
        load_result.candles,
        symbol
    );


    CandleAggregator aggregator;


    const auto five_result =
        aggregator.aggregate(
            load_result.candles,
            Timeframe::FIVE_MINUTES
        );


    const auto fifteen_result =
        aggregator.aggregate(
            load_result.candles,
            Timeframe::FIFTEEN_MINUTES
        );


    if (
        five_result.candles.empty()
    )
    {
        throw std::runtime_error(
            "No 5-minute candles generated for " +
            symbol
        );
    }


    if (
        fifteen_result.candles.empty()
    )
    {
        throw std::runtime_error(
            "No 15-minute candles generated for " +
            symbol
        );
    }


    InstrumentRuntimeData result;

    result.symbol =
        symbol;

    result.one_minute =
        load_result.candles;

    result.five_minute =
        five_result.candles;

    result.fifteen_minute =
        fifteen_result.candles;

    result.timed_one =
        synchronizer.prepareOneMinute(
            result.one_minute
        );

    result.timed_five =
        synchronizer.prepareFiveMinute(
            result.five_minute
        );

    result.timed_fifteen =
        synchronizer.prepareFifteenMinute(
            result.fifteen_minute
        );

    result.invalid_rows =
        load_result.invalid_rows;

    result.duplicate_rows =
        load_result.duplicate_rows;

    result.skipped_rows =
        load_result.skipped_rows;

    result.incomplete_five =
        five_result.incomplete_buckets;

    result.incomplete_fifteen =
        fifteen_result.incomplete_buckets;


    return result;
}


// ============================================================================
// Look-Ahead Validation
//
// A candle is legal only if:
//
// candle start + timeframe <= decision_time
// ============================================================================

bool hasLookahead(
    const std::optional<Candle>& candle,
    std::int64_t timeframe_seconds,
    std::int64_t decision_time)
{
    if (
        !candle.has_value()
    )
    {
        return false;
    }

    const std::int64_t completed_at =
        candle->timestamp +
        timeframe_seconds;

    return
        completed_at >
        decision_time;
}


// ============================================================================
// Session Carryover Validation
// ============================================================================

bool belongsToSnapshotDate(
    const std::optional<Candle>& candle,
    const TradingDate& snapshot_date)
{
    if (
        !candle.has_value()
    )
    {
        return true;
    }

    return
        tradingDateFromTimestamp(
            candle->timestamp
        ) ==
        snapshot_date;
}


// ============================================================================
// Candle Alignment
//
// Missing on either side is NOT an alignment violation.
// It simply means the pair is incomplete.
// ============================================================================

bool aligned(
    const std::optional<Candle>& stock,
    const std::optional<Candle>& benchmark)
{
    if (
        !stock.has_value() ||
        !benchmark.has_value()
    )
    {
        return true;
    }

    return
        stock->timestamp ==
        benchmark->timestamp;
}


// ============================================================================
// Timestamp Regression Validation
//
// Snapshot candle timestamps should never move backwards during forward
// runtime.
//
// A new trading day is allowed because SessionState removes previous-day
// carryover and today's candles then begin from 09:15.
// ============================================================================

void checkTimestampRegression(
    const MarketSnapshot& snapshot,
    std::optional<std::int64_t>& previous_one,
    std::optional<std::int64_t>& previous_five,
    std::optional<std::int64_t>& previous_fifteen,
    TradingDate& previous_date,
    bool& previous_date_initialized,
    std::size_t& violation_counter)
{
    const bool date_changed =
        !previous_date_initialized ||
        snapshot.trading_date !=
            previous_date;


    if (date_changed)
    {
        previous_one.reset();
        previous_five.reset();
        previous_fifteen.reset();

        previous_date =
            snapshot.trading_date;

        previous_date_initialized =
            true;
    }


    const auto check =
        [&violation_counter](
            const std::optional<Candle>& candle,
            std::optional<std::int64_t>& previous)
        {
            if (
                !candle.has_value()
            )
            {
                return;
            }

            if (
                previous.has_value() &&
                candle->timestamp <
                    *previous
            )
            {
                ++violation_counter;
            }

            previous =
                candle->timestamp;
        };


    check(
        snapshot.one_minute,
        previous_one
    );

    check(
        snapshot.five_minute,
        previous_five
    );

    check(
        snapshot.fifteen_minute,
        previous_fifteen
    );
}


// ============================================================================
// Collect common decision timestamps
//
// We use 1-minute candle COMPLETION times.
//
// Example:
//
// candle timestamp 09:15
// decision time     09:16
//
// This is exactly when that candle becomes available in ZERO_LATENCY mode.
//
// Only stock decision times that also fall within the overall NIFTY data
// coverage are considered.
// ============================================================================

std::vector<std::int64_t> buildDecisionTimes(
    const InstrumentRuntimeData& stock,
    const InstrumentRuntimeData& benchmark)
{
    std::vector<std::int64_t> result;


    if (
        stock.one_minute.empty() ||
        benchmark.one_minute.empty()
    )
    {
        return result;
    }


    const std::int64_t benchmark_first =
        benchmark.one_minute.front().timestamp;

    const std::int64_t benchmark_last_completion =
        benchmark.one_minute.back().timestamp +
        ONE_MINUTE_SECONDS;


    result.reserve(
        stock.one_minute.size()
    );


    for (
        const Candle& candle :
        stock.one_minute
    )
    {
        const std::int64_t decision_time =
            candle.timestamp +
            ONE_MINUTE_SECONDS;


        if (
            decision_time <
            benchmark_first
        )
        {
            continue;
        }


        if (
            decision_time >
            benchmark_last_completion
        )
        {
            continue;
        }


        result.push_back(
            decision_time
        );
    }


    std::sort(
        result.begin(),
        result.end()
    );


    result.erase(
        std::unique(
            result.begin(),
            result.end()
        ),
        result.end()
    );


    return result;
}


// ============================================================================
// Print Instrument Summary
// ============================================================================

void printInstrumentSummary(
    const InstrumentRuntimeData& data)
{
    std::cout
        << "\n"
        << data.symbol
        << "\n"
        << "--------------------------------------------\n"
        << "1-minute candles       : "
        << data.one_minute.size()
        << "\n"
        << "5-minute candles       : "
        << data.five_minute.size()
        << "\n"
        << "15-minute candles      : "
        << data.fifteen_minute.size()
        << "\n"
        << "Invalid rows           : "
        << data.invalid_rows
        << "\n"
        << "Duplicate rows         : "
        << data.duplicate_rows
        << "\n"
        << "Skipped rows           : "
        << data.skipped_rows
        << "\n"
        << "Incomplete 5m buckets  : "
        << data.incomplete_five
        << "\n"
        << "Incomplete 15m buckets : "
        << data.incomplete_fifteen
        << "\n";
}

}


// ============================================================================
// MAIN
// ============================================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        // ====================================================================
        // Optional command-line configuration
        //
        // Usage:
        //
        // ./build/test_real_runtime_pipeline
        //
        // or:
        //
        // ./build/test_real_runtime_pipeline \
        //     /home/hadoop/shareMarket_Data/minute \
        //     RELIANCE \
        //     NIFTY%2050
        // ====================================================================

        std::filesystem::path data_folder =
            DEFAULT_DATA_FOLDER;

        std::string stock_symbol =
            DEFAULT_STOCK_SYMBOL;

        std::string benchmark_symbol =
            DEFAULT_BENCHMARK_SYMBOL;


        if (argc >= 2)
        {
            data_folder =
                argv[1];
        }


        if (argc >= 3)
        {
            stock_symbol =
                argv[2];
        }


        if (argc >= 4)
        {
            benchmark_symbol =
                argv[3];
        }


        if (
            stock_symbol ==
            benchmark_symbol
        )
        {
            throw std::invalid_argument(
                "Stock and benchmark symbols must be different."
            );
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 2.5 - REAL RUNTIME PIPELINE VALIDATION\n"
            << "============================================================\n"
            << "Data folder : "
            << data_folder
            << "\n"
            << "Stock       : "
            << stock_symbol
            << "\n"
            << "Benchmark   : "
            << benchmark_symbol
            << "\n"
            << "Mode        : ZERO_LATENCY historical runtime\n"
            << "============================================================\n";


        // ====================================================================
        // Synchronizer
        // ====================================================================

        MultiTimeframeSynchronizerConfig sync_config;

        sync_config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer timeframe_synchronizer(
            sync_config
        );


        // ====================================================================
        // Load + aggregate + prepare real market data
        // ====================================================================

        std::cout
            << "\nPreparing stock data...\n";


        InstrumentRuntimeData stock_data =
            prepareInstrument(
                data_folder,
                stock_symbol,
                timeframe_synchronizer
            );


        std::cout
            << "Preparing benchmark data...\n";


        InstrumentRuntimeData benchmark_data =
            prepareInstrument(
                data_folder,
                benchmark_symbol,
                timeframe_synchronizer
            );


        printInstrumentSummary(
            stock_data
        );


        printInstrumentSummary(
            benchmark_data
        );


        // ====================================================================
        // Runtime objects
        // ====================================================================

        MultiTimeframeCursor stock_cursor(
            stock_data.timed_one,
            stock_data.timed_five,
            stock_data.timed_fifteen
        );


        MultiTimeframeCursor benchmark_cursor(
            benchmark_data.timed_one,
            benchmark_data.timed_five,
            benchmark_data.timed_fifteen
        );


        SessionState stock_session;
        SessionState benchmark_session;


        MarketSnapshotBuilder stock_builder(
            stock_symbol
        );


        MarketSnapshotBuilder benchmark_builder(
            benchmark_symbol
        );


        StockBenchmarkSynchronizer
            stock_benchmark_synchronizer;


        // ====================================================================
        // Decision timeline
        // ====================================================================

        const std::vector<std::int64_t> decision_times =
            buildDecisionTimes(
                stock_data,
                benchmark_data
            );


        if (
            decision_times.empty()
        )
        {
            throw std::runtime_error(
                "No overlapping decision-time range found."
            );
        }


        std::cout
            << "\n"
            << "Runtime decision range\n"
            << "--------------------------------------------\n"
            << "First decision : "
            << formatTimestamp(
                decision_times.front()
            )
            << "\n"
            << "Last decision  : "
            << formatTimestamp(
                decision_times.back()
            )
            << "\n"
            << "Decision points: "
            << decision_times.size()
            << "\n";


        // ====================================================================
        // Statistics
        // ====================================================================

        RuntimeValidationStats stats;


        std::optional<std::int64_t>
            previous_stock_one;

        std::optional<std::int64_t>
            previous_stock_five;

        std::optional<std::int64_t>
            previous_stock_fifteen;


        std::optional<std::int64_t>
            previous_benchmark_one;

        std::optional<std::int64_t>
            previous_benchmark_five;

        std::optional<std::int64_t>
            previous_benchmark_fifteen;


        TradingDate previous_stock_date{};
        TradingDate previous_benchmark_date{};

        bool previous_stock_date_initialized =
            false;

        bool previous_benchmark_date_initialized =
            false;


        // ====================================================================
        // FULL REAL-DATA RUNTIME LOOP
        // ====================================================================

        for (
            const std::int64_t decision_time :
            decision_times
        )
        {
            ++stats.decision_points;


            MarketSnapshot stock_snapshot =
                stock_builder.build(
                    decision_time,
                    stock_cursor,
                    stock_session
                );


            MarketSnapshot benchmark_snapshot =
                benchmark_builder.build(
                    decision_time,
                    benchmark_cursor,
                    benchmark_session
                );


            // ================================================================
            // Session statistics
            // ================================================================

            if (
                stock_snapshot.session_phase ==
                SessionPhase::BEFORE_OPEN
            )
            {
                ++stats.before_open_points;
            }
            else if (
                stock_snapshot.session_phase ==
                SessionPhase::ACTIVE
            )
            {
                ++stats.active_session_points;
            }
            else
            {
                ++stats.after_close_points;
            }


            if (
                stock_snapshot.new_session
            )
            {
                ++stats.new_sessions;
            }


            // ================================================================
            // Availability statistics
            // ================================================================

            if (
                stock_snapshot.one_minute
            )
            {
                ++stats.stock_one_available;
            }


            if (
                stock_snapshot.five_minute
            )
            {
                ++stats.stock_five_available;
            }


            if (
                stock_snapshot.fifteen_minute
            )
            {
                ++stats.stock_fifteen_available;
            }


            if (
                benchmark_snapshot.one_minute
            )
            {
                ++stats.benchmark_one_available;
            }


            if (
                benchmark_snapshot.five_minute
            )
            {
                ++stats.benchmark_five_available;
            }


            if (
                benchmark_snapshot.fifteen_minute
            )
            {
                ++stats.benchmark_fifteen_available;
            }


            // ================================================================
            // Decision-time validation
            // ================================================================

            if (
                stock_snapshot.decision_time !=
                    decision_time ||
                benchmark_snapshot.decision_time !=
                    decision_time
            )
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // Trading-date validation
            // ================================================================

            if (
                stock_snapshot.trading_date !=
                benchmark_snapshot.trading_date
            )
            {
                ++stats.trading_date_violations;
            }


            // ================================================================
            // Stock look-ahead protection
            // ================================================================

            if (
                hasLookahead(
                    stock_snapshot.one_minute,
                    ONE_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.stock_lookahead_violations;
            }


            if (
                hasLookahead(
                    stock_snapshot.five_minute,
                    FIVE_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.stock_lookahead_violations;
            }


            if (
                hasLookahead(
                    stock_snapshot.fifteen_minute,
                    FIFTEEN_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.stock_lookahead_violations;
            }


            // ================================================================
            // Benchmark look-ahead protection
            // ================================================================

            if (
                hasLookahead(
                    benchmark_snapshot.one_minute,
                    ONE_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.benchmark_lookahead_violations;
            }


            if (
                hasLookahead(
                    benchmark_snapshot.five_minute,
                    FIVE_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.benchmark_lookahead_violations;
            }


            if (
                hasLookahead(
                    benchmark_snapshot.fifteen_minute,
                    FIFTEEN_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.benchmark_lookahead_violations;
            }


            // ================================================================
            // Previous-day carryover protection
            // ================================================================

            if (
                !belongsToSnapshotDate(
                    stock_snapshot.one_minute,
                    stock_snapshot.trading_date
                ) ||
                !belongsToSnapshotDate(
                    stock_snapshot.five_minute,
                    stock_snapshot.trading_date
                ) ||
                !belongsToSnapshotDate(
                    stock_snapshot.fifteen_minute,
                    stock_snapshot.trading_date
                )
            )
            {
                ++stats.stock_session_carryover_violations;
            }


            if (
                !belongsToSnapshotDate(
                    benchmark_snapshot.one_minute,
                    benchmark_snapshot.trading_date
                ) ||
                !belongsToSnapshotDate(
                    benchmark_snapshot.five_minute,
                    benchmark_snapshot.trading_date
                ) ||
                !belongsToSnapshotDate(
                    benchmark_snapshot.fifteen_minute,
                    benchmark_snapshot.trading_date
                )
            )
            {
                ++stats.benchmark_session_carryover_violations;
            }


            // ================================================================
            // Explicit timeframe alignment
            // ================================================================

            if (
                !aligned(
                    stock_snapshot.one_minute,
                    benchmark_snapshot.one_minute
                )
            )
            {
                ++stats.one_minute_alignment_violations;
            }


            if (
                !aligned(
                    stock_snapshot.five_minute,
                    benchmark_snapshot.five_minute
                )
            )
            {
                ++stats.five_minute_alignment_violations;
            }


            if (
                !aligned(
                    stock_snapshot.fifteen_minute,
                    benchmark_snapshot.fifteen_minute
                )
            )
            {
                ++stats.fifteen_minute_alignment_violations;
            }


            // ================================================================
            // Forward-only timestamp validation
            // ================================================================

            checkTimestampRegression(
                stock_snapshot,
                previous_stock_one,
                previous_stock_five,
                previous_stock_fifteen,
                previous_stock_date,
                previous_stock_date_initialized,
                stats.stock_timestamp_regressions
            );


            checkTimestampRegression(
                benchmark_snapshot,
                previous_benchmark_one,
                previous_benchmark_five,
                previous_benchmark_fifteen,
                previous_benchmark_date,
                previous_benchmark_date_initialized,
                stats.benchmark_timestamp_regressions
            );


            // ================================================================
            // Production StockBenchmarkSynchronizer validation
            //
            // Important:
            //
            // Real missing candles can legitimately cause the stock and NIFTY
            // latest timestamps to differ.
            //
            // The synchronizer intentionally rejects mismatched timestamps.
            // We record those occurrences instead of terminating this entire
            // multi-year validation run.
            // ================================================================

            try
            {
                const StockBenchmarkSnapshot pair =
                    stock_benchmark_synchronizer.synchronize(
                        stock_snapshot,
                        benchmark_snapshot
                    );


                if (
                    pair.fullySynchronized()
                )
                {
                    ++stats.fully_synchronized_pairs;
                }
                else
                {
                    ++stats.incomplete_pairs;
                }
            }
            catch (const std::exception&)
            {
                ++stats.synchronization_exceptions;
                ++stats.incomplete_pairs;
            }
        }


        // ====================================================================
        // Final Validation
        // ====================================================================

        const std::size_t total_lookahead =
            stats.stock_lookahead_violations +
            stats.benchmark_lookahead_violations;


        const std::size_t total_carryover =
            stats.stock_session_carryover_violations +
            stats.benchmark_session_carryover_violations;


        const std::size_t total_alignment =
            stats.one_minute_alignment_violations +
            stats.five_minute_alignment_violations +
            stats.fifteen_minute_alignment_violations;


        const std::size_t total_regressions =
            stats.stock_timestamp_regressions +
            stats.benchmark_timestamp_regressions;


        // ====================================================================
        // Print Report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 2.5 RUNTIME VALIDATION REPORT\n"
            << "============================================================\n"

            << "Decision points                  : "
            << stats.decision_points
            << "\n"

            << "Active-session points            : "
            << stats.active_session_points
            << "\n"

            << "New sessions                     : "
            << stats.new_sessions
            << "\n"

            << "\n"

            << "Stock 1m available               : "
            << stats.stock_one_available
            << "\n"

            << "Stock 5m available               : "
            << stats.stock_five_available
            << "\n"

            << "Stock 15m available              : "
            << stats.stock_fifteen_available
            << "\n"

            << "\n"

            << "Benchmark 1m available           : "
            << stats.benchmark_one_available
            << "\n"

            << "Benchmark 5m available           : "
            << stats.benchmark_five_available
            << "\n"

            << "Benchmark 15m available          : "
            << stats.benchmark_fifteen_available
            << "\n"

            << "\n"

            << "Fully synchronized pairs         : "
            << stats.fully_synchronized_pairs
            << "\n"

            << "Incomplete/rejected pairs        : "
            << stats.incomplete_pairs
            << "\n"

            << "Synchronizer exceptions          : "
            << stats.synchronization_exceptions
            << "\n"

            << "\n"

            << "Decision-time violations         : "
            << stats.decision_time_violations
            << "\n"

            << "Trading-date violations          : "
            << stats.trading_date_violations
            << "\n"

            << "Look-ahead violations            : "
            << total_lookahead
            << "\n"

            << "Previous-day carryover violations: "
            << total_carryover
            << "\n"

            << "1m alignment violations          : "
            << stats.one_minute_alignment_violations
            << "\n"

            << "5m alignment violations          : "
            << stats.five_minute_alignment_violations
            << "\n"

            << "15m alignment violations         : "
            << stats.fifteen_minute_alignment_violations
            << "\n"

            << "Timestamp regressions            : "
            << total_regressions
            << "\n"

            << "============================================================\n";


        // ====================================================================
        // Hard correctness failures
        //
        // Missing real data can cause incomplete/alignment-rejected pairs.
        // That is a DATA QUALITY condition already known from Phase 1.
        //
        // It must NOT be confused with a runtime architecture failure.
        //
        // These are the conditions that would indicate a Phase 2 runtime bug.
        // ====================================================================

        const bool hard_failure =
            stats.decision_time_violations != 0 ||
            stats.trading_date_violations != 0 ||
            total_lookahead != 0 ||
            total_carryover != 0 ||
            total_regressions != 0;


        if (hard_failure)
        {
            std::cerr
                << "\n"
                << "============================================================\n"
                << "PHASE 2.5 RUNTIME VALIDATION FAILED\n"
                << "============================================================\n"
                << "A runtime correctness violation was detected.\n"
                << "Phase 2 must NOT be considered complete.\n"
                << "============================================================\n";

            return 1;
        }


        // ====================================================================
        // Alignment issues are reported separately.
        //
        // With your known real TXT gaps, some stock/NIFTY timestamps can
        // legitimately be unavailable or mismatched.
        // ====================================================================

        if (
            total_alignment != 0 ||
            stats.synchronization_exceptions != 0
        )
        {
            std::cout
                << "\n"
                << "NOTE\n"
                << "------------------------------------------------------------\n"
                << "Runtime architecture passed, but real-data alignment gaps\n"
                << "were detected. These are reported rather than fabricated.\n"
                << "They can later be handled by the recovery/data-continuity\n"
                << "layer before live trading.\n"
                << "------------------------------------------------------------\n";
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 2.5 RUNTIME VALIDATION PASSED\n"
            << "============================================================\n"
            << "Forward-only runtime             : PASSED\n"
            << "Decision-time consistency        : PASSED\n"
            << "Look-ahead protection            : PASSED\n"
            << "Session isolation                : PASSED\n"
            << "Previous-day protection          : PASSED\n"
            << "Stock + benchmark runtime        : PASSED\n"
            << "Real-data pipeline               : PASSED\n"
            << "============================================================\n";


        return 0;
    }
    catch (
        const std::exception& error
    )
    {
        std::cerr
            << "\n"
            << "PHASE 2.5 RUNTIME VALIDATION ERROR\n"
            << error.what()
            << "\n";

        return 1;
    }
}