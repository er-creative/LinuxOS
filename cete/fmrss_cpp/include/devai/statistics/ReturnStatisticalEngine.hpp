#pragma once

#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include "devai/statistics/RollingStatisticalEngine.hpp"
#include "devai/statistics/ReturnStatisticalFeatures.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace devai::statistics
{

class ReturnStatisticalEngine
{
public:

    explicit ReturnStatisticalEngine(
        std::string symbol,
        std::size_t window_size = 60,
        double extreme_z_threshold = 2.0);

    ReturnStatisticalFeatures update(
        const devai::features::PriceReturnFeatures& features,
        const devai::market::MarketSnapshot& snapshot);

    void reset() noexcept;

    [[nodiscard]]
    const std::string& symbol() const noexcept;

    [[nodiscard]]
    std::size_t windowSize() const noexcept;

    [[nodiscard]]
    double extremeZThreshold() const noexcept;


private:

    struct HorizonState
    {
        RollingStatisticalEngine return_statistics;

        RollingStatisticalEngine absolute_return_statistics;

        std::optional<std::int64_t> last_source_timestamp;

        ReturnHorizonStatistics latest;

        HorizonState(
            const std::string& symbol,
            std::size_t window_size);
    };


    static StatisticalExtremeState classifyExtreme(
        double return_value,
        double z_score,
        double extreme_threshold) noexcept;


    static void copyStatistics(
        const RollingStatisticalFeatures& source,
        ReturnHorizonStatistics& destination);


    static void copyAbsoluteStatistics(
        const RollingStatisticalFeatures& source,
        ReturnHorizonStatistics& destination);


    void updateHorizon(
        HorizonState& state,
        bool has_observation,
        std::optional<std::int64_t> source_timestamp,
        std::int64_t decision_time,
        double return_value);


    std::string symbol_;

    std::size_t window_size_{60};

    double extreme_z_threshold_{2.0};

    std::optional<std::int64_t> last_decision_time_;

    HorizonState one_minute_;

    HorizonState five_minute_;

    HorizonState fifteen_minute_;
};

} // namespace devai::statistics