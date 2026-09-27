#include "devai/statistics/RollingStatisticalEngine.hpp"

#include "devai/statistics/RobustStatistics.hpp"

#include <cmath>
#include <deque>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devai::statistics
{

RollingStatisticalEngine::RollingStatisticalEngine(
    std::string symbol,
    std::size_t window_size)
    : symbol_(std::move(symbol)),
      statistics_(window_size)
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "RollingStatisticalEngine symbol cannot be empty.");
    }
}

RollingStatisticalFeatures
RollingStatisticalEngine::update(
    std::int64_t decision_time,
    double value)
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument(
            "RollingStatisticalEngine received "
            "a non-finite value.");
    }

    if (last_decision_time_.has_value())
    {
        if (decision_time < *last_decision_time_)
        {
            throw std::runtime_error(
                "RollingStatisticalEngine received "
                "a decreasing decision time.");
        }

        if (decision_time == *last_decision_time_)
        {
            throw std::runtime_error(
                "RollingStatisticalEngine received "
                "a duplicate decision time.");
        }
    }

    // --------------------------------------------------------
    // Add current observation first.
    //
    // Therefore the rolling statistics for decision_time
    // include the current legally available observation.
    // --------------------------------------------------------

    statistics_.add(value);

    last_decision_time_ = decision_time;

    RollingStatisticalFeatures features;

    features.symbol = symbol_;
    features.decision_time = decision_time;

    features.value = value;
    features.has_value = true;

    features.window_size = statistics_.windowSize();
    features.observation_count = statistics_.size();

    // --------------------------------------------------------
    // Mean
    // --------------------------------------------------------

    features.rolling_mean = statistics_.mean();

    features.has_mean =
        std::isfinite(features.rolling_mean);

    // --------------------------------------------------------
    // Standard deviation
    // --------------------------------------------------------

    features.rolling_standard_deviation =
        statistics_.standardDeviation();

    features.has_standard_deviation =
        std::isfinite(
            features.rolling_standard_deviation);

    // --------------------------------------------------------
    // Standard Z-score
    // --------------------------------------------------------

    features.z_score =
        statistics_.zScore(value);

    features.has_z_score =
        std::isfinite(features.z_score);

    // --------------------------------------------------------
    // Convert rolling deque into vector for robust statistics
    // --------------------------------------------------------

    const std::deque<double>& rolling_values =
        statistics_.values();

    const std::vector<double> values(
        rolling_values.begin(),
        rolling_values.end());

    // --------------------------------------------------------
    // Median
    // --------------------------------------------------------

    features.rolling_median =
        RobustStatistics::median(values);

    features.has_median =
        std::isfinite(features.rolling_median);

    // --------------------------------------------------------
    // MAD
    // --------------------------------------------------------

    features.rolling_mad =
        RobustStatistics::medianAbsoluteDeviation(values);

    features.has_mad =
        std::isfinite(features.rolling_mad);

    // --------------------------------------------------------
    // Robust Z-score
    // --------------------------------------------------------

    features.robust_z_score =
        RobustStatistics::robustZScore(
            value,
            values);

    features.has_robust_z_score =
        std::isfinite(features.robust_z_score);

    // --------------------------------------------------------
    // Window readiness
    // --------------------------------------------------------

    features.ready = statistics_.ready();

    return features;
}

void RollingStatisticalEngine::reset() noexcept
{
    statistics_.reset();

    last_decision_time_.reset();
}

const std::string&
RollingStatisticalEngine::symbol() const noexcept
{
    return symbol_;
}

std::size_t
RollingStatisticalEngine::windowSize() const noexcept
{
    return statistics_.windowSize();
}

std::size_t
RollingStatisticalEngine::observationCount() const noexcept
{
    return statistics_.size();
}

bool
RollingStatisticalEngine::ready() const noexcept
{
    return statistics_.ready();
}

} // namespace devai::statistics