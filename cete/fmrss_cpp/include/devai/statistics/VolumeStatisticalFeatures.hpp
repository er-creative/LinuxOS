#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace devai::statistics
{

// ============================================================
// Abnormal-volume state
//
// Descriptive statistical state only.
// This is NOT a BUY / SELL signal.
// ============================================================

enum class AbnormalVolumeState
{
    UNAVAILABLE,

    VERY_LOW,
    LOW,
    NORMAL,
    HIGH,
    EXTREME_HIGH
};


// ============================================================
// Statistics for one volume-derived value
// ============================================================

struct VolumeMetricStatistics
{
    double value{
        std::numeric_limits<double>::quiet_NaN()
    };

    double rolling_mean{
        std::numeric_limits<double>::quiet_NaN()
    };

    double rolling_standard_deviation{
        std::numeric_limits<double>::quiet_NaN()
    };

    double z_score{
        std::numeric_limits<double>::quiet_NaN()
    };

    double rolling_median{
        std::numeric_limits<double>::quiet_NaN()
    };

    double rolling_mad{
        std::numeric_limits<double>::quiet_NaN()
    };

    double robust_z_score{
        std::numeric_limits<double>::quiet_NaN()
    };

    std::size_t observation_count{0};

    bool has_observation{false};

    bool ready{false};
};


// ============================================================
// One timeframe
//
// Three statistical streams are deliberately kept separate:
//
// 1. raw volume
// 2. RVOL20
// 3. RVOL50
// ============================================================

struct VolumeHorizonStatistics
{
    VolumeMetricStatistics volume;

    VolumeMetricStatistics rvol20;

    VolumeMetricStatistics rvol50;

    AbnormalVolumeState abnormal_volume_state{
        AbnormalVolumeState::UNAVAILABLE
    };

    bool has_volume{false};

    bool has_rvol20{false};

    bool has_rvol50{false};

    bool volume_ready{false};

    bool rvol20_ready{false};

    bool rvol50_ready{false};

    bool fully_ready{false};
};


// ============================================================
// Complete Phase 4.3 output
// ============================================================

struct VolumeStatisticalFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};

    VolumeHorizonStatistics one_minute;

    VolumeHorizonStatistics five_minute;

    VolumeHorizonStatistics fifteen_minute;

    bool one_minute_ready{false};

    bool five_minute_ready{false};

    bool fifteen_minute_ready{false};

    bool fully_ready{false};
};

} // namespace devai::statistics