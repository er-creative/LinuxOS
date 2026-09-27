#pragma once

#include "devai/statistics/RollingStatisticalFeatures.hpp"
#include "devai/statistics/RollingStatistics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace devai::statistics
{

class RollingStatisticalEngine
{
public:
    explicit RollingStatisticalEngine(
        std::string symbol,
        std::size_t window_size);

    RollingStatisticalFeatures update(
        std::int64_t decision_time,
        double value);

    void reset() noexcept;

    [[nodiscard]]
    const std::string& symbol() const noexcept;

    [[nodiscard]]
    std::size_t windowSize() const noexcept;

    [[nodiscard]]
    std::size_t observationCount() const noexcept;

    [[nodiscard]]
    bool ready() const noexcept;

private:
    std::string symbol_;

    RollingStatistics statistics_;

    std::optional<std::int64_t> last_decision_time_;
};

} // namespace devai::statistics