#include "devai/statistics/VolumeStatisticalEngine.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::statistics
{

// ============================================================================
// MetricState
// ============================================================================

VolumeStatisticalEngine::MetricState::MetricState(
    const std::string& symbol,
    std::size_t window_size)
    :
    statistics(
        symbol,
        window_size)
{
}


// ============================================================================
// HorizonState
// ============================================================================

VolumeStatisticalEngine::HorizonState::HorizonState(
    const std::string& symbol,
    std::size_t window_size)
    :
    volume(
        symbol,
        window_size),

    rvol20(
        symbol,
        window_size),

    rvol50(
        symbol,
        window_size)
{
}


// ============================================================================
// Constructor
// ============================================================================

VolumeStatisticalEngine::VolumeStatisticalEngine(
    std::string symbol,
    std::size_t window_size,
    double low_z_threshold,
    double high_z_threshold,
    double extreme_z_threshold)
    :
    symbol_(
        std::move(symbol)),

    window_size_(
        window_size),

    low_z_threshold_(
        low_z_threshold),

    high_z_threshold_(
        high_z_threshold),

    extreme_z_threshold_(
        extreme_z_threshold),

    one_minute_(
        symbol_,
        window_size_),

    five_minute_(
        symbol_,
        window_size_),

    fifteen_minute_(
        symbol_,
        window_size_)
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: symbol cannot be empty.");
    }


    if (window_size_ == 0)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: window size cannot be zero.");
    }


    if (!std::isfinite(low_z_threshold_) ||
        !std::isfinite(high_z_threshold_) ||
        !std::isfinite(extreme_z_threshold_))
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: thresholds must be finite.");
    }


    if (low_z_threshold_ >= 0.0)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: low Z threshold must be negative.");
    }


    if (high_z_threshold_ <= 0.0)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: high Z threshold must be positive.");
    }


    if (extreme_z_threshold_ <= high_z_threshold_)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: extreme Z threshold "
            "must be greater than high Z threshold.");
    }


    if (-extreme_z_threshold_ >= low_z_threshold_)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: negative extreme Z threshold "
            "must be lower than low Z threshold.");
    }
}


// ============================================================================
// Accessors
// ============================================================================

const std::string&
VolumeStatisticalEngine::symbol() const noexcept
{
    return symbol_;
}


std::size_t
VolumeStatisticalEngine::windowSize() const noexcept
{
    return window_size_;
}


double
VolumeStatisticalEngine::lowZThreshold() const noexcept
{
    return low_z_threshold_;
}


double
VolumeStatisticalEngine::highZThreshold() const noexcept
{
    return high_z_threshold_;
}


double
VolumeStatisticalEngine::extremeZThreshold() const noexcept
{
    return extreme_z_threshold_;
}


// ============================================================================
// Copy Phase 4.1 rolling-statistical output into Phase 4.3 metric output
// ============================================================================

void VolumeStatisticalEngine::copyStatistics(
    const RollingStatisticalFeatures& source,
    VolumeMetricStatistics& destination)
{
    destination.value =
        source.value;

    destination.rolling_mean =
        source.rolling_mean;

    destination.rolling_standard_deviation =
        source.rolling_standard_deviation;

    destination.z_score =
        source.z_score;

    destination.rolling_median =
        source.rolling_median;

    destination.rolling_mad =
        source.rolling_mad;

    destination.robust_z_score =
        source.robust_z_score;

    destination.observation_count =
        source.observation_count;

    destination.has_observation =
        source.has_value;

    destination.ready =
        source.ready;
}


// ============================================================================
// Abnormal-volume classification
//
// IMPORTANT:
//
// This classification describes RAW VOLUME abnormality.
//
// It is NOT:
//     - a BUY signal
//     - a SELL signal
//     - directional market interpretation
//
// State boundaries:
//
//     z <= -extreme      VERY_LOW
//     z <= low           LOW
//     low < z < high     NORMAL
//     z >= high          HIGH
//     z >= extreme       EXTREME_HIGH
// ============================================================================

AbnormalVolumeState
VolumeStatisticalEngine::classifyAbnormalVolume(
    const VolumeMetricStatistics& volume_statistics)
    const noexcept
{
    if (!volume_statistics.has_observation)
    {
        return
            AbnormalVolumeState::UNAVAILABLE;
    }


    if (!std::isfinite(
            volume_statistics.z_score))
    {
        return
            AbnormalVolumeState::UNAVAILABLE;
    }


    const double z_score =
        volume_statistics.z_score;


    // ------------------------------------------------------------------------
    // Extreme high volume
    // ------------------------------------------------------------------------

    if (z_score >= extreme_z_threshold_)
    {
        return
            AbnormalVolumeState::EXTREME_HIGH;
    }


    // ------------------------------------------------------------------------
    // High volume
    // ------------------------------------------------------------------------

    if (z_score >= high_z_threshold_)
    {
        return
            AbnormalVolumeState::HIGH;
    }


    // ------------------------------------------------------------------------
    // Extremely low volume
    // ------------------------------------------------------------------------

    if (z_score <= -extreme_z_threshold_)
    {
        return
            AbnormalVolumeState::VERY_LOW;
    }


    // ------------------------------------------------------------------------
    // Low volume
    // ------------------------------------------------------------------------

    if (z_score <= low_z_threshold_)
    {
        return
            AbnormalVolumeState::LOW;
    }


    // ------------------------------------------------------------------------
    // Normal statistical range
    // ------------------------------------------------------------------------

    return
        AbnormalVolumeState::NORMAL;
}


// ============================================================================
// Update one statistical metric
//
// A metric may become available later than the source candle.
//
// Example:
//
//     raw volume      -> available immediately
//     RVOL20          -> available after Phase 3.3 warm-up
//     RVOL50          -> available after longer Phase 3.3 warm-up
//
// Therefore each metric has its own RollingStatisticalEngine.
// ============================================================================

void VolumeStatisticalEngine::updateMetric(
    MetricState& state,
    bool has_observation,
    std::int64_t decision_time,
    double value)
{
    if (!has_observation)
    {
        return;
    }


    if (!std::isfinite(value))
    {
        throw std::runtime_error(
            "VolumeStatisticalEngine: "
            "available metric contains a non-finite value.");
    }


    const auto statistics =
        state.statistics.update(
            decision_time,
            value);


    copyStatistics(
        statistics,
        state.latest);
}


// ============================================================================
// Update one timeframe
//
// The source timestamp prevents a completed 5m/15m candle from being
// consumed repeatedly while that same candle remains the latest available
// candle in successive 1-minute MarketSnapshots.
//
// Example:
//
// Decision time       Latest 5m source
// ------------------------------------------------
// 09:20               09:15
// 09:21               09:15  -> ignored
// 09:22               09:15  -> ignored
// 09:23               09:15  -> ignored
// 09:24               09:15  -> ignored
// 09:25               09:20  -> new observation
//
// Phase 4.3 consumes the new source candle exactly once.
// ============================================================================

void VolumeStatisticalEngine::updateHorizon(
    HorizonState& state,

    bool has_source_candle,

    std::optional<std::int64_t>
        source_timestamp,

    std::int64_t decision_time,

    bool has_volume,
    double volume,

    bool has_rvol20,
    double rvol20,

    bool has_rvol50,
    double rvol50)
{
    // ------------------------------------------------------------------------
    // No legally available source candle
    // ------------------------------------------------------------------------

    if (!has_source_candle ||
        !source_timestamp.has_value())
    {
        return;
    }


    // ------------------------------------------------------------------------
    // Source-candle ordering / duplicate protection
    // ------------------------------------------------------------------------

    if (state.last_source_timestamp.has_value())
    {
        if (*source_timestamp <
            *state.last_source_timestamp)
        {
            throw std::runtime_error(
                "VolumeStatisticalEngine: "
                "source candle timestamp moved backwards.");
        }


        if (*source_timestamp ==
            *state.last_source_timestamp)
        {
            // Same completed candle carried forward by the
            // minute-by-minute runtime.
            //
            // Do NOT insert it again into any rolling window.

            return;
        }
    }


    // ------------------------------------------------------------------------
    // Raw Volume
    // ------------------------------------------------------------------------

    updateMetric(
        state.volume,
        has_volume,
        decision_time,
        volume);


    // ------------------------------------------------------------------------
    // RVOL20
    //
    // Phase 4.3 does NOT calculate RVOL20.
    // It statistically interprets the Phase 3.3 result.
    // ------------------------------------------------------------------------

    updateMetric(
        state.rvol20,
        has_rvol20,
        decision_time,
        rvol20);


    // ------------------------------------------------------------------------
    // RVOL50
    //
    // Phase 4.3 does NOT calculate RVOL50.
    // It statistically interprets the Phase 3.3 result.
    // ------------------------------------------------------------------------

    updateMetric(
        state.rvol50,
        has_rvol50,
        decision_time,
        rvol50);


    // ------------------------------------------------------------------------
    // Copy latest metric states into horizon output
    // ------------------------------------------------------------------------

    state.latest.volume =
        state.volume.latest;

    state.latest.rvol20 =
        state.rvol20.latest;

    state.latest.rvol50 =
        state.rvol50.latest;


    // ------------------------------------------------------------------------
    // Availability
    // ------------------------------------------------------------------------

    state.latest.has_volume =
        state.volume.latest.has_observation;

    state.latest.has_rvol20 =
        state.rvol20.latest.has_observation;

    state.latest.has_rvol50 =
        state.rvol50.latest.has_observation;


    // ------------------------------------------------------------------------
    // Statistical-window readiness
    // ------------------------------------------------------------------------

    state.latest.volume_ready =
        state.volume.latest.ready;

    state.latest.rvol20_ready =
        state.rvol20.latest.ready;

    state.latest.rvol50_ready =
        state.rvol50.latest.ready;


    // ------------------------------------------------------------------------
    // Full horizon readiness
    //
    // All three statistical streams must have completed their
    // Phase 4.3 rolling window.
    // ------------------------------------------------------------------------

    state.latest.fully_ready =
        state.latest.volume_ready &&
        state.latest.rvol20_ready &&
        state.latest.rvol50_ready;


    // ------------------------------------------------------------------------
    // Descriptive abnormal-volume state
    //
    // Based on RAW VOLUME Z-score.
    // ------------------------------------------------------------------------

    state.latest.abnormal_volume_state =
        classifyAbnormalVolume(
            state.volume.latest);


    // ------------------------------------------------------------------------
    // Mark source candle as consumed
    // ------------------------------------------------------------------------

    state.last_source_timestamp =
        *source_timestamp;
}


// ============================================================================
// Main update
// ============================================================================

VolumeStatisticalFeatures
VolumeStatisticalEngine::update(
    const devai::features::VolumeFeatures& features,
    const devai::market::MarketSnapshot& snapshot)
{
    // ------------------------------------------------------------------------
    // Symbol validation
    // ------------------------------------------------------------------------

    if (features.symbol != symbol_)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: "
            "VolumeFeatures symbol mismatch.");
    }


    if (snapshot.symbol != symbol_)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: "
            "MarketSnapshot symbol mismatch.");
    }


    // ------------------------------------------------------------------------
    // Decision-time consistency
    // ------------------------------------------------------------------------

    if (features.decision_time !=
        snapshot.decision_time)
    {
        throw std::invalid_argument(
            "VolumeStatisticalEngine: "
            "VolumeFeatures and MarketSnapshot "
            "decision times do not match.");
    }


    // ------------------------------------------------------------------------
    // Global decision-time ordering
    // ------------------------------------------------------------------------

    if (last_decision_time_.has_value())
    {
        if (features.decision_time <
            *last_decision_time_)
        {
            throw std::runtime_error(
                "VolumeStatisticalEngine: "
                "decision time moved backwards.");
        }


        if (features.decision_time ==
            *last_decision_time_)
        {
            throw std::runtime_error(
                "VolumeStatisticalEngine: "
                "duplicate decision time.");
        }
    }


    // ------------------------------------------------------------------------
    // Extract source-candle timestamps.
    //
    // MarketSnapshot is used only for source identity.
    //
    // Volume/RVOL values themselves come exclusively from frozen
    // Phase 3.3 VolumeFeatures.
    // ------------------------------------------------------------------------

    std::optional<std::int64_t>
        one_minute_timestamp;

    std::optional<std::int64_t>
        five_minute_timestamp;

    std::optional<std::int64_t>
        fifteen_minute_timestamp;


    if (snapshot.one_minute.has_value())
    {
        one_minute_timestamp =
            snapshot.one_minute->timestamp;
    }


    if (snapshot.five_minute.has_value())
    {
        five_minute_timestamp =
            snapshot.five_minute->timestamp;
    }


    if (snapshot.fifteen_minute.has_value())
    {
        fifteen_minute_timestamp =
            snapshot.fifteen_minute->timestamp;
    }


    // ========================================================================
    // 1-MINUTE VOLUME STATISTICS
    //
    // Actual frozen Phase 3.3 mapping:
    //
    // features.one_minute.volume
    // features.one_minute.relative_volume_20
    // features.one_minute.relative_volume_50
    // ========================================================================

    updateHorizon(
        one_minute_,

        features.one_minute.has_volume,

        one_minute_timestamp,

        features.decision_time,

        features.one_minute.has_volume,
        features.one_minute.volume,

        features.one_minute.has_relative_volume_20,
        features.one_minute.relative_volume_20,

        features.one_minute.has_relative_volume_50,
        features.one_minute.relative_volume_50);


    // ========================================================================
    // 5-MINUTE VOLUME STATISTICS
    // ========================================================================

    updateHorizon(
        five_minute_,

        features.five_minute.has_volume,

        five_minute_timestamp,

        features.decision_time,

        features.five_minute.has_volume,
        features.five_minute.volume,

        features.five_minute.has_relative_volume_20,
        features.five_minute.relative_volume_20,

        features.five_minute.has_relative_volume_50,
        features.five_minute.relative_volume_50);


    // ========================================================================
    // 15-MINUTE VOLUME STATISTICS
    // ========================================================================

    updateHorizon(
        fifteen_minute_,

        features.fifteen_minute.has_volume,

        fifteen_minute_timestamp,

        features.decision_time,

        features.fifteen_minute.has_volume,
        features.fifteen_minute.volume,

        features.fifteen_minute.has_relative_volume_20,
        features.fifteen_minute.relative_volume_20,

        features.fifteen_minute.has_relative_volume_50,
        features.fifteen_minute.relative_volume_50);


    // ========================================================================
    // Build Phase 4.3 output
    // ========================================================================

    VolumeStatisticalFeatures result;


    result.symbol =
        symbol_;


    result.decision_time =
        features.decision_time;


    result.one_minute =
        one_minute_.latest;


    result.five_minute =
        five_minute_.latest;


    result.fifteen_minute =
        fifteen_minute_.latest;


    // ------------------------------------------------------------------------
    // Per-timeframe readiness
    // ------------------------------------------------------------------------

    result.one_minute_ready =
        result.one_minute.fully_ready;


    result.five_minute_ready =
        result.five_minute.fully_ready;


    result.fifteen_minute_ready =
        result.fifteen_minute.fully_ready;


    // ------------------------------------------------------------------------
    // Complete Phase 4.3 readiness
    // ------------------------------------------------------------------------

    result.fully_ready =
        result.one_minute_ready &&
        result.five_minute_ready &&
        result.fifteen_minute_ready;


    // ------------------------------------------------------------------------
    // Commit decision time only after successful processing
    // ------------------------------------------------------------------------

    last_decision_time_ =
        features.decision_time;


    return result;
}


// ============================================================================
// Explicit reset
//
// IMPORTANT:
//
// This is an explicit engine reset.
//
// It is NOT called at a daily/session boundary.
//
// CETE policy:
// continuous mathematical/statistical history survives across sessions.
// ============================================================================

void VolumeStatisticalEngine::reset() noexcept
{
    // ------------------------------------------------------------------------
    // Reset 1-minute rolling engines
    // ------------------------------------------------------------------------

    one_minute_.volume.statistics.reset();

    one_minute_.rvol20.statistics.reset();

    one_minute_.rvol50.statistics.reset();


    // ------------------------------------------------------------------------
    // Reset 5-minute rolling engines
    // ------------------------------------------------------------------------

    five_minute_.volume.statistics.reset();

    five_minute_.rvol20.statistics.reset();

    five_minute_.rvol50.statistics.reset();


    // ------------------------------------------------------------------------
    // Reset 15-minute rolling engines
    // ------------------------------------------------------------------------

    fifteen_minute_.volume.statistics.reset();

    fifteen_minute_.rvol20.statistics.reset();

    fifteen_minute_.rvol50.statistics.reset();


    // ------------------------------------------------------------------------
    // Reset source-candle identity
    // ------------------------------------------------------------------------

    one_minute_.last_source_timestamp.reset();

    five_minute_.last_source_timestamp.reset();

    fifteen_minute_.last_source_timestamp.reset();


    // ------------------------------------------------------------------------
    // Reset horizon outputs
    // ------------------------------------------------------------------------

    one_minute_.latest =
        VolumeHorizonStatistics{};

    five_minute_.latest =
        VolumeHorizonStatistics{};

    fifteen_minute_.latest =
        VolumeHorizonStatistics{};


    // ------------------------------------------------------------------------
    // Reset individual metric outputs
    // ------------------------------------------------------------------------

    one_minute_.volume.latest =
        VolumeMetricStatistics{};

    one_minute_.rvol20.latest =
        VolumeMetricStatistics{};

    one_minute_.rvol50.latest =
        VolumeMetricStatistics{};


    five_minute_.volume.latest =
        VolumeMetricStatistics{};

    five_minute_.rvol20.latest =
        VolumeMetricStatistics{};

    five_minute_.rvol50.latest =
        VolumeMetricStatistics{};


    fifteen_minute_.volume.latest =
        VolumeMetricStatistics{};

    fifteen_minute_.rvol20.latest =
        VolumeMetricStatistics{};

    fifteen_minute_.rvol50.latest =
        VolumeMetricStatistics{};


    // ------------------------------------------------------------------------
    // Reset decision-time state
    // ------------------------------------------------------------------------

    last_decision_time_.reset();
}

} // namespace devai::statistics