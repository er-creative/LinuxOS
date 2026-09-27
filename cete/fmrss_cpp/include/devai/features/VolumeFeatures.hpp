#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace devai::features
{

inline double volumeFeatureNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// Volume features for one timeframe
// ============================================================================

struct TimeframeVolumeFeatures
{
    // Current completed candle volume.
    double volume{volumeFeatureNaN()};

    // Rolling averages INCLUDING the current completed candle.
    double average_volume_20{volumeFeatureNaN()};
    double average_volume_50{volumeFeatureNaN()};

    // Relative volume:
    //
    //     current volume / rolling average volume
    //
    // Available only when the corresponding rolling average is available
    // and greater than zero.
    double relative_volume_20{volumeFeatureNaN()};
    double relative_volume_50{volumeFeatureNaN()};

    bool has_volume{false};

    bool has_average_volume_20{false};
    bool has_average_volume_50{false};

    bool has_relative_volume_20{false};
    bool has_relative_volume_50{false};
};


// ============================================================================
// Complete Phase 3.3 output
// ============================================================================

struct VolumeFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};


    // ------------------------------------------------------------------------
    // Continuous rolling features
    //
    // These DO NOT reset when a new trading session begins.
    // ------------------------------------------------------------------------

    TimeframeVolumeFeatures one_minute;
    TimeframeVolumeFeatures five_minute;
    TimeframeVolumeFeatures fifteen_minute;


    // ------------------------------------------------------------------------
    // Current-session 1-minute volume state
    //
    // These DO reset at the beginning of a new trading session.
    // ------------------------------------------------------------------------

    double session_cumulative_volume{0.0};

    std::size_t session_bar_count{0};

    double session_average_volume_per_bar{volumeFeatureNaN()};

    bool has_session_volume{false};
};

} // namespace devai::features