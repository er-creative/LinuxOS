#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/Timeframe.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"

#include "devai/statistics/RollingStatisticalEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr std::size_t WINDOW_SIZE = 60;

constexpr double ABSOLUTE_TOLERANCE = 1.0e-9;
constexpr double RELATIVE_TOLERANCE = 1.0e-10;

constexpr double ROBUST_Z_CONSTANT =
    0.6744897501960817;


// ============================================================
// Comparison helper
// ============================================================

bool approximatelyEqual(
    double lhs,
    double rhs)
{
    if (!std::isfinite(lhs) ||
        !std::isfinite(rhs))
    {
        return false;
    }

    const double difference =
        std::abs(lhs - rhs);

    if (difference <= ABSOLUTE_TOLERANCE)
    {
        return true;
    }

    const double scale =
        std::max(
            {
                1.0,
                std::abs(lhs),
                std::abs(rhs)
            });

    return difference <=
           RELATIVE_TOLERANCE * scale;
}


// ============================================================
// Independent mean
// ============================================================

double referenceMean(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    long double sum = 0.0L;

    for (const double value : values)
    {
        sum += static_cast<long double>(value);
    }

    return static_cast<double>(
        sum /
        static_cast<long double>(
            values.size()));
}


// ============================================================
// Independent population standard deviation
//
// This intentionally does NOT use RollingStatistics.
// It is an independent reference implementation.
// ============================================================

double referencePopulationStdDev(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    const double mean =
        referenceMean(values);

    long double squared_sum = 0.0L;

    for (const double value : values)
    {
        const long double difference =
            static_cast<long double>(value) -
            static_cast<long double>(mean);

        squared_sum +=
            difference * difference;
    }

    const long double variance =
        squared_sum /
        static_cast<long double>(
            values.size());

    return std::sqrt(
        static_cast<double>(variance));
}


// ============================================================
// Independent median
// ============================================================

double referenceMedian(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    std::vector<double> copy(
        values.begin(),
        values.end());

    std::sort(
        copy.begin(),
        copy.end());

    const std::size_t size =
        copy.size();

    const std::size_t middle =
        size / 2;

    if ((size % 2) != 0)
    {
        return copy[middle];
    }

    return
        (copy[middle - 1] +
         copy[middle]) /
        2.0;
}


// ============================================================
// Independent MAD
// ============================================================

double referenceMAD(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    const double median =
        referenceMedian(values);

    std::vector<double> deviations;

    deviations.reserve(
        values.size());

    for (const double value : values)
    {
        deviations.push_back(
            std::abs(value - median));
    }

    std::sort(
        deviations.begin(),
        deviations.end());

    const std::size_t size =
        deviations.size();

    const std::size_t middle =
        size / 2;

    if ((size % 2) != 0)
    {
        return deviations[middle];
    }

    return
        (deviations[middle - 1] +
         deviations[middle]) /
        2.0;
}


// ============================================================
// Independent standard Z-score
// ============================================================

double referenceZScore(
    double value,
    const std::deque<double>& values)
{
    const double mean =
        referenceMean(values);

    const double stddev =
        referencePopulationStdDev(
            values);

    if (!std::isfinite(mean) ||
        !std::isfinite(stddev))
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    if (stddev <= 1.0e-12)
    {
        return 0.0;
    }

    return
        (value - mean) /
        stddev;
}


// ============================================================
// Independent robust Z-score
// ============================================================

double referenceRobustZScore(
    double value,
    const std::deque<double>& values)
{
    const double median =
        referenceMedian(values);

    const double mad =
        referenceMAD(values);

    if (!std::isfinite(median) ||
        !std::isfinite(mad))
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    if (mad <= 1.0e-12)
    {
        return 0.0;
    }

    return
        ROBUST_Z_CONSTANT *
        (value - median) /
        mad;
}


// ============================================================
// Validation report
// ============================================================

struct ValidationReport
{
    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t snapshots_with_1m{0};

    std::size_t observations{0};

    std::size_t ready_observations{0};

    std::size_t new_sessions{0};

    std::size_t mean_mismatches{0};

    std::size_t stddev_mismatches{0};

    std::size_t zscore_mismatches{0};

    std::size_t median_mismatches{0};

    std::size_t mad_mismatches{0};

    std::size_t robust_zscore_mismatches{0};

    std::size_t readiness_mismatches{0};

    std::size_t lookahead_violations{0};

    std::size_t decision_time_violations{0};

    std::size_t
        cross_session_continuity_violations{0};

    std::size_t invalid_numeric_values{0};

    std::size_t statistical_exceptions{0};
};

} // namespace


// ============================================================
// MAIN
// ============================================================

int main(
    int argc,
    char* argv[])
{
    try
    {
        // ====================================================
        // Command-line argument
        // ====================================================

        if (argc != 2)
        {
            std::cerr
                << "Usage:\n\n"
                << "  "
                << argv[0]
                << " <1-minute-market-data-file>\n\n";

            return EXIT_FAILURE;
        }

        const std::filesystem::path data_file =
            argv[1];

        if (!std::filesystem::exists(
                data_file))
        {
            throw std::runtime_error(
                "Market-data file does not exist: " +
                data_file.string());
        }


        // ====================================================
        // Header
        // ====================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 4.1 — REAL ROLLING STATISTICAL VALIDATION\n"
            << "============================================================\n"
            << "File        : "
            << data_file
            << "\n"
            << "Window size : "
            << WINDOW_SIZE
            << "\n"
            << "Input value : legally available 1-minute close\n"
            << "============================================================\n";


        // ====================================================
        // Phase 1 — Load real 1-minute data
        // ====================================================

        const std::filesystem::path
            data_directory =
                data_file.parent_path();

        const std::string filename =
            data_file.filename().string();

        devai::market::MarketDataLoader loader(
            data_directory);

        const auto load_result =
            loader.loadFile(filename);

        if (load_result.candles.empty())
        {
            throw std::runtime_error(
                "No valid 1-minute candles were loaded.");
        }

        const std::string symbol =
            load_result.candles.front().symbol;

        std::cout
            << "Symbol      : "
            << symbol
            << "\n"
            << "1m candles  : "
            << load_result.candles.size()
            << "\n"
            << "Invalid rows: "
            << load_result.invalid_rows
            << "\n"
            << "Duplicates  : "
            << load_result.duplicate_rows
            << "\n"
            << "Skipped     : "
            << load_result.skipped_rows
            << "\n";


        // ====================================================
        // Phase 1 — Aggregate 5m and 15m
        // ====================================================

        devai::market::CandleAggregator
            aggregator;

        const auto five_minute_result =
            aggregator.aggregate(
                load_result.candles,
                devai::market::Timeframe::
                    FIVE_MINUTES);

        const auto fifteen_minute_result =
            aggregator.aggregate(
                load_result.candles,
                devai::market::Timeframe::
                    FIFTEEN_MINUTES);

        std::cout
            << "5m candles  : "
            << five_minute_result.candles.size()
            << "\n"
            << "15m candles : "
            << fifteen_minute_result.candles.size()
            << "\n"
            << "Incomplete 5m buckets  : "
            << five_minute_result.
                incomplete_buckets
            << "\n"
            << "Incomplete 15m buckets : "
            << fifteen_minute_result.
                incomplete_buckets
            << "\n";


        // ====================================================
        // Phase 1 — Multi-timeframe availability
        //
        // ZERO_LATENCY:
        //
        // 09:15 1m candle -> available at 09:16
        // 09:15 5m candle -> available at 09:20
        // 09:15 15m candle -> available at 09:30
        // ====================================================

        devai::market::
            MultiTimeframeSynchronizerConfig
                synchronizer_config;

        synchronizer_config.mode =
            devai::market::AvailabilityMode::
                ZERO_LATENCY;

        devai::market::
            MultiTimeframeSynchronizer
                synchronizer(
                    synchronizer_config);

        const auto one_minute_timed =
            synchronizer.prepareOneMinute(
                load_result.candles);

        const auto five_minute_timed =
            synchronizer.prepareFiveMinute(
                five_minute_result.candles);

        const auto fifteen_minute_timed =
            synchronizer.prepareFifteenMinute(
                fifteen_minute_result.candles);


        // ====================================================
        // Phase 2 — Runtime cursor
        // ====================================================

        devai::market::MultiTimeframeCursor
            cursor(
                one_minute_timed,
                five_minute_timed,
                fifteen_minute_timed);


        // ====================================================
        // Phase 2 — Session state
        // ====================================================

        devai::market::SessionState
            session_state;


        // ====================================================
        // Phase 2 — Market snapshot builder
        //
        // The builder owns only its expected symbol.
        //
        // Cursor + SessionState are supplied to build().
        // ====================================================

        devai::market::MarketSnapshotBuilder
            snapshot_builder(symbol);


        // ====================================================
        // Phase 4.1 — Engine under validation
        // ====================================================

        devai::statistics::
            RollingStatisticalEngine
                statistical_engine(
                    symbol,
                    WINDOW_SIZE);


        // ====================================================
        // Independent reference state
        // ====================================================

        std::deque<double>
            reference_window;

        ValidationReport report;

        std::int64_t
            previous_engine_decision_time{0};

        bool
            have_previous_engine_decision_time{
                false};

        std::size_t
            previous_observation_count{0};

        std::int64_t
            previous_observed_candle_timestamp{0};

        bool
            have_previous_observed_candle{
                false};


        // ====================================================
        // Real historical runtime replay
        //
        // Each 1-minute candle is evaluated at its completion:
        //
        // candle timestamp 09:15
        // decision time    09:16
        //
        // This preserves the Phase-2 no-look-ahead rule.
        // ====================================================

        for (const auto& source_candle :
             load_result.candles)
        {
            const std::int64_t decision_time =
                source_candle.timestamp + 60;

            ++report.decision_points;


            // =================================================
            // Build legally available Phase-2 snapshot
            // =================================================

            const auto snapshot =
                snapshot_builder.build(
                    decision_time,
                    cursor,
                    session_state);


            // =================================================
            // Cross-session continuity
            //
            // Phase 4 statistical history must NOT reset
            // automatically when a new trading session starts.
            // =================================================

            if (snapshot.new_session)
            {
                ++report.new_sessions;

                if (previous_observation_count > 0 &&
                    statistical_engine.
                        observationCount() == 0)
                {
                    ++report.
                        cross_session_continuity_violations;
                }
            }


            // =================================================
            // Only active trading session observations
            // =================================================

            if (snapshot.session_phase !=
                devai::market::SessionPhase::
                    ACTIVE)
            {
                continue;
            }

            ++report.active_session_points;


            // =================================================
            // Require legally available 1m candle
            // =================================================

            if (!snapshot.one_minute.has_value())
            {
                continue;
            }

            ++report.snapshots_with_1m;

            const auto& candle =
                *snapshot.one_minute;


            // =================================================
            // No look-ahead validation
            // =================================================

            const std::int64_t
                candle_completed_at =
                    candle.timestamp + 60;

            if (candle_completed_at >
                decision_time)
            {
                ++report.lookahead_violations;

                continue;
            }


            // =================================================
            // Do not count a repeated source candle twice.
            //
            // This is important for the CETE architecture.
            // Repeated 5m/15m snapshots must later follow the
            // same rule.
            // =================================================

            if (have_previous_observed_candle)
            {
                if (candle.timestamp <
                    previous_observed_candle_timestamp)
                {
                    ++report.
                        decision_time_violations;

                    continue;
                }

                if (candle.timestamp ==
                    previous_observed_candle_timestamp)
                {
                    continue;
                }
            }


            // =================================================
            // Decision-time monotonicity
            // =================================================

            if (have_previous_engine_decision_time &&
                decision_time <=
                    previous_engine_decision_time)
            {
                ++report.
                    decision_time_violations;

                continue;
            }


            // =================================================
            // Observation
            //
            // Phase 4.1 is generic mathematics, so for this
            // integration validation we use real 1m CLOSE.
            // =================================================

            const double value =
                candle.close;

            if (!std::isfinite(value))
            {
                ++report.invalid_numeric_values;

                continue;
            }


            // =================================================
            // Independent reference rolling window
            // =================================================

            reference_window.push_back(
                value);

            if (reference_window.size() >
                WINDOW_SIZE)
            {
                reference_window.pop_front();
            }


            // =================================================
            // Independent calculations
            // =================================================

            const double expected_mean =
                referenceMean(
                    reference_window);

            const double expected_stddev =
                referencePopulationStdDev(
                    reference_window);

            const double expected_zscore =
                referenceZScore(
                    value,
                    reference_window);

            const double expected_median =
                referenceMedian(
                    reference_window);

            const double expected_mad =
                referenceMAD(
                    reference_window);

            const double
                expected_robust_zscore =
                    referenceRobustZScore(
                        value,
                        reference_window);

            const bool expected_ready =
                reference_window.size() ==
                WINDOW_SIZE;


            // =================================================
            // Phase 4.1 calculation
            // =================================================

            try
            {
                const auto features =
                    statistical_engine.update(
                        decision_time,
                        value);

                ++report.observations;

                if (features.ready)
                {
                    ++report.ready_observations;
                }


                // =============================================
                // Metadata integrity
                // =============================================

                if (features.symbol != symbol)
                {
                    ++report.
                        decision_time_violations;
                }

                if (features.decision_time !=
                    decision_time)
                {
                    ++report.
                        decision_time_violations;
                }

                if (features.observation_count !=
                    reference_window.size())
                {
                    ++report.
                        readiness_mismatches;
                }

                if (features.window_size !=
                    WINDOW_SIZE)
                {
                    ++report.
                        readiness_mismatches;
                }


                // =============================================
                // Rolling Mean
                // =============================================

                if (!approximatelyEqual(
                        features.rolling_mean,
                        expected_mean))
                {
                    ++report.mean_mismatches;
                }


                // =============================================
                // Population Standard Deviation
                // =============================================

                if (!approximatelyEqual(
                        features.
                            rolling_standard_deviation,
                        expected_stddev))
                {
                    ++report.stddev_mismatches;
                }


                // =============================================
                // Standard Z-Score
                // =============================================

                if (!approximatelyEqual(
                        features.z_score,
                        expected_zscore))
                {
                    ++report.zscore_mismatches;
                }


                // =============================================
                // Median
                // =============================================

                if (!approximatelyEqual(
                        features.rolling_median,
                        expected_median))
                {
                    ++report.median_mismatches;
                }


                // =============================================
                // MAD
                // =============================================

                if (!approximatelyEqual(
                        features.rolling_mad,
                        expected_mad))
                {
                    ++report.mad_mismatches;
                }


                // =============================================
                // Robust Z-Score
                // =============================================

                if (!approximatelyEqual(
                        features.robust_z_score,
                        expected_robust_zscore))
                {
                    ++report.
                        robust_zscore_mismatches;
                }


                // =============================================
                // Readiness
                // =============================================

                if (features.ready !=
                    expected_ready)
                {
                    ++report.
                        readiness_mismatches;
                }


                // =============================================
                // Numeric integrity
                // =============================================

                if (!std::isfinite(
                        features.value) ||
                    !std::isfinite(
                        features.rolling_mean) ||
                    !std::isfinite(
                        features.
                            rolling_standard_deviation) ||
                    !std::isfinite(
                        features.z_score) ||
                    !std::isfinite(
                        features.rolling_median) ||
                    !std::isfinite(
                        features.rolling_mad) ||
                    !std::isfinite(
                        features.robust_z_score))
                {
                    ++report.
                        invalid_numeric_values;
                }


                // =============================================
                // Update validation state only after successful
                // Phase 4.1 processing.
                // =============================================

                previous_observation_count =
                    statistical_engine.
                        observationCount();

                previous_engine_decision_time =
                    decision_time;

                have_previous_engine_decision_time =
                    true;

                previous_observed_candle_timestamp =
                    candle.timestamp;

                have_previous_observed_candle =
                    true;
            }
            catch (const std::exception&)
            {
                ++report.
                    statistical_exceptions;
            }
        }


        // ====================================================
        // Runtime report
        // ====================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "RUNTIME SUMMARY\n"
            << "------------------------------------------------------------\n"
            << "Decision points                    : "
            << report.decision_points
            << "\n"
            << "Active-session points              : "
            << report.active_session_points
            << "\n"
            << "Snapshots with 1m                  : "
            << report.snapshots_with_1m
            << "\n"
            << "Statistical observations           : "
            << report.observations
            << "\n"
            << "Ready observations                 : "
            << report.ready_observations
            << "\n"
            << "New sessions                       : "
            << report.new_sessions
            << "\n";


        // ====================================================
        // Statistical report
        // ====================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "STATISTICAL VALIDATION\n"
            << "------------------------------------------------------------\n"
            << "Rolling Mean mismatches            : "
            << report.mean_mismatches
            << "\n"
            << "Standard Deviation mismatches      : "
            << report.stddev_mismatches
            << "\n"
            << "Z-Score mismatches                 : "
            << report.zscore_mismatches
            << "\n"
            << "Median mismatches                  : "
            << report.median_mismatches
            << "\n"
            << "MAD mismatches                     : "
            << report.mad_mismatches
            << "\n"
            << "Robust Z-Score mismatches          : "
            << report.robust_zscore_mismatches
            << "\n"
            << "Readiness mismatches               : "
            << report.readiness_mismatches
            << "\n";


        // ====================================================
        // Safety / integrity report
        // ====================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "SAFETY / INTEGRITY VALIDATION\n"
            << "------------------------------------------------------------\n"
            << "Look-ahead violations              : "
            << report.lookahead_violations
            << "\n"
            << "Decision-time violations           : "
            << report.decision_time_violations
            << "\n"
            << "Cross-session continuity violations: "
            << report.
                cross_session_continuity_violations
            << "\n"
            << "Invalid numeric values             : "
            << report.invalid_numeric_values
            << "\n"
            << "Statistical exceptions             : "
            << report.statistical_exceptions
            << "\n";


        // ====================================================
        // Final decision
        // ====================================================

        const bool passed =
            report.observations > 0 &&

            report.ready_observations > 0 &&

            report.mean_mismatches == 0 &&

            report.stddev_mismatches == 0 &&

            report.zscore_mismatches == 0 &&

            report.median_mismatches == 0 &&

            report.mad_mismatches == 0 &&

            report.robust_zscore_mismatches == 0 &&

            report.readiness_mismatches == 0 &&

            report.lookahead_violations == 0 &&

            report.decision_time_violations == 0 &&

            report.
                cross_session_continuity_violations == 0 &&

            report.invalid_numeric_values == 0 &&

            report.statistical_exceptions == 0;


        std::cout
            << "\n"
            << "============================================================\n";


        if (!passed)
        {
            std::cout
                << "PHASE 4.1 REAL STATISTICAL VALIDATION FAILED\n"
                << "============================================================\n";

            return EXIT_FAILURE;
        }


        std::cout
            << "PHASE 4.1 REAL STATISTICAL VALIDATION PASSED\n"
            << "============================================================\n"
            << "Rolling Mean                  : PASSED\n"
            << "Rolling Standard Deviation    : PASSED\n"
            << "Z-Score                       : PASSED\n"
            << "Median                        : PASSED\n"
            << "MAD                           : PASSED\n"
            << "Robust Z-Score                : PASSED\n"
            << "Window Readiness              : PASSED\n"
            << "Cross-Session Continuity      : PASSED\n"
            << "Look-Ahead Protection         : PASSED\n"
            << "Decision-Time Integrity       : PASSED\n"
            << "Numeric Integrity             : PASSED\n"
            << "============================================================\n";

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "PHASE 4.1 REAL STATISTICAL VALIDATION FAILED\n"
            << "============================================================\n"
            << exception.what()
            << "\n"
            << "============================================================\n";

        return EXIT_FAILURE;
    }
}