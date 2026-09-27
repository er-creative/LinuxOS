#include "devai/features/PriceReturnFeatureEngine.hpp"
#include "devai/features/PriceReturnFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/Timeframe.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace devai::market;
using namespace devai::features;


namespace
{

// ============================================================================
// Configuration
// ============================================================================

const std::filesystem::path DEFAULT_DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

const std::string DEFAULT_SYMBOL =
    "RELIANCE";

constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr std::int64_t FIVE_MINUTE_SECONDS =
    5 * 60;

constexpr std::int64_t FIFTEEN_MINUTE_SECONDS =
    15 * 60;


// ============================================================================
// Validation Statistics
// ============================================================================

struct ValidationStats
{
    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t new_sessions{0};

    std::size_t snapshots_with_1m{0};
    std::size_t snapshots_with_5m{0};
    std::size_t snapshots_with_15m{0};

    std::size_t one_minute_return_available{0};
    std::size_t five_minute_return_available{0};
    std::size_t fifteen_minute_return_available{0};

    std::size_t log_return_available{0};

    std::size_t return_5_bar_available{0};
    std::size_t return_15_bar_available{0};
    std::size_t return_30_bar_available{0};

    std::size_t candle_geometry_available{0};

    std::size_t invalid_numeric_values{0};

    std::size_t lookahead_violations{0};

    std::size_t symbol_violations{0};

    std::size_t decision_time_violations{0};

    std::size_t cross_session_continuity_violations{0};

    std::size_t duplicate_history_violations{0};

    std::size_t one_minute_return_mismatches{0};
    std::size_t five_minute_return_mismatches{0};
    std::size_t fifteen_minute_return_mismatches{0};

    std::size_t candle_return_mismatches{0};

    std::size_t geometry_violations{0};

    std::size_t feature_exceptions{0};
};


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


// ============================================================================
// Numeric Helpers
// ============================================================================

bool approximatelyEqual(
    double left,
    double right,
    double tolerance = 1e-10)
{
    if (
        !std::isfinite(left) ||
        !std::isfinite(right)
    )
    {
        return false;
    }

    const double scale =
        std::max(
            {
                1.0,
                std::fabs(left),
                std::fabs(right)
            }
        );

    return
        std::fabs(
            left - right
        ) <=
        tolerance * scale;
}


bool validFeatureNumber(
    double value)
{
    return
        std::isfinite(value);
}


// ============================================================================
// Look-Ahead
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

    return
        candle->timestamp +
            timeframe_seconds >
        decision_time;
}


// ============================================================================
// Expected Return
// ============================================================================

double expectedCloseReturn(
    const Candle& current,
    const Candle& previous)
{
    if (
        previous.close <= 0.0 ||
        current.close <= 0.0
    )
    {
        return
            std::numeric_limits<double>::quiet_NaN();
    }

    return
        (
            current.close /
            previous.close
        ) -
        1.0;
}


double expectedCandleReturn(
    const Candle& candle)
{
    if (
        candle.open <= 0.0
    )
    {
        return
            std::numeric_limits<double>::quiet_NaN();
    }

    return
        (
            candle.close -
            candle.open
        ) /
        candle.open;
}


// ============================================================================
// Validate feature numeric integrity
//
// NaN is LEGAL during warm-up.
// Infinity is never legal.
// ============================================================================

void validateNoInfinity(
    const PriceReturnFeatures& features,
    ValidationStats& stats)
{
    const double values[] =
    {
        features.one_minute_candle_return,
        features.five_minute_candle_return,
        features.fifteen_minute_candle_return,

        features.one_minute_return,
        features.five_minute_return,
        features.fifteen_minute_return,

        features.one_minute_log_return,
        features.five_minute_log_return,
        features.fifteen_minute_log_return,

        features.return_5_bars,
        features.return_15_bars,
        features.return_30_bars,

        features.candle_body_percent,
        features.candle_range_percent,
        features.close_location
    };


    for (
        const double value :
        values
    )
    {
        if (
            std::isinf(value)
        )
        {
            ++stats.invalid_numeric_values;
        }
    }
}


// ============================================================================
// MAIN
// ============================================================================

}


// ============================================================================
// Program Entry
// ============================================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        // ====================================================================
        // Arguments
        //
        // Default:
        //
        // /home/hadoop/shareMarket_Data/minute
        // RELIANCE
        //
        // Optional:
        //
        // ./build/test_real_price_return_features \
        //     "/home/hadoop/shareMarket_Data/minute" \
        //     "RELIANCE"
        // ====================================================================

        std::filesystem::path data_folder =
            DEFAULT_DATA_FOLDER;

        std::string symbol =
            DEFAULT_SYMBOL;


        if (
            argc >= 2
        )
        {
            data_folder =
                argv[1];
        }


        if (
            argc >= 3
        )
        {
            symbol =
                argv[2];
        }


        const std::filesystem::path filename =
            data_folder /
            (
                symbol +
                "_1min.txt"
            );


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.1 - REAL PRICE / RETURN FEATURE VALIDATION\n"
            << "============================================================\n"
            << "Data folder : "
            << data_folder
            << "\n"
            << "File        : "
            << filename
            << "\n"
            << "Symbol      : "
            << symbol
            << "\n"
            << "Mode        : ZERO_LATENCY historical runtime\n"
            << "============================================================\n";


        // ====================================================================
        // File validation
        // ====================================================================

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


        // ====================================================================
        // Load real 1-minute data
        // ====================================================================

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
                "No 1-minute candles loaded."
            );
        }


        for (
            const Candle& candle :
            load_result.candles
        )
        {
            if (
                candle.symbol !=
                symbol
            )
            {
                throw std::runtime_error(
                    "Unexpected symbol inside market-data file."
                );
            }
        }


        // ====================================================================
        // Aggregate
        // ====================================================================

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
                "No 5-minute candles generated."
            );
        }


        if (
            fifteen_result.candles.empty()
        )
        {
            throw std::runtime_error(
                "No 15-minute candles generated."
            );
        }


        std::cout
            << "\n"
            << "Market data\n"
            << "------------------------------------------------------------\n"
            << "1-minute candles       : "
            << load_result.candles.size()
            << "\n"
            << "5-minute candles       : "
            << five_result.candles.size()
            << "\n"
            << "15-minute candles      : "
            << fifteen_result.candles.size()
            << "\n"
            << "Invalid rows           : "
            << load_result.invalid_rows
            << "\n"
            << "Duplicate rows         : "
            << load_result.duplicate_rows
            << "\n"
            << "Skipped rows           : "
            << load_result.skipped_rows
            << "\n"
            << "Incomplete 5m buckets  : "
            << five_result.incomplete_buckets
            << "\n"
            << "Incomplete 15m buckets : "
            << fifteen_result.incomplete_buckets
            << "\n";


        // ====================================================================
        // Prepare synchronized historical streams
        // ====================================================================

        MultiTimeframeSynchronizerConfig config;

        config.mode =
            AvailabilityMode::ZERO_LATENCY;


        MultiTimeframeSynchronizer synchronizer(
            config
        );


        const auto timed_one =
            synchronizer.prepareOneMinute(
                load_result.candles
            );


        const auto timed_five =
            synchronizer.prepareFiveMinute(
                five_result.candles
            );


        const auto timed_fifteen =
            synchronizer.prepareFifteenMinute(
                fifteen_result.candles
            );


        // ====================================================================
        // Phase 2 runtime
        // ====================================================================

        MultiTimeframeCursor cursor(
            timed_one,
            timed_five,
            timed_fifteen
        );


        SessionState session;


        MarketSnapshotBuilder snapshot_builder(
            symbol
        );


        // ====================================================================
        // Phase 3.1
        //
        // Keep enough rolling history for 30-bar features across sessions.
        // ====================================================================

        PriceReturnFeatureEngine feature_engine(
            symbol,
            64
        );


        ValidationStats stats;


        // ====================================================================
        // Independent expected-state tracking
        //
        // These are NOT used by the feature engine.
        // They exist only to independently verify its output.
        // ====================================================================

        std::optional<Candle>
            previous_one;

        std::optional<Candle>
            previous_five;

        std::optional<Candle>
            previous_fifteen;


        std::optional<std::int64_t>
            last_seen_one_timestamp;

        std::optional<std::int64_t>
            last_seen_five_timestamp;

        std::optional<std::int64_t>
            last_seen_fifteen_timestamp;


        // ====================================================================
        // Runtime range
        //
        // Every real 1-minute candle becomes available one minute after its
        // market timestamp.
        // ====================================================================

        const std::int64_t first_decision_time =
            load_result.candles.front().timestamp +
            ONE_MINUTE_SECONDS;


        const std::int64_t last_decision_time =
            load_result.candles.back().timestamp +
            ONE_MINUTE_SECONDS;


        std::cout
            << "\n"
            << "Runtime range\n"
            << "------------------------------------------------------------\n"
            << "First decision : "
            << formatTimestamp(
                first_decision_time
            )
            << "\n"
            << "Last decision  : "
            << formatTimestamp(
                last_decision_time
            )
            << "\n";


        // ====================================================================
        // Full real-data runtime loop
        //
        // We use every actual 1-minute candle completion as a decision point.
        // ====================================================================

        for (
            const Candle& source_candle :
            load_result.candles
        )
        {
            const std::int64_t decision_time =
                source_candle.timestamp +
                ONE_MINUTE_SECONDS;


            ++stats.decision_points;


            // ================================================================
            // Build legal point-in-time snapshot
            // ================================================================

            MarketSnapshot snapshot =
                snapshot_builder.build(
                    decision_time,
                    cursor,
                    session
                );


            if (
                snapshot.sessionActive()
            )
            {
                ++stats.active_session_points;
            }


            if (
                snapshot.new_session
            )
            {
                ++stats.new_sessions;

                // CETE continuous-history policy:
                //
                // A new trading session does NOT reset price/return history.
                // The previous completed trading candle remains the reference
                // observation for the next real completed candle.
                //
                // Therefore the independent validation state intentionally
                // remains unchanged here.
            }


            // ================================================================
            // Snapshot integrity
            // ================================================================

            if (
                snapshot.symbol !=
                symbol
            )
            {
                ++stats.symbol_violations;
            }


            if (
                snapshot.decision_time !=
                decision_time
            )
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // Look-ahead validation BEFORE features
            // ================================================================

            if (
                hasLookahead(
                    snapshot.one_minute,
                    ONE_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.lookahead_violations;
            }


            if (
                hasLookahead(
                    snapshot.five_minute,
                    FIVE_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.lookahead_violations;
            }


            if (
                hasLookahead(
                    snapshot.fifteen_minute,
                    FIFTEEN_MINUTE_SECONDS,
                    decision_time
                )
            )
            {
                ++stats.lookahead_violations;
            }


            // ================================================================
            // Availability
            // ================================================================

            if (
                snapshot.one_minute
            )
            {
                ++stats.snapshots_with_1m;
            }


            if (
                snapshot.five_minute
            )
            {
                ++stats.snapshots_with_5m;
            }


            if (
                snapshot.fifteen_minute
            )
            {
                ++stats.snapshots_with_15m;
            }


            // ================================================================
            // Detect whether each timeframe contains a genuinely NEW candle.
            //
            // We need this because 5m/15m snapshots repeat their latest candle
            // between completions.
            // ================================================================

            bool new_one =
                false;

            bool new_five =
                false;

            bool new_fifteen =
                false;


            if (
                snapshot.one_minute
            )
            {
                if (
                    !last_seen_one_timestamp ||
                    snapshot.one_minute->timestamp !=
                        *last_seen_one_timestamp
                )
                {
                    new_one =
                        true;
                }
            }


            if (
                snapshot.five_minute
            )
            {
                if (
                    !last_seen_five_timestamp ||
                    snapshot.five_minute->timestamp !=
                        *last_seen_five_timestamp
                )
                {
                    new_five =
                        true;
                }
            }


            if (
                snapshot.fifteen_minute
            )
            {
                if (
                    !last_seen_fifteen_timestamp ||
                    snapshot.fifteen_minute->timestamp !=
                        *last_seen_fifteen_timestamp
                )
                {
                    new_fifteen =
                        true;
                }
            }


            // ================================================================
            // Save history sizes before feature update.
            // ================================================================

            const std::size_t one_size_before =
                feature_engine.oneMinuteHistorySize();

            const std::size_t five_size_before =
                feature_engine.fiveMinuteHistorySize();

            const std::size_t fifteen_size_before =
                feature_engine.fifteenMinuteHistorySize();


            // ================================================================
            // Run Phase 3.1
            // ================================================================

            PriceReturnFeatures features;

            try
            {
                features =
                    feature_engine.update(
                        snapshot
                    );
            }
            catch (
                const std::exception& error
            )
            {
                ++stats.feature_exceptions;

                std::cerr
                    << "\nFeature exception at "
                    << formatTimestamp(
                        decision_time
                    )
                    << ": "
                    << error.what()
                    << "\n";

                continue;
            }


            // ================================================================
            // Feature identity
            // ================================================================

            if (
                features.symbol !=
                symbol
            )
            {
                ++stats.symbol_violations;
            }


            if (
                features.decision_time !=
                decision_time
            )
            {
                ++stats.decision_time_violations;
            }


            // ================================================================
            // No infinity
            //
            // NaN during warm-up is expected.
            // ================================================================

            validateNoInfinity(
                features,
                stats
            );


            // ================================================================
            // Duplicate-history and cross-session continuity validation
            //
            // Expected size increase:
            //
            // new candle      -> +1, unless bounded history is already full
            // repeated candle -> +0
            //
            // CETE policy: new_session NEVER clears continuous history.
            // ================================================================

            const std::size_t one_size_after =
                feature_engine.oneMinuteHistorySize();

            const std::size_t five_size_after =
                feature_engine.fiveMinuteHistorySize();

            const std::size_t fifteen_size_after =
                feature_engine.fifteenMinuteHistorySize();


            const auto validate_history_transition =
                [&](
                    bool new_candle,
                    std::size_t before,
                    std::size_t after)
                {
                    const std::size_t expected_after =
                        new_candle
                            ? std::min<std::size_t>(before + 1, 64)
                            : before;

                    if (after != expected_after)
                    {
                        ++stats.duplicate_history_violations;
                    }
                };


            validate_history_transition(
                new_one,
                one_size_before,
                one_size_after
            );

            validate_history_transition(
                new_five,
                five_size_before,
                five_size_after
            );

            validate_history_transition(
                new_fifteen,
                fifteen_size_before,
                fifteen_size_after
            );


            // Explicitly verify that a session transition itself never causes
            // any continuous timeframe history to shrink.

            if (snapshot.new_session)
            {
                if (
                    one_size_after < one_size_before ||
                    five_size_after < five_size_before ||
                    fifteen_size_after < fifteen_size_before
                )
                {
                    ++stats.cross_session_continuity_violations;
                }
            }


            // ================================================================
            // Current candle return validation
            // ================================================================

            if (
                snapshot.one_minute
            )
            {
                const double expected =
                    expectedCandleReturn(
                        *snapshot.one_minute
                    );


                if (
                    !approximatelyEqual(
                        features.one_minute_candle_return,
                        expected
                    )
                )
                {
                    ++stats.candle_return_mismatches;
                }


                if (
                    validFeatureNumber(
                        features.candle_body_percent
                    ) &&
                    validFeatureNumber(
                        features.candle_range_percent
                    ) &&
                    validFeatureNumber(
                        features.close_location
                    )
                )
                {
                    ++stats.candle_geometry_available;
                }


                if (
                    std::isfinite(
                        features.close_location
                    ) &&
                    (
                        features.close_location < -1e-12 ||
                        features.close_location > 1.0 + 1e-12
                    )
                )
                {
                    ++stats.geometry_violations;
                }


                if (
                    std::isfinite(
                        features.candle_range_percent
                    ) &&
                    features.candle_range_percent < 0.0
                )
                {
                    ++stats.geometry_violations;
                }
            }


            if (
                snapshot.five_minute
            )
            {
                const double expected =
                    expectedCandleReturn(
                        *snapshot.five_minute
                    );


                if (
                    !approximatelyEqual(
                        features.five_minute_candle_return,
                        expected
                    )
                )
                {
                    ++stats.candle_return_mismatches;
                }
            }


            if (
                snapshot.fifteen_minute
            )
            {
                const double expected =
                    expectedCandleReturn(
                        *snapshot.fifteen_minute
                    );


                if (
                    !approximatelyEqual(
                        features.fifteen_minute_candle_return,
                        expected
                    )
                )
                {
                    ++stats.candle_return_mismatches;
                }
            }


            // ================================================================
            // Independent close-to-close return validation
            //
            // Validate only when a NEW candle arrived.
            //
            // Repeated 5m/15m snapshots should simply retain the most recently
            // calculated return.
            // ================================================================

            if (
                new_one &&
                snapshot.one_minute
            )
            {
                if (
                    previous_one
                )
                {
                    const double expected =
                        expectedCloseReturn(
                            *snapshot.one_minute,
                            *previous_one
                        );


                    if (
                        !approximatelyEqual(
                            features.one_minute_return,
                            expected
                        )
                    )
                    {
                        ++stats.one_minute_return_mismatches;
                    }
                    else
                    {
                        ++stats.one_minute_return_available;
                    }
                }
                else
                {
                    if (
                        !std::isnan(
                            features.one_minute_return
                        )
                    )
                    {
                        ++stats.one_minute_return_mismatches;
                    }
                }


                previous_one =
                    *snapshot.one_minute;

                last_seen_one_timestamp =
                    snapshot.one_minute->timestamp;
            }


            if (
                new_five &&
                snapshot.five_minute
            )
            {
                if (
                    previous_five
                )
                {
                    const double expected =
                        expectedCloseReturn(
                            *snapshot.five_minute,
                            *previous_five
                        );


                    if (
                        !approximatelyEqual(
                            features.five_minute_return,
                            expected
                        )
                    )
                    {
                        ++stats.five_minute_return_mismatches;
                    }
                    else
                    {
                        ++stats.five_minute_return_available;
                    }
                }
                else
                {
                    if (
                        !std::isnan(
                            features.five_minute_return
                        )
                    )
                    {
                        ++stats.five_minute_return_mismatches;
                    }
                }


                previous_five =
                    *snapshot.five_minute;

                last_seen_five_timestamp =
                    snapshot.five_minute->timestamp;
            }


            if (
                new_fifteen &&
                snapshot.fifteen_minute
            )
            {
                if (
                    previous_fifteen
                )
                {
                    const double expected =
                        expectedCloseReturn(
                            *snapshot.fifteen_minute,
                            *previous_fifteen
                        );


                    if (
                        !approximatelyEqual(
                            features.fifteen_minute_return,
                            expected
                        )
                    )
                    {
                        ++stats.fifteen_minute_return_mismatches;
                    }
                    else
                    {
                        ++stats.fifteen_minute_return_available;
                    }
                }
                else
                {
                    if (
                        !std::isnan(
                            features.fifteen_minute_return
                        )
                    )
                    {
                        ++stats.fifteen_minute_return_mismatches;
                    }
                }


                previous_fifteen =
                    *snapshot.fifteen_minute;

                last_seen_fifteen_timestamp =
                    snapshot.fifteen_minute->timestamp;
            }


            // ================================================================
            // Log-return availability
            // ================================================================

            if (
                std::isfinite(
                    features.one_minute_log_return
                )
            )
            {
                ++stats.log_return_available;
            }


            // ================================================================
            // Multi-bar feature availability
            // ================================================================

            if (
                features.has_5_bar_history
            )
            {
                if (
                    !std::isfinite(
                        features.return_5_bars
                    )
                )
                {
                    ++stats.invalid_numeric_values;
                }
                else
                {
                    ++stats.return_5_bar_available;
                }
            }


            if (
                features.has_15_bar_history
            )
            {
                if (
                    !std::isfinite(
                        features.return_15_bars
                    )
                )
                {
                    ++stats.invalid_numeric_values;
                }
                else
                {
                    ++stats.return_15_bar_available;
                }
            }


            if (
                features.has_30_bar_history
            )
            {
                if (
                    !std::isfinite(
                        features.return_30_bars
                    )
                )
                {
                    ++stats.invalid_numeric_values;
                }
                else
                {
                    ++stats.return_30_bar_available;
                }
            }
        }


        // ====================================================================
        // Final Validation
        // ====================================================================

        const std::size_t total_return_mismatches =
            stats.one_minute_return_mismatches +
            stats.five_minute_return_mismatches +
            stats.fifteen_minute_return_mismatches;


        const bool failed =
            stats.lookahead_violations != 0 ||
            stats.symbol_violations != 0 ||
            stats.decision_time_violations != 0 ||
            stats.cross_session_continuity_violations != 0 ||
            stats.duplicate_history_violations != 0 ||
            stats.invalid_numeric_values != 0 ||
            total_return_mismatches != 0 ||
            stats.candle_return_mismatches != 0 ||
            stats.geometry_violations != 0 ||
            stats.feature_exceptions != 0;


        // ====================================================================
        // Report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.1 REAL FEATURE VALIDATION REPORT\n"
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

            << "Snapshots with 1m                : "
            << stats.snapshots_with_1m
            << "\n"

            << "Snapshots with 5m                : "
            << stats.snapshots_with_5m
            << "\n"

            << "Snapshots with 15m               : "
            << stats.snapshots_with_15m
            << "\n"

            << "\n"

            << "Valid 1m close returns           : "
            << stats.one_minute_return_available
            << "\n"

            << "Valid 5m close returns           : "
            << stats.five_minute_return_available
            << "\n"

            << "Valid 15m close returns          : "
            << stats.fifteen_minute_return_available
            << "\n"

            << "Valid 1m log returns             : "
            << stats.log_return_available
            << "\n"

            << "\n"

            << "5-bar features available         : "
            << stats.return_5_bar_available
            << "\n"

            << "15-bar features available        : "
            << stats.return_15_bar_available
            << "\n"

            << "30-bar features available        : "
            << stats.return_30_bar_available
            << "\n"

            << "Candle geometry available        : "
            << stats.candle_geometry_available
            << "\n"

            << "\n"

            << "Look-ahead violations            : "
            << stats.lookahead_violations
            << "\n"

            << "Symbol violations                : "
            << stats.symbol_violations
            << "\n"

            << "Decision-time violations         : "
            << stats.decision_time_violations
            << "\n"

            << "Cross-session continuity viol.  : "
            << stats.cross_session_continuity_violations
            << "\n"

            << "Duplicate-history violations     : "
            << stats.duplicate_history_violations
            << "\n"

            << "1m return mismatches             : "
            << stats.one_minute_return_mismatches
            << "\n"

            << "5m return mismatches             : "
            << stats.five_minute_return_mismatches
            << "\n"

            << "15m return mismatches            : "
            << stats.fifteen_minute_return_mismatches
            << "\n"

            << "Candle-return mismatches         : "
            << stats.candle_return_mismatches
            << "\n"

            << "Geometry violations              : "
            << stats.geometry_violations
            << "\n"

            << "Invalid numeric values           : "
            << stats.invalid_numeric_values
            << "\n"

            << "Feature exceptions               : "
            << stats.feature_exceptions
            << "\n"

            << "============================================================\n";


        if (
            failed
        )
        {
            std::cerr
                << "\n"
                << "============================================================\n"
                << "PHASE 3.1 REAL FEATURE VALIDATION FAILED\n"
                << "============================================================\n"
                << "Price/return feature correctness violation detected.\n"
                << "Phase 3.1 must NOT be considered complete.\n"
                << "============================================================\n";

            return 1;
        }


        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 3.1 REAL FEATURE VALIDATION PASSED\n"
            << "============================================================\n"
            << "Real market-data integration     : PASSED\n"
            << "1m price/return features         : PASSED\n"
            << "5m price/return features         : PASSED\n"
            << "15m price/return features        : PASSED\n"
            << "Multi-bar returns                : PASSED\n"
            << "Candle geometry                  : PASSED\n"
            << "Warm-up handling                 : PASSED\n"
            << "Duplicate-candle protection      : PASSED\n"
            << "Cross-session continuity         : PASSED\n"
            << "Look-ahead protection            : PASSED\n"
            << "Numeric integrity                : PASSED\n"
            << "============================================================\n";


        return 0;
    }
    catch (
        const std::exception& error
    )
    {
        std::cerr
            << "\n"
            << "PHASE 3.1 REAL FEATURE VALIDATION ERROR\n"
            << error.what()
            << "\n";

        return 1;
    }
}