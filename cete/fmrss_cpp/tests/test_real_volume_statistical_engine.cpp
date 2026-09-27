#include "devai/features/VolumeFeatureEngine.hpp"

#include "devai/market/CandleAggregator.hpp"
#include "devai/market/MarketDataLoader.hpp"
#include "devai/market/MarketSnapshotBuilder.hpp"
#include "devai/market/MultiTimeframeCursor.hpp"
#include "devai/market/MultiTimeframeSynchronizer.hpp"
#include "devai/market/SessionState.hpp"

#include "devai/statistics/VolumeStatisticalEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

// ============================================================================
// Configuration
// ============================================================================

constexpr std::size_t STATISTICAL_WINDOW = 60;

constexpr std::size_t RVOL20_PERIOD = 20;
constexpr std::size_t RVOL50_PERIOD = 50;

constexpr double LOW_Z_THRESHOLD = -1.0;
constexpr double HIGH_Z_THRESHOLD = 1.0;
constexpr double EXTREME_Z_THRESHOLD = 2.0;

constexpr double ROBUST_Z_CONSTANT =
    0.6744897501960817;

constexpr double EPSILON =
    1.0e-8;


// ============================================================================
// Approximate comparison
// ============================================================================

bool approximatelyEqual(
    double lhs,
    double rhs,
    double tolerance = EPSILON)
{
    if (std::isnan(lhs) &&
        std::isnan(rhs))
    {
        return true;
    }

    if (!std::isfinite(lhs) ||
        !std::isfinite(rhs))
    {
        return lhs == rhs;
    }

    const double scale =
        std::max(
            1.0,
            std::max(
                std::fabs(lhs),
                std::fabs(rhs)));

    return
        std::fabs(lhs - rhs) <=
        tolerance * scale;
}


// ============================================================================
// Independent rolling mean
// ============================================================================

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


// ============================================================================
// Independent population standard deviation
//
// Centered calculation is deliberately used here.
//
// This matches the numerically stable semantics frozen in Phase 4.1.
// ============================================================================

double referencePopulationStdDev(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    const long double mean =
        static_cast<long double>(
            referenceMean(values));

    long double sum_squared_deviation =
        0.0L;

    for (const double value : values)
    {
        const long double difference =
            static_cast<long double>(value) -
            mean;

        sum_squared_deviation +=
            difference * difference;
    }

    long double variance =
        sum_squared_deviation /
        static_cast<long double>(
            values.size());

    if (variance < 0.0L &&
        std::fabs(variance) < 1.0e-18L)
    {
        variance = 0.0L;
    }

    if (variance < 0.0L)
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    return std::sqrt(
        static_cast<double>(variance));
}


// ============================================================================
// Independent median
// ============================================================================

double referenceMedian(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    std::vector<double> sorted(
        values.begin(),
        values.end());

    std::sort(
        sorted.begin(),
        sorted.end());

    const std::size_t size =
        sorted.size();

    const std::size_t middle =
        size / 2;

    if ((size % 2) != 0)
    {
        return sorted[middle];
    }

    return
        (sorted[middle - 1] +
         sorted[middle]) /
        2.0;
}


// ============================================================================
// Independent MAD
// ============================================================================

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
            std::fabs(
                value - median));
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


// ============================================================================
// Independent Z-score
// ============================================================================

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


// ============================================================================
// Independent robust Z-score
// ============================================================================

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


// ============================================================================
// Bounded window append
// ============================================================================

void appendWindow(
    std::deque<double>& window,
    double value,
    std::size_t maximum_size)
{
    window.push_back(
        value);

    while (window.size() >
           maximum_size)
    {
        window.pop_front();
    }
}


// ============================================================================
// Independent rolling average
//
// Used to independently reproduce Phase 3.3 RVOL.
// ============================================================================

double referenceRollingAverage(
    const std::deque<double>& values,
    std::size_t period)
{
    if (period == 0 ||
        values.size() < period)
    {
        return std::numeric_limits<double>::
            quiet_NaN();
    }

    long double sum = 0.0L;

    const std::size_t start =
        values.size() - period;

    for (std::size_t index = start;
         index < values.size();
         ++index)
    {
        sum +=
            static_cast<long double>(
                values[index]);
    }

    return static_cast<double>(
        sum /
        static_cast<long double>(
            period));
}


// ============================================================================
// Reference abnormal-volume classification
//
// This matches the CURRENT Phase 4.3 implementation:
//
// classification is available when a volume observation and finite Z-score
// exist. It is not additionally gated by `ready`.
// ============================================================================

devai::statistics::AbnormalVolumeState
referenceAbnormalVolumeState(
    bool has_observation,
    double z_score)
{
    using State =
        devai::statistics::AbnormalVolumeState;

    if (!has_observation ||
        !std::isfinite(z_score))
    {
        return State::UNAVAILABLE;
    }

    if (z_score >=
        EXTREME_Z_THRESHOLD)
    {
        return State::EXTREME_HIGH;
    }

    if (z_score >=
        HIGH_Z_THRESHOLD)
    {
        return State::HIGH;
    }

    if (z_score <=
        -EXTREME_Z_THRESHOLD)
    {
        return State::VERY_LOW;
    }

    if (z_score <=
        LOW_Z_THRESHOLD)
    {
        return State::LOW;
    }

    return State::NORMAL;
}


// ============================================================================
// Independent metric state
// ============================================================================

struct ReferenceMetric
{
    std::deque<double> window;

    std::size_t total_observations{0};

    bool has_latest{false};

    double latest{
        std::numeric_limits<double>::
            quiet_NaN()
    };


    void add(
        double value)
    {
        appendWindow(
            window,
            value,
            STATISTICAL_WINDOW);

        ++total_observations;

        latest =
            value;

        has_latest =
            true;
    }


    [[nodiscard]]
    bool ready() const noexcept
    {
        return
            window.size() >=
            STATISTICAL_WINDOW;
    }
};


// ============================================================================
// Independent timeframe state
//
// raw_volume_history:
//     used only for independently constructing Phase 3.3 RVOL20/RVOL50.
//
// volume / rvol20 / rvol50:
//     independent Phase 4.3 statistical windows.
// ============================================================================

struct ReferenceHorizon
{
    std::optional<std::int64_t>
        last_source_timestamp;

    std::deque<double>
        raw_volume_history;

    ReferenceMetric
        volume;

    ReferenceMetric
        rvol20;

    ReferenceMetric
        rvol50;

    std::size_t
        repeated_sources{0};

    std::size_t
        source_observations{0};
};


// ============================================================================
// Per-metric mismatch counters
// ============================================================================

struct MetricValidation
{
    std::size_t
        observation_count_mismatches{0};

    std::size_t
        availability_mismatches{0};

    std::size_t
        readiness_mismatches{0};

    std::size_t
        value_mismatches{0};

    std::size_t
        mean_mismatches{0};

    std::size_t
        stddev_mismatches{0};

    std::size_t
        zscore_mismatches{0};

    std::size_t
        median_mismatches{0};

    std::size_t
        mad_mismatches{0};

    std::size_t
        robust_zscore_mismatches{0};


    [[nodiscard]]
    std::size_t totalMismatches() const noexcept
    {
        return
            observation_count_mismatches +
            availability_mismatches +
            readiness_mismatches +
            value_mismatches +
            mean_mismatches +
            stddev_mismatches +
            zscore_mismatches +
            median_mismatches +
            mad_mismatches +
            robust_zscore_mismatches;
    }
};


// ============================================================================
// Horizon validation report
// ============================================================================

struct HorizonValidation
{
    MetricValidation volume;
    MetricValidation rvol20;
    MetricValidation rvol50;

    std::size_t
        abnormal_state_mismatches{0};

    std::size_t
        fully_ready_mismatches{0};

    std::size_t
        source_observations{0};

    std::size_t
        repeated_sources{0};


    [[nodiscard]]
    std::size_t totalMismatches() const noexcept
    {
        return
            volume.totalMismatches() +
            rvol20.totalMismatches() +
            rvol50.totalMismatches() +
            abnormal_state_mismatches +
            fully_ready_mismatches;
    }
};


// ============================================================================
// Complete validation report
// ============================================================================

struct ValidationReport
{
    std::size_t
        decision_points{0};

    std::size_t
        active_session_points{0};

    std::size_t
        new_sessions{0};

    HorizonValidation
        one_minute;

    HorizonValidation
        five_minute;

    HorizonValidation
        fifteen_minute;

    std::size_t
        lookahead_violations{0};

    std::size_t
        decision_time_violations{0};

    std::size_t
        cross_session_continuity_violations{0};

    std::size_t
        invalid_numeric_values{0};

    std::size_t
        feature_exceptions{0};

    std::size_t
        statistical_exceptions{0};
};


// ============================================================================
// Validate one statistical metric
// ============================================================================

void validateMetric(
    const ReferenceMetric& reference,
    const devai::statistics::VolumeMetricStatistics& actual,
    MetricValidation& validation)
{
    const std::size_t expected_count =
        std::min(
            reference.total_observations,
            STATISTICAL_WINDOW);

    if (actual.observation_count !=
        expected_count)
    {
        ++validation.
            observation_count_mismatches;
    }


    if (actual.has_observation !=
        reference.has_latest)
    {
        ++validation.
            availability_mismatches;
    }


    const bool expected_ready =
        reference.ready();

    if (actual.ready !=
        expected_ready)
    {
        ++validation.
            readiness_mismatches;
    }


    if (!reference.has_latest)
    {
        return;
    }


    const double expected_value =
        reference.latest;

    const double expected_mean =
        referenceMean(
            reference.window);

    const double expected_stddev =
        referencePopulationStdDev(
            reference.window);

    const double expected_z =
        referenceZScore(
            expected_value,
            reference.window);

    const double expected_median =
        referenceMedian(
            reference.window);

    const double expected_mad =
        referenceMAD(
            reference.window);

    const double expected_robust_z =
        referenceRobustZScore(
            expected_value,
            reference.window);


    if (!approximatelyEqual(
            actual.value,
            expected_value))
    {
        ++validation.value_mismatches;
    }


    if (!approximatelyEqual(
            actual.rolling_mean,
            expected_mean))
    {
        ++validation.mean_mismatches;
    }


    if (!approximatelyEqual(
            actual.rolling_standard_deviation,
            expected_stddev))
    {
        ++validation.stddev_mismatches;
    }


    if (!approximatelyEqual(
            actual.z_score,
            expected_z))
    {
        ++validation.zscore_mismatches;
    }


    if (!approximatelyEqual(
            actual.rolling_median,
            expected_median))
    {
        ++validation.median_mismatches;
    }


    if (!approximatelyEqual(
            actual.rolling_mad,
            expected_mad))
    {
        ++validation.mad_mismatches;
    }


    if (!approximatelyEqual(
            actual.robust_z_score,
            expected_robust_z))
    {
        ++validation.
            robust_zscore_mismatches;
    }
}


// ============================================================================
// Consume one real source candle independently
//
// Returns true only when the source candle is genuinely new.
// ============================================================================

bool consumeReferenceHorizon(
    ReferenceHorizon& state,
    const std::optional<devai::market::Candle>& candle)
{
    if (!candle)
    {
        return false;
    }


    if (state.last_source_timestamp)
    {
        if (candle->timestamp ==
            *state.last_source_timestamp)
        {
            ++state.repeated_sources;

            return false;
        }


        if (candle->timestamp <
            *state.last_source_timestamp)
        {
            throw std::runtime_error(
                "Reference source timestamp moved backwards.");
        }
    }


    const double raw_volume =
        candle->volume;


    if (!std::isfinite(raw_volume) ||
        raw_volume < 0.0)
    {
        throw std::runtime_error(
            "Reference encountered invalid volume.");
    }


    // ------------------------------------------------------------------------
    // Phase 3.3 reference history
    // ------------------------------------------------------------------------

    appendWindow(
        state.raw_volume_history,
        raw_volume,
        RVOL50_PERIOD);


    // ------------------------------------------------------------------------
    // Phase 4.3 raw-volume observation
    // ------------------------------------------------------------------------

    state.volume.add(
        raw_volume);


    // ------------------------------------------------------------------------
    // Independent Phase 3.3 RVOL20
    // ------------------------------------------------------------------------

    const double average20 =
        referenceRollingAverage(
            state.raw_volume_history,
            RVOL20_PERIOD);


    if (std::isfinite(average20) &&
        average20 > 0.0)
    {
        const double rvol20 =
            raw_volume /
            average20;

        state.rvol20.add(
            rvol20);
    }


    // ------------------------------------------------------------------------
    // Independent Phase 3.3 RVOL50
    // ------------------------------------------------------------------------

    const double average50 =
        referenceRollingAverage(
            state.raw_volume_history,
            RVOL50_PERIOD);


    if (std::isfinite(average50) &&
        average50 > 0.0)
    {
        const double rvol50 =
            raw_volume /
            average50;

        state.rvol50.add(
            rvol50);
    }


    state.last_source_timestamp =
        candle->timestamp;

    ++state.source_observations;


    return true;
}


// ============================================================================
// Validate one horizon
// ============================================================================

void validateHorizon(
    const ReferenceHorizon& reference,
    const devai::statistics::VolumeHorizonStatistics& actual,
    HorizonValidation& validation)
{
    validateMetric(
        reference.volume,
        actual.volume,
        validation.volume);

    validateMetric(
        reference.rvol20,
        actual.rvol20,
        validation.rvol20);

    validateMetric(
        reference.rvol50,
        actual.rvol50,
        validation.rvol50);


    const bool expected_fully_ready =
        reference.volume.ready() &&
        reference.rvol20.ready() &&
        reference.rvol50.ready();


    if (actual.fully_ready !=
        expected_fully_ready)
    {
        ++validation.
            fully_ready_mismatches;
    }


    const auto expected_state =
        referenceAbnormalVolumeState(
            reference.volume.has_latest,
            reference.volume.has_latest
                ? referenceZScore(
                    reference.volume.latest,
                    reference.volume.window)
                : std::numeric_limits<double>::
                    quiet_NaN());


    if (actual.abnormal_volume_state !=
        expected_state)
    {
        ++validation.
            abnormal_state_mismatches;
    }
}


// ============================================================================
// Print metric validation
// ============================================================================

void printMetricValidation(
    const std::string& title,
    const ReferenceMetric& reference,
    const MetricValidation& validation)
{
    std::cout
        << "\n"
        << title
        << "\n"
        << "------------------------------------------------------------\n"
        << "Total source observations        : "
        << reference.total_observations
        << "\n"
        << "Current rolling observation count: "
        << std::min(
               reference.total_observations,
               STATISTICAL_WINDOW)
        << "\n"
        << "Observation-count mismatches     : "
        << validation.observation_count_mismatches
        << "\n"
        << "Availability mismatches          : "
        << validation.availability_mismatches
        << "\n"
        << "Readiness mismatches             : "
        << validation.readiness_mismatches
        << "\n"
        << "Value mismatches                 : "
        << validation.value_mismatches
        << "\n"
        << "Mean mismatches                  : "
        << validation.mean_mismatches
        << "\n"
        << "StdDev mismatches                : "
        << validation.stddev_mismatches
        << "\n"
        << "Z-Score mismatches               : "
        << validation.zscore_mismatches
        << "\n"
        << "Median mismatches                : "
        << validation.median_mismatches
        << "\n"
        << "MAD mismatches                   : "
        << validation.mad_mismatches
        << "\n"
        << "Robust Z-Score mismatches        : "
        << validation.robust_zscore_mismatches
        << "\n";
}


// ============================================================================
// Print one horizon
// ============================================================================

void printHorizonValidation(
    const std::string& horizon,
    const ReferenceHorizon& reference,
    const HorizonValidation& validation)
{
    std::cout
        << "\n"
        << "============================================================\n"
        << horizon
        << " VALIDATION\n"
        << "============================================================\n"
        << "Unique source candles           : "
        << reference.source_observations
        << "\n"
        << "Repeated source snapshots       : "
        << reference.repeated_sources
        << "\n";


    printMetricValidation(
        horizon + " RAW VOLUME",
        reference.volume,
        validation.volume);


    printMetricValidation(
        horizon + " RVOL20",
        reference.rvol20,
        validation.rvol20);


    printMetricValidation(
        horizon + " RVOL50",
        reference.rvol50,
        validation.rvol50);


    std::cout
        << "\n"
        << horizon
        << " HORIZON STATE\n"
        << "------------------------------------------------------------\n"
        << "Abnormal-state mismatches       : "
        << validation.abnormal_state_mismatches
        << "\n"
        << "Fully-ready mismatches          : "
        << validation.fully_ready_mismatches
        << "\n";
}


// ============================================================================
// Horizon pass helper
// ============================================================================

bool horizonPassed(
    const ReferenceHorizon& reference,
    const HorizonValidation& validation)
{
    return
        reference.volume.total_observations > 0 &&
        reference.rvol20.total_observations > 0 &&
        reference.rvol50.total_observations > 0 &&
        reference.volume.ready() &&
        reference.rvol20.ready() &&
        reference.rvol50.ready() &&
        validation.totalMismatches() == 0;
}

} // namespace


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
        // Command line
        // ====================================================================

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


        // ====================================================================
        // Header
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 4.3 — REAL VOLUME STATISTICAL VALIDATION\n"
            << "============================================================\n"
            << "File                : "
            << data_file
            << "\n"
            << "Statistical window  : "
            << STATISTICAL_WINDOW
            << "\n"
            << "Phase 3.3 RVOL20    : "
            << RVOL20_PERIOD
            << " candles\n"
            << "Phase 3.3 RVOL50    : "
            << RVOL50_PERIOD
            << " candles\n"
            << "Low Z threshold     : "
            << LOW_Z_THRESHOLD
            << "\n"
            << "High Z threshold    : "
            << HIGH_Z_THRESHOLD
            << "\n"
            << "Extreme Z threshold : "
            << EXTREME_Z_THRESHOLD
            << "\n"
            << "============================================================\n";


        // ====================================================================
        // Phase 1 — Load real 1-minute data
        // ====================================================================

        const std::filesystem::path data_directory =
            data_file.parent_path();


        const std::string filename =
            data_file.filename().string();


        devai::market::MarketDataLoader loader(
            data_directory);


        const auto load_result =
            loader.loadFile(
                filename);


        if (load_result.candles.empty())
        {
            throw std::runtime_error(
                "No valid 1-minute candles were loaded.");
        }


        const std::string symbol =
            load_result.candles.front().symbol;


        std::cout
            << "Symbol              : "
            << symbol
            << "\n"
            << "1m candles          : "
            << load_result.candles.size()
            << "\n"
            << "Invalid rows        : "
            << load_result.invalid_rows
            << "\n"
            << "Duplicate rows      : "
            << load_result.duplicate_rows
            << "\n"
            << "Skipped rows        : "
            << load_result.skipped_rows
            << "\n";


        // ====================================================================
        // Phase 1 — Aggregate 5m and 15m
        // ====================================================================

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
            << "5m candles          : "
            << five_minute_result.candles.size()
            << "\n"
            << "5m incomplete       : "
            << five_minute_result.incomplete_buckets
            << "\n"
            << "15m candles         : "
            << fifteen_minute_result.candles.size()
            << "\n"
            << "15m incomplete      : "
            << fifteen_minute_result.incomplete_buckets
            << "\n";


        // ====================================================================
        // Phase 1/2 — Legal availability streams
        // ====================================================================

        devai::market::
            MultiTimeframeSynchronizerConfig
                synchronizer_config;


        synchronizer_config.mode =
            devai::market::
                AvailabilityMode::ZERO_LATENCY;


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


        // ====================================================================
        // Phase 2 — Runtime
        // ====================================================================

        devai::market::MultiTimeframeCursor
            cursor(
                one_minute_timed,
                five_minute_timed,
                fifteen_minute_timed);


        devai::market::SessionState
            session_state;


        devai::market::MarketSnapshotBuilder
            snapshot_builder(
                symbol);


        // ====================================================================
        // Phase 3.3 — Actual production feature engine
        // ====================================================================

        devai::features::VolumeFeatureEngine
            volume_feature_engine(
                symbol);


        // ====================================================================
        // Phase 4.3 — Engine under validation
        // ====================================================================

        devai::statistics::VolumeStatisticalEngine
            statistical_engine(
                symbol,
                STATISTICAL_WINDOW,
                LOW_Z_THRESHOLD,
                HIGH_Z_THRESHOLD,
                EXTREME_Z_THRESHOLD);


        // ====================================================================
        // Independent reference states
        // ====================================================================

        ReferenceHorizon
            one_minute_reference;

        ReferenceHorizon
            five_minute_reference;

        ReferenceHorizon
            fifteen_minute_reference;


        ValidationReport report;


        std::optional<std::int64_t>
            previous_decision_time;


        std::size_t
            previous_one_minute_total = 0;

        std::size_t
            previous_five_minute_total = 0;

        std::size_t
            previous_fifteen_minute_total = 0;


        // ====================================================================
        // Real-market runtime replay
        //
        // Each source 1m candle is evaluated at completion:
        //
        //     09:15 candle -> decision time 09:16
        //
        // ====================================================================

        for (const auto& source_candle :
             load_result.candles)
        {
            const std::int64_t decision_time =
                source_candle.timestamp + 60;


            ++report.decision_points;


            // ----------------------------------------------------------------
            // Decision-time monotonicity
            // ----------------------------------------------------------------

            if (previous_decision_time &&
                decision_time <=
                    *previous_decision_time)
            {
                ++report.
                    decision_time_violations;

                continue;
            }


            // ----------------------------------------------------------------
            // Advance Phase-2 runtime
            // ----------------------------------------------------------------

            cursor.advanceTo(
                decision_time);


            const auto snapshot =
                snapshot_builder.build(
                    decision_time,
                    cursor,
                    session_state);


            if (snapshot.new_session)
            {
                ++report.new_sessions;


                // ------------------------------------------------------------
                // Phase 4.3 continuous histories must survive sessions.
                //
                // The reference observation totals must therefore never
                // spontaneously return to zero.
                // ------------------------------------------------------------

                if (previous_one_minute_total > 0 &&
                    one_minute_reference.
                        volume.total_observations == 0)
                {
                    ++report.
                        cross_session_continuity_violations;
                }


                if (previous_five_minute_total > 0 &&
                    five_minute_reference.
                        volume.total_observations == 0)
                {
                    ++report.
                        cross_session_continuity_violations;
                }


                if (previous_fifteen_minute_total > 0 &&
                    fifteen_minute_reference.
                        volume.total_observations == 0)
                {
                    ++report.
                        cross_session_continuity_violations;
                }
            }


            if (snapshot.session_phase !=
                devai::market::SessionPhase::ACTIVE)
            {
                previous_decision_time =
                    decision_time;

                continue;
            }


            ++report.active_session_points;


            // ----------------------------------------------------------------
            // Look-ahead validation
            // ----------------------------------------------------------------

            if (snapshot.one_minute)
            {
                if (snapshot.one_minute->timestamp +
                        60 >
                    decision_time)
                {
                    ++report.
                        lookahead_violations;
                }
            }


            if (snapshot.five_minute)
            {
                if (snapshot.five_minute->timestamp +
                        300 >
                    decision_time)
                {
                    ++report.
                        lookahead_violations;
                }
            }


            if (snapshot.fifteen_minute)
            {
                if (snapshot.fifteen_minute->timestamp +
                        900 >
                    decision_time)
                {
                    ++report.
                        lookahead_violations;
                }
            }


            // ----------------------------------------------------------------
            // Independent source consumption
            // ----------------------------------------------------------------

            try
            {
                consumeReferenceHorizon(
                    one_minute_reference,
                    snapshot.one_minute);


                consumeReferenceHorizon(
                    five_minute_reference,
                    snapshot.five_minute);


                consumeReferenceHorizon(
                    fifteen_minute_reference,
                    snapshot.fifteen_minute);
            }
            catch (const std::exception&)
            {
                ++report.
                    invalid_numeric_values;

                previous_decision_time =
                    decision_time;

                continue;
            }


            // ----------------------------------------------------------------
            // Actual Phase 3.3 production features
            // ----------------------------------------------------------------

            devai::features::VolumeFeatures
                volume_features;


            try
            {
                volume_features =
                    volume_feature_engine.update(
                        snapshot);
            }
            catch (const std::exception&)
            {
                ++report.feature_exceptions;

                previous_decision_time =
                    decision_time;

                continue;
            }


            // ----------------------------------------------------------------
            // Phase 4.3 production engine
            // ----------------------------------------------------------------

            devai::statistics::VolumeStatisticalFeatures
                result;


            try
            {
                result =
                    statistical_engine.update(
                        volume_features,
                        snapshot);
            }
            catch (const std::exception&)
            {
                ++report.
                    statistical_exceptions;

                previous_decision_time =
                    decision_time;

                continue;
            }


            // ----------------------------------------------------------------
            // Result identity
            // ----------------------------------------------------------------

            if (result.symbol !=
                    symbol ||
                result.decision_time !=
                    decision_time)
            {
                ++report.
                    decision_time_violations;
            }


            // ----------------------------------------------------------------
            // Validate all three horizons
            // ----------------------------------------------------------------

            validateHorizon(
                one_minute_reference,
                result.one_minute,
                report.one_minute);


            validateHorizon(
                five_minute_reference,
                result.five_minute,
                report.five_minute);


            validateHorizon(
                fifteen_minute_reference,
                result.fifteen_minute,
                report.fifteen_minute);


            // ----------------------------------------------------------------
            // Top-level readiness
            // ----------------------------------------------------------------

            if (result.one_minute_ready !=
                result.one_minute.fully_ready)
            {
                ++report.one_minute.
                    fully_ready_mismatches;
            }


            if (result.five_minute_ready !=
                result.five_minute.fully_ready)
            {
                ++report.five_minute.
                    fully_ready_mismatches;
            }


            if (result.fifteen_minute_ready !=
                result.fifteen_minute.fully_ready)
            {
                ++report.fifteen_minute.
                    fully_ready_mismatches;
            }


            const bool expected_global_ready =
                result.one_minute.fully_ready &&
                result.five_minute.fully_ready &&
                result.fifteen_minute.fully_ready;


            if (result.fully_ready !=
                expected_global_ready)
            {
                ++report.one_minute.
                    fully_ready_mismatches;
            }


            // ----------------------------------------------------------------
            // Numeric integrity
            // ----------------------------------------------------------------

            const auto validateFiniteMetric =
                [&report](
                    const devai::statistics::
                        VolumeMetricStatistics& metric)
                {
                    if (!metric.has_observation)
                    {
                        return;
                    }

                    if (!std::isfinite(metric.value) ||
                        !std::isfinite(metric.rolling_mean) ||
                        !std::isfinite(
                            metric.
                                rolling_standard_deviation) ||
                        !std::isfinite(metric.z_score) ||
                        !std::isfinite(
                            metric.rolling_median) ||
                        !std::isfinite(
                            metric.rolling_mad) ||
                        !std::isfinite(
                            metric.robust_z_score))
                    {
                        ++report.
                            invalid_numeric_values;
                    }
                };


            validateFiniteMetric(
                result.one_minute.volume);

            validateFiniteMetric(
                result.one_minute.rvol20);

            validateFiniteMetric(
                result.one_minute.rvol50);


            validateFiniteMetric(
                result.five_minute.volume);

            validateFiniteMetric(
                result.five_minute.rvol20);

            validateFiniteMetric(
                result.five_minute.rvol50);


            validateFiniteMetric(
                result.fifteen_minute.volume);

            validateFiniteMetric(
                result.fifteen_minute.rvol20);

            validateFiniteMetric(
                result.fifteen_minute.rvol50);


            // ----------------------------------------------------------------
            // Preserve counts across session boundaries
            // ----------------------------------------------------------------

            previous_one_minute_total =
                one_minute_reference.
                    volume.total_observations;

            previous_five_minute_total =
                five_minute_reference.
                    volume.total_observations;

            previous_fifteen_minute_total =
                fifteen_minute_reference.
                    volume.total_observations;


            previous_decision_time =
                decision_time;
        }


        // ====================================================================
        // Copy source counters
        // ====================================================================

        report.one_minute.source_observations =
            one_minute_reference.
                source_observations;

        report.one_minute.repeated_sources =
            one_minute_reference.
                repeated_sources;


        report.five_minute.source_observations =
            five_minute_reference.
                source_observations;

        report.five_minute.repeated_sources =
            five_minute_reference.
                repeated_sources;


        report.fifteen_minute.source_observations =
            fifteen_minute_reference.
                source_observations;

        report.fifteen_minute.repeated_sources =
            fifteen_minute_reference.
                repeated_sources;


        // ====================================================================
        // Detailed reports
        // ====================================================================

        printHorizonValidation(
            "1-MINUTE",
            one_minute_reference,
            report.one_minute);


        printHorizonValidation(
            "5-MINUTE",
            five_minute_reference,
            report.five_minute);


        printHorizonValidation(
            "15-MINUTE",
            fifteen_minute_reference,
            report.fifteen_minute);


        // ====================================================================
        // Runtime / safety report
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n"
            << "RUNTIME / SAFETY VALIDATION\n"
            << "============================================================\n"
            << "Decision points                     : "
            << report.decision_points
            << "\n"
            << "Active-session points               : "
            << report.active_session_points
            << "\n"
            << "New sessions                        : "
            << report.new_sessions
            << "\n"
            << "Look-ahead violations               : "
            << report.lookahead_violations
            << "\n"
            << "Decision-time violations            : "
            << report.decision_time_violations
            << "\n"
            << "Cross-session continuity violations : "
            << report.
                   cross_session_continuity_violations
            << "\n"
            << "Invalid numeric values              : "
            << report.invalid_numeric_values
            << "\n"
            << "Phase 3.3 feature exceptions        : "
            << report.feature_exceptions
            << "\n"
            << "Phase 4.3 statistical exceptions    : "
            << report.statistical_exceptions
            << "\n";


        // ====================================================================
        // Final validation decision
        // ====================================================================

        const bool one_minute_passed =
            horizonPassed(
                one_minute_reference,
                report.one_minute);


        const bool five_minute_passed =
            horizonPassed(
                five_minute_reference,
                report.five_minute);


        const bool fifteen_minute_passed =
            horizonPassed(
                fifteen_minute_reference,
                report.fifteen_minute);


        const bool runtime_passed =
            report.decision_points > 0 &&
            report.active_session_points > 0 &&
            report.new_sessions > 0 &&
            report.lookahead_violations == 0 &&
            report.decision_time_violations == 0 &&
            report.
                cross_session_continuity_violations == 0 &&
            report.invalid_numeric_values == 0 &&
            report.feature_exceptions == 0 &&
            report.statistical_exceptions == 0;


        const bool passed =
            one_minute_passed &&
            five_minute_passed &&
            fifteen_minute_passed &&
            runtime_passed;


        // ====================================================================
        // Final output
        // ====================================================================

        std::cout
            << "\n"
            << "============================================================\n";


        if (!passed)
        {
            std::cout
                << "PHASE 4.3 REAL VOLUME STATISTICAL VALIDATION FAILED\n"
                << "============================================================\n";

            return EXIT_FAILURE;
        }


        std::cout
            << "PHASE 4.3 REAL VOLUME STATISTICAL VALIDATION PASSED\n"
            << "============================================================\n"
            << "1m Raw Volume Statistics       : PASSED\n"
            << "1m RVOL20 Statistics           : PASSED\n"
            << "1m RVOL50 Statistics           : PASSED\n"
            << "5m Raw Volume Statistics       : PASSED\n"
            << "5m RVOL20 Statistics           : PASSED\n"
            << "5m RVOL50 Statistics           : PASSED\n"
            << "15m Raw Volume Statistics      : PASSED\n"
            << "15m RVOL20 Statistics          : PASSED\n"
            << "15m RVOL50 Statistics          : PASSED\n"
            << "Abnormal Volume State          : PASSED\n"
            << "Repeated Candle Protection     : PASSED\n"
            << "Window Readiness               : PASSED\n"
            << "Cross-Session Continuity       : PASSED\n"
            << "Look-Ahead Protection          : PASSED\n"
            << "Decision-Time Integrity        : PASSED\n"
            << "Numeric Integrity              : PASSED\n"
            << "============================================================\n";


        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "REAL VOLUME STATISTICAL VALIDATION ERROR\n"
            << "------------------------------------------------------------\n"
            << exception.what()
            << "\n";

        return EXIT_FAILURE;
    }
}