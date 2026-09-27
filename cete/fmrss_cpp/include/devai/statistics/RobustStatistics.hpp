#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace devai::statistics
{

class RobustStatistics
{
public:
    [[nodiscard]]
    static double median(const std::vector<double>& values)
    {
        if (values.empty())
        {
            return nan();
        }

        std::vector<double> copy = values;

        for (const double value : copy)
        {
            if (!std::isfinite(value))
            {
                return nan();
            }
        }

        std::sort(copy.begin(), copy.end());

        const std::size_t size = copy.size();
        const std::size_t middle = size / 2;

        if ((size % 2) != 0)
        {
            return copy[middle];
        }

        return (copy[middle - 1] + copy[middle]) / 2.0;
    }

    [[nodiscard]]
    static double medianAbsoluteDeviation(
        const std::vector<double>& values)
    {
        if (values.empty())
        {
            return nan();
        }

        const double current_median = median(values);

        if (!std::isfinite(current_median))
        {
            return nan();
        }

        std::vector<double> deviations;
        deviations.reserve(values.size());

        for (const double value : values)
        {
            if (!std::isfinite(value))
            {
                return nan();
            }

            deviations.push_back(
                std::abs(value - current_median));
        }

        return median(deviations);
    }

    [[nodiscard]]
    static double robustZScore(
        double value,
        const std::vector<double>& values)
    {
        if (!std::isfinite(value) || values.empty())
        {
            return nan();
        }

        const double current_median = median(values);

        const double current_mad =
            medianAbsoluteDeviation(values);

        if (!std::isfinite(current_median) ||
            !std::isfinite(current_mad))
        {
            return nan();
        }

        if (current_mad <= epsilon())
        {
            return 0.0;
        }

        // Modified / robust Z-score.
        //
        // 0.6744897501960817 is the approximately 75th
        // percentile of the standard normal distribution.
        constexpr double NORMAL_CONSISTENCY_CONSTANT =
            0.6744897501960817;

        return NORMAL_CONSISTENCY_CONSTANT *
               (value - current_median) /
               current_mad;
    }

private:
    [[nodiscard]]
    static constexpr double epsilon() noexcept
    {
        return 1.0e-12;
    }

    [[nodiscard]]
    static constexpr double nan() noexcept
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
};

} // namespace devai::statistics