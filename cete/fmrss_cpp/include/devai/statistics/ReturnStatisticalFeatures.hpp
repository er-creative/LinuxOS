#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace devai::statistics
{

enum class StatisticalExtremeState
{
    UNAVAILABLE,

    EXTREME_NEGATIVE,
    NEGATIVE,

    NORMAL,

    POSITIVE,
    EXTREME_POSITIVE
};


struct ReturnHorizonStatistics
{
    // --------------------------------------------------------
    // Observation
    // --------------------------------------------------------

    double return_value{
        std::numeric_limits<double>::quiet_NaN()
    };

    double absolute_return{
        std::numeric_limits<double>::quiet_NaN()
    };


    // --------------------------------------------------------
    // Ordinary return statistics
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // Absolute-return statistics
    // --------------------------------------------------------

    double absolute_return_mean{
        std::numeric_limits<double>::quiet_NaN()
    };

    double absolute_return_standard_deviation{
        std::numeric_limits<double>::quiet_NaN()
    };

    double absolute_return_z_score{
        std::numeric_limits<double>::quiet_NaN()
    };

    double absolute_return_median{
        std::numeric_limits<double>::quiet_NaN()
    };

    double absolute_return_mad{
        std::numeric_limits<double>::quiet_NaN()
    };

    double absolute_return_robust_z_score{
        std::numeric_limits<double>::quiet_NaN()
    };


    // --------------------------------------------------------
    // State
    // --------------------------------------------------------

    StatisticalExtremeState extreme_state{
        StatisticalExtremeState::UNAVAILABLE
    };

    std::size_t observation_count{0};

    bool has_observation{false};

    bool ready{false};
};


struct ReturnStatisticalFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};

    ReturnHorizonStatistics one_minute;

    ReturnHorizonStatistics five_minute;

    ReturnHorizonStatistics fifteen_minute;

    bool one_minute_ready{false};

    bool five_minute_ready{false};

    bool fifteen_minute_ready{false};

    bool fully_ready{false};
};

} // namespace devai::statistics