#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace devai::statistics
{

struct RollingStatisticalFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};

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

    std::size_t window_size{0};

    bool has_value{false};

    bool has_mean{false};

    bool has_standard_deviation{false};

    bool has_z_score{false};

    bool has_median{false};

    bool has_mad{false};

    bool has_robust_z_score{false};

    bool ready{false};
};

} // namespace devai::statistics