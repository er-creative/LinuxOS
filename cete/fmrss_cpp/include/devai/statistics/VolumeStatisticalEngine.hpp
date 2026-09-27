#pragma once

#include "devai/features/VolumeFeatures.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include "devai/statistics/RollingStatisticalEngine.hpp"
#include "devai/statistics/VolumeStatisticalFeatures.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace devai::statistics
{

class VolumeStatisticalEngine
{
public:

    explicit VolumeStatisticalEngine(
        std::string symbol,
        std::size_t window_size = 60,
        double low_z_threshold = -1.0,
        double high_z_threshold = 1.0,
        double extreme_z_threshold = 2.0);


    VolumeStatisticalFeatures update(
        const devai::features::VolumeFeatures& features,
        const devai::market::MarketSnapshot& snapshot);


    void reset() noexcept;


    [[nodiscard]]
    const std::string& symbol() const noexcept;


    [[nodiscard]]
    std::size_t windowSize() const noexcept;


    [[nodiscard]]
    double lowZThreshold() const noexcept;


    [[nodiscard]]
    double highZThreshold() const noexcept;


    [[nodiscard]]
    double extremeZThreshold() const noexcept;


private:

    // ========================================================
    // One statistical stream
    // ========================================================

    struct MetricState
    {
        RollingStatisticalEngine statistics;

        VolumeMetricStatistics latest;


        MetricState(
            const std::string& symbol,
            std::size_t window_size);
    };


    // ========================================================
    // One timeframe
    // ========================================================

    struct HorizonState
    {
        MetricState volume;

        MetricState rvol20;

        MetricState rvol50;

        std::optional<std::int64_t>
            last_source_timestamp;

        VolumeHorizonStatistics latest;


        HorizonState(
            const std::string& symbol,
            std::size_t window_size);
    };


    static void copyStatistics(
        const RollingStatisticalFeatures& source,
        VolumeMetricStatistics& destination);


    [[nodiscard]]
    AbnormalVolumeState classifyAbnormalVolume(
        const VolumeMetricStatistics& volume_statistics)
        const noexcept;


    void updateMetric(
        MetricState& state,
        bool has_observation,
        std::int64_t decision_time,
        double value);


    void updateHorizon(
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
        double rvol50);


    std::string symbol_;

    std::size_t window_size_{60};

    double low_z_threshold_{-1.0};

    double high_z_threshold_{1.0};

    double extreme_z_threshold_{2.0};

    std::optional<std::int64_t>
        last_decision_time_;

    HorizonState one_minute_;

    HorizonState five_minute_;

    HorizonState fifteen_minute_;
};

} // namespace devai::statistics