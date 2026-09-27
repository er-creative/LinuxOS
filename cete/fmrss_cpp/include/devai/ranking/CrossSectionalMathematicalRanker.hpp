#pragma once

#include "devai/ranking/CrossSectionalMathematicalFeatures.hpp"
#include "devai/ranking/CrossSectionalRanker.hpp"
#include "devai/statistics/MathematicalStateFeatures.hpp"

#include <cstddef>
#include <vector>


namespace devai::ranking
{

class CrossSectionalMathematicalRanker
{
public:

    explicit CrossSectionalMathematicalRanker(
        std::size_t minimum_universe_size = 2);


    [[nodiscard]]
    std::size_t minimumUniverseSize() const noexcept;


    [[nodiscard]]
    std::vector<CrossSectionalMathematicalFeatures>
    rank(
        const std::vector<
            devai::statistics::MathematicalStateFeatures>&
            states) const;


public:

    enum class Metric
    {
        RETURN_VALUE,
        RETURN_Z_SCORE,
        RETURN_ROBUST_Z_SCORE,

        VOLUME_Z_SCORE,
        RVOL20_Z_SCORE,

        RELATIVE_RETURN,

        RESIDUAL_RETURN,
        RESIDUAL_Z_SCORE,
        RESIDUAL_ROBUST_Z_SCORE,

        ATR14_PERCENT,
        REALIZED_VOLATILITY_20,

        ROLLING_BETA,
        ROLLING_CORRELATION
    };


    enum class Horizon
    {
        ONE_MINUTE,
        FIVE_MINUTE,
        FIFTEEN_MINUTE
    };


    struct MetricValue
    {
        double value{
            crossSectionalNaN()
        };

        bool available{false};
    };


    CrossSectionalRanker
        ranker_;


    static void validateStates(
        const std::vector<
            devai::statistics::MathematicalStateFeatures>&
            states);


    [[nodiscard]]
    static MetricValue extractMetric(
        const devai::statistics::MathematicalStateFeatures&
            state,
        Horizon horizon,
        Metric metric);


    [[nodiscard]]
    std::vector<CrossSectionalRank>
    rankMetric(
        const std::vector<
            devai::statistics::MathematicalStateFeatures>&
            states,
        Horizon horizon,
        Metric metric) const;


    static void assignMetric(
        CrossSectionalMathematicalHorizon& output,
        Metric metric,
        const CrossSectionalRank& rank);
};

} // namespace devai::ranking