#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/CandleAggregator.hpp"
#include "devai/market/Timeframe.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/SessionState.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"

#include "devai/features/PriceReturnFeatureEngine.hpp"

#include "devai/statistics/ReturnStatisticalEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr std::size_t WINDOW_SIZE = 60;

constexpr double EXTREME_Z_THRESHOLD = 2.0;

constexpr double ABSOLUTE_TOLERANCE = 1.0e-9;
constexpr double RELATIVE_TOLERANCE = 1.0e-10;

constexpr double ROBUST_Z_CONSTANT =
    0.6744897501960817;


// ============================================================
// Floating-point comparison
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
// Independent reference mean
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
        sum +=
            static_cast<long double>(value);
    }

    return static_cast<double>(
        sum /
        static_cast<long double>(
            values.size()));
}


// ============================================================
// Independent population standard deviation
// ============================================================

double referencePopulationStdDev(
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
        sum +=
            static_cast<long double>(value);
    }

    const long double mean =
        sum /
        static_cast<long double>(
            values.size());

    long double squared_deviation_sum =
        0.0L;

    for (const double value : values)
    {
        const long double difference =
            static_cast<long double>(value) -
            mean;

        squared_deviation_sum +=
            difference * difference;
    }

    const long double variance =
        squared_deviation_sum /
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
// Independent Z-score
// ============================================================

double referenceZScore(
    double value,
    const std::deque<double>& values)
{
    const double mean =
        referenceMean(values);

    const double stddev =
        referencePopulationStdDev(values);

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
// Reference window
// ============================================================

struct ReferenceWindow
{
    std::deque<double> returns;

    std::deque<double> absolute_returns;

    std::optional<std::int64_t>
        last_source_timestamp;

    std::size_t observations{0};

    std::size_t ready_observations{0};


    void add(
        std::int64_t source_timestamp,
        double value)
    {
        if (last_source_timestamp.has_value())
        {
            if (source_timestamp <
                *last_source_timestamp)
            {
                throw std::runtime_error(
                    "Reference source timestamp moved backwards.");
            }

            if (source_timestamp ==
                *last_source_timestamp)
            {
                return;
            }
        }

        returns.push_back(value);

        absolute_returns.push_back(
            std::abs(value));

        if (returns.size() >
            WINDOW_SIZE)
        {
            returns.pop_front();
        }

        if (absolute_returns.size() >
            WINDOW_SIZE)
        {
            absolute_returns.pop_front();
        }

        last_source_timestamp =
            source_timestamp;

        ++observations;

        if (returns.size() ==
            WINDOW_SIZE)
        {
            ++ready_observations;
        }
    }


    [[nodiscard]]
    bool ready() const noexcept
    {
        return returns.size() ==
               WINDOW_SIZE;
    }
};


// ============================================================
// Per-timeframe validation counters
// ============================================================

struct HorizonValidation
{
    std::size_t expected_observations{0};

    std::size_t engine_observations{0};

    std::size_t expected_ready_observations{0};

    std::size_t duplicate_source_events{0};

    std::size_t observation_count_mismatches{0};

    std::size_t readiness_mismatches{0};

    std::size_t return_value_mismatches{0};

    std::size_t mean_mismatches{0};

    std::size_t stddev_mismatches{0};

    std::size_t zscore_mismatches{0};

    std::size_t median_mismatches{0};

    std::size_t mad_mismatches{0};

    std::size_t robust_zscore_mismatches{0};

    std::size_t absolute_return_mismatches{0};

    std::size_t absolute_mean_mismatches{0};

    std::size_t absolute_stddev_mismatches{0};

    std::size_t absolute_zscore_mismatches{0};

    std::size_t absolute_median_mismatches{0};

    std::size_t absolute_mad_mismatches{0};

    std::size_t
        absolute_robust_zscore_mismatches{0};
};


// ============================================================
// Overall report
// ============================================================

struct ValidationReport
{
    std::size_t decision_points{0};

    std::size_t active_session_points{0};

    std::size_t new_sessions{0};

    std::size_t lookahead_violations{0};

    std::size_t decision_time_violations{0};

    std::size_t
        cross_session_continuity_violations{0};

    std::size_t invalid_numeric_values{0};

    std::size_t statistical_exceptions{0};

    HorizonValidation one_minute;

    HorizonValidation five_minute;

    HorizonValidation fifteen_minute;
};


// ============================================================
// Validate one engine horizon against independent reference
// ============================================================

void validateHorizon(
    const devai::statistics::ReturnHorizonStatistics&
        actual,
    const ReferenceWindow& reference,
    HorizonValidation& report)
{
    report.expected_observations =
        reference.observations;

    report.engine_observations =
        actual.observation_count;

    report.expected_ready_observations =
        reference.ready_observations;


    if (actual.observation_count !=
        reference.returns.size() &&
        reference.observations <
            WINDOW_SIZE)
    {
        ++report.observation_count_mismatches;
    }

    if (reference.observations >=
        WINDOW_SIZE)
    {
        if (actual.observation_count !=
            WINDOW_SIZE)
        {
            ++report.observation_count_mismatches;
        }
    }


    if (actual.ready !=
        reference.ready())
    {
        ++report.readiness_mismatches;
    }


    if (reference.returns.empty())
    {
        return;
    }


    const double current_return =
        reference.returns.back();

    const double current_absolute_return =
        reference.absolute_returns.back();


    // --------------------------------------------------------
    // Return value
    // --------------------------------------------------------

    if (!approximatelyEqual(
            actual.return_value,
            current_return))
    {
        ++report.return_value_mismatches;
    }


    // --------------------------------------------------------
    // Ordinary return statistics
    // --------------------------------------------------------

    const double expected_mean =
        referenceMean(
            reference.returns);

    const double expected_stddev =
        referencePopulationStdDev(
            reference.returns);

    const double expected_zscore =
        referenceZScore(
            current_return,
            reference.returns);

    const double expected_median =
        referenceMedian(
            reference.returns);

    const double expected_mad =
        referenceMAD(
            reference.returns);

    const double expected_robust_zscore =
        referenceRobustZScore(
            current_return,
            reference.returns);


    if (!approximatelyEqual(
            actual.rolling_mean,
            expected_mean))
    {
        ++report.mean_mismatches;
    }

    if (!approximatelyEqual(
            actual.rolling_standard_deviation,
            expected_stddev))
    {
        ++report.stddev_mismatches;
    }

    if (!approximatelyEqual(
            actual.z_score,
            expected_zscore))
    {
        ++report.zscore_mismatches;
    }

    if (!approximatelyEqual(
            actual.rolling_median,
            expected_median))
    {
        ++report.median_mismatches;
    }

    if (!approximatelyEqual(
            actual.rolling_mad,
            expected_mad))
    {
        ++report.mad_mismatches;
    }

    if (!approximatelyEqual(
            actual.robust_z_score,
            expected_robust_zscore))
    {
        ++report.robust_zscore_mismatches;
    }


    // --------------------------------------------------------
    // Absolute return statistics
    // --------------------------------------------------------

    const double expected_absolute_mean =
        referenceMean(
            reference.absolute_returns);

    const double expected_absolute_stddev =
        referencePopulationStdDev(
            reference.absolute_returns);

    const double expected_absolute_zscore =
        referenceZScore(
            current_absolute_return,
            reference.absolute_returns);

    const double expected_absolute_median =
        referenceMedian(
            reference.absolute_returns);

    const double expected_absolute_mad =
        referenceMAD(
            reference.absolute_returns);

    const double
        expected_absolute_robust_zscore =
            referenceRobustZScore(
                current_absolute_return,
                reference.absolute_returns);


    if (!approximatelyEqual(
            actual.absolute_return,
            current_absolute_return))
    {
        ++report.absolute_return_mismatches;
    }

    if (!approximatelyEqual(
            actual.absolute_return_mean,
            expected_absolute_mean))
    {
        ++report.absolute_mean_mismatches;
    }

    if (!approximatelyEqual(
            actual.absolute_return_standard_deviation,
            expected_absolute_stddev))
    {
        ++report.absolute_stddev_mismatches;
    }

    if (!approximatelyEqual(
            actual.absolute_return_z_score,
            expected_absolute_zscore))
    {
        ++report.absolute_zscore_mismatches;
    }

    if (!approximatelyEqual(
            actual.absolute_return_median,
            expected_absolute_median))
    {
        ++report.absolute_median_mismatches;
    }

    if (!approximatelyEqual(
            actual.absolute_return_mad,
            expected_absolute_mad))
    {
        ++report.absolute_mad_mismatches;
    }

    if (!approximatelyEqual(
            actual.absolute_return_robust_z_score,
            expected_absolute_robust_zscore))
    {
        ++report.
            absolute_robust_zscore_mismatches;
    }
}


// ============================================================
// Horizon pass/fail
// ============================================================

bool horizonPassed(
    const HorizonValidation& report)
{
    return
        report.expected_observations > 0 &&

        report.engine_observations > 0 &&

        report.observation_count_mismatches == 0 &&

        report.readiness_mismatches == 0 &&

        report.return_value_mismatches == 0 &&

        report.mean_mismatches == 0 &&

        report.stddev_mismatches == 0 &&

        report.zscore_mismatches == 0 &&

        report.median_mismatches == 0 &&

        report.mad_mismatches == 0 &&

        report.robust_zscore_mismatches == 0 &&

        report.absolute_return_mismatches == 0 &&

        report.absolute_mean_mismatches == 0 &&

        report.absolute_stddev_mismatches == 0 &&

        report.absolute_zscore_mismatches == 0 &&

        report.absolute_median_mismatches == 0 &&

        report.absolute_mad_mismatches == 0 &&

        report.
            absolute_robust_zscore_mismatches == 0;
}


// ============================================================
// Print horizon report
// ============================================================

void printHorizonReport(
    const std::string& name,
    const HorizonValidation& report)
{
    std::cout
        << "\n"
        << "------------------------------------------------------------\n"
        << name
        << " RETURN STATISTICS\n"
        << "------------------------------------------------------------\n"
        << "Expected source observations       : "
        << report.expected_observations
        << "\n"
        << "Engine rolling observation count   : "
        << report.engine_observations
        << "\n"
        << "Expected ready observations        : "
        << report.expected_ready_observations
        << "\n"
        << "Repeated source snapshots ignored  : "
        << report.duplicate_source_events
        << "\n"
        << "Observation-count mismatches       : "
        << report.observation_count_mismatches
        << "\n"
        << "Readiness mismatches               : "
        << report.readiness_mismatches
        << "\n"
        << "Return-value mismatches            : "
        << report.return_value_mismatches
        << "\n"
        << "Mean mismatches                    : "
        << report.mean_mismatches
        << "\n"
        << "StdDev mismatches                  : "
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
        << "Absolute-return mismatches         : "
        << report.absolute_return_mismatches
        << "\n"
        << "Absolute mean mismatches           : "
        << report.absolute_mean_mismatches
        << "\n"
        << "Absolute StdDev mismatches         : "
        << report.absolute_stddev_mismatches
        << "\n"
        << "Absolute Z-Score mismatches        : "
        << report.absolute_zscore_mismatches
        << "\n"
        << "Absolute median mismatches         : "
        << report.absolute_median_mismatches
        << "\n"
        << "Absolute MAD mismatches            : "
        << report.absolute_mad_mismatches
        << "\n"
        << "Absolute robust-Z mismatches       : "
        << report.absolute_robust_zscore_mismatches
        << "\n";
}

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
            << "PHASE 4.2 — REAL RETURN STATISTICAL VALIDATION\n"
            << "============================================================\n"
            << "File        : "
            << data_file
            << "\n"
            << "Window size : "
            << WINDOW_SIZE
            << "\n"
            << "Extreme Z   : "
            << EXTREME_Z_THRESHOLD
            << "\n"
            << "============================================================\n";


        // ====================================================
        // Load real 1m market data
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
        // Aggregate 5m + 15m
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
        // Synchronizer
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
        // Runtime
        // ====================================================

        devai::market::MultiTimeframeCursor
            cursor(
                one_minute_timed,
                five_minute_timed,
                fifteen_minute_timed);


        devai::market::SessionState
            session_state;


        devai::market::MarketSnapshotBuilder
            snapshot_builder(symbol);


        // ====================================================
        // Phase 3.1
        // ====================================================

        devai::features::
            PriceReturnFeatureEngine
                price_return_engine(
                    symbol);


        // ====================================================
        // Phase 4.2
        // ====================================================

        devai::statistics::
            ReturnStatisticalEngine
                return_statistics_engine(
                    symbol,
                    WINDOW_SIZE,
                    EXTREME_Z_THRESHOLD);


        // ====================================================
        // Independent references
        // ====================================================

        ReferenceWindow reference_1m;

        ReferenceWindow reference_5m;

        ReferenceWindow reference_15m;


        ValidationReport report;


        std::optional<std::size_t>
            previous_1m_count;

        std::optional<std::size_t>
            previous_5m_count;

        std::optional<std::size_t>
            previous_15m_count;


        // ====================================================
        // Replay
        // ====================================================

        for (const auto& source_candle :
             load_result.candles)
        {
            const std::int64_t decision_time =
                source_candle.timestamp + 60;

            ++report.decision_points;


            const auto snapshot =
                snapshot_builder.build(
                    decision_time,
                    cursor,
                    session_state);


            if (snapshot.new_session)
            {
                ++report.new_sessions;
            }

            if (snapshot.session_phase !=
                devai::market::SessionPhase::
                    ACTIVE)
            {
                continue;
            }


            ++report.active_session_points;


            // =================================================
            // Phase 3.1
            // =================================================

            const auto price_features =
                price_return_engine.update(
                    snapshot);


            // =================================================
            // Capture source timestamps BEFORE Phase 4.2
            // =================================================

            std::optional<std::int64_t> ts_1m;
            std::optional<std::int64_t> ts_5m;
            std::optional<std::int64_t> ts_15m;


            if (snapshot.one_minute.has_value())
            {
                ts_1m =
                    snapshot.one_minute->timestamp;
            }


            if (snapshot.five_minute.has_value())
            {
                ts_5m =
                    snapshot.five_minute->timestamp;
            }


            if (snapshot.fifteen_minute.has_value())
            {
                ts_15m =
                    snapshot.fifteen_minute->timestamp;
            }


            // =================================================
            // Determine duplicate source snapshots
            // =================================================

            if (ts_1m.has_value() &&
                reference_1m.last_source_timestamp.has_value() &&
                *ts_1m ==
                    *reference_1m.last_source_timestamp)
            {
                ++report.one_minute.
                    duplicate_source_events;
            }


            if (ts_5m.has_value() &&
                reference_5m.last_source_timestamp.has_value() &&
                *ts_5m ==
                    *reference_5m.last_source_timestamp)
            {
                ++report.five_minute.
                    duplicate_source_events;
            }


            if (ts_15m.has_value() &&
                reference_15m.last_source_timestamp.has_value() &&
                *ts_15m ==
                    *reference_15m.last_source_timestamp)
            {
                ++report.fifteen_minute.
                    duplicate_source_events;
            }


            // =================================================
            // Independent reference observations
            //
            // Only add a return when:
            //
            // 1. Phase 3.1 says the timeframe exists.
            // 2. Return is finite.
            // 3. Source candle timestamp is available.
            // 4. Source candle is genuinely new.
            // =================================================

            if (price_features.has_one_minute &&
                std::isfinite(
                    price_features.one_minute_return) &&
                ts_1m.has_value())
            {
                reference_1m.add(
                    *ts_1m,
                    price_features.one_minute_return);
            }


            if (price_features.has_five_minute &&
                std::isfinite(
                    price_features.five_minute_return) &&
                ts_5m.has_value())
            {
                reference_5m.add(
                    *ts_5m,
                    price_features.five_minute_return);
            }


            if (price_features.has_fifteen_minute &&
                std::isfinite(
                    price_features.fifteen_minute_return) &&
                ts_15m.has_value())
            {
                reference_15m.add(
                    *ts_15m,
                    price_features.fifteen_minute_return);
            }


            // =================================================
            // Phase 4.2
            // =================================================

            try
            {
                const auto actual =
                    return_statistics_engine.update(
                        price_features,
                        snapshot);


                // =============================================
                // Decision-time integrity
                // =============================================

                if (actual.decision_time !=
                    decision_time)
                {
                    ++report.
                        decision_time_violations;
                }


                if (actual.symbol != symbol)
                {
                    ++report.
                        decision_time_violations;
                }


                // =============================================
                // Cross-session continuity
                //
                // No statistical history may reset merely
                // because snapshot.new_session is true.
                // =============================================

                if (snapshot.new_session)
                {
                    if (previous_1m_count.has_value() &&
                        *previous_1m_count > 0 &&
                        actual.one_minute.
                            observation_count == 0)
                    {
                        ++report.
                            cross_session_continuity_violations;
                    }

                    if (previous_5m_count.has_value() &&
                        *previous_5m_count > 0 &&
                        actual.five_minute.
                            observation_count == 0)
                    {
                        ++report.
                            cross_session_continuity_violations;
                    }

                    if (previous_15m_count.has_value() &&
                        *previous_15m_count > 0 &&
                        actual.fifteen_minute.
                            observation_count == 0)
                    {
                        ++report.
                            cross_session_continuity_violations;
                    }
                }


                // =============================================
                // Independent comparison
                // =============================================

                validateHorizon(
                    actual.one_minute,
                    reference_1m,
                    report.one_minute);


                validateHorizon(
                    actual.five_minute,
                    reference_5m,
                    report.five_minute);


                validateHorizon(
                    actual.fifteen_minute,
                    reference_15m,
                    report.fifteen_minute);


                // =============================================
                // Numeric integrity once observation exists
                // =============================================

                const auto check_numeric =
                    [&](const auto& horizon)
                    {
                        if (!horizon.has_observation)
                        {
                            return;
                        }

                        if (!std::isfinite(
                                horizon.return_value) ||
                            !std::isfinite(
                                horizon.absolute_return) ||
                            !std::isfinite(
                                horizon.rolling_mean) ||
                            !std::isfinite(
                                horizon.
                                    rolling_standard_deviation) ||
                            !std::isfinite(
                                horizon.z_score) ||
                            !std::isfinite(
                                horizon.rolling_median) ||
                            !std::isfinite(
                                horizon.rolling_mad) ||
                            !std::isfinite(
                                horizon.robust_z_score) ||
                            !std::isfinite(
                                horizon.absolute_return_mean) ||
                            !std::isfinite(
                                horizon.
                                    absolute_return_standard_deviation) ||
                            !std::isfinite(
                                horizon.absolute_return_z_score) ||
                            !std::isfinite(
                                horizon.absolute_return_median) ||
                            !std::isfinite(
                                horizon.absolute_return_mad) ||
                            !std::isfinite(
                                horizon.
                                    absolute_return_robust_z_score))
                        {
                            ++report.
                                invalid_numeric_values;
                        }
                    };


                check_numeric(
                    actual.one_minute);

                check_numeric(
                    actual.five_minute);

                check_numeric(
                    actual.fifteen_minute);


                previous_1m_count =
                    actual.one_minute.
                        observation_count;

                previous_5m_count =
                    actual.five_minute.
                        observation_count;

                previous_15m_count =
                    actual.fifteen_minute.
                        observation_count;
            }
            catch (const std::exception&)
            {
                ++report.
                    statistical_exceptions;
            }


            // =================================================
            // Look-ahead protection
            // =================================================

            if (snapshot.one_minute.has_value())
            {
                if (snapshot.one_minute->timestamp +
                        60 >
                    decision_time)
                {
                    ++report.lookahead_violations;
                }
            }


            if (snapshot.five_minute.has_value())
            {
                if (snapshot.five_minute->timestamp +
                        300 >
                    decision_time)
                {
                    ++report.lookahead_violations;
                }
            }


            if (snapshot.fifteen_minute.has_value())
            {
                if (snapshot.fifteen_minute->timestamp +
                        900 >
                    decision_time)
                {
                    ++report.lookahead_violations;
                }
            }
        }


        // ====================================================
        // Final horizon reports
        // ====================================================

        printHorizonReport(
            "1-MINUTE",
            report.one_minute);

        printHorizonReport(
            "5-MINUTE",
            report.five_minute);

        printHorizonReport(
            "15-MINUTE",
            report.fifteen_minute);


        // ====================================================
        // Runtime integrity
        // ====================================================

        std::cout
            << "\n"
            << "------------------------------------------------------------\n"
            << "RUNTIME / SAFETY VALIDATION\n"
            << "------------------------------------------------------------\n"
            << "Decision points                    : "
            << report.decision_points
            << "\n"
            << "Active-session points              : "
            << report.active_session_points
            << "\n"
            << "New sessions                       : "
            << report.new_sessions
            << "\n"
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
            horizonPassed(
                report.one_minute) &&

            horizonPassed(
                report.five_minute) &&

            horizonPassed(
                report.fifteen_minute) &&

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
                << "PHASE 4.2 REAL RETURN STATISTICAL VALIDATION FAILED\n"
                << "============================================================\n";

            return EXIT_FAILURE;
        }


        std::cout
            << "PHASE 4.2 REAL RETURN STATISTICAL VALIDATION PASSED\n"
            << "============================================================\n"
            << "1m Return Statistics          : PASSED\n"
            << "5m Return Statistics          : PASSED\n"
            << "15m Return Statistics         : PASSED\n"
            << "Absolute Return Statistics    : PASSED\n"
            << "Repeated Candle Protection    : PASSED\n"
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
            << "PHASE 4.2 REAL RETURN STATISTICAL VALIDATION FAILED\n"
            << "============================================================\n"
            << exception.what()
            << "\n"
            << "============================================================\n";

        return EXIT_FAILURE;
    }
}