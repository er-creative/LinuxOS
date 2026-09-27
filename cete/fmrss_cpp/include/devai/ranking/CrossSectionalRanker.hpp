#pragma once

#include "devai/ranking/CrossSectionalRank.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>


namespace devai::ranking
{

class CrossSectionalRanker
{
public:

    explicit CrossSectionalRanker(
        std::size_t minimum_universe_size = 2);


    [[nodiscard]]
    std::size_t minimumUniverseSize() const noexcept;


    [[nodiscard]]
    std::vector<CrossSectionalRank> rank(
        const std::vector<CrossSectionalObservation>& observations,
        RankingDirection direction =
            RankingDirection::DESCENDING) const;


private:

    std::size_t minimum_universe_size_;


    static void validateInput(
        const std::vector<CrossSectionalObservation>& observations);


    [[nodiscard]]
    static bool sameValue(
        double lhs,
        double rhs) noexcept;


    [[nodiscard]]
    static double calculatePercentile(
        double ascending_average_rank,
        std::size_t universe_size) noexcept;


    [[nodiscard]]
    static double calculateCenteredPercentile(
        double percentile) noexcept;
};

} // namespace devai::ranking