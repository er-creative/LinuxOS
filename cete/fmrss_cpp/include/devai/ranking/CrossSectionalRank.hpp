#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>


namespace devai::ranking
{

inline double crossSectionalNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// Ranking direction
//
// ASCENDING:
//     Smallest value receives rank 1.
//
// DESCENDING:
//     Largest value receives rank 1.
//
// Important:
//     This describes ordering only.
//     It does NOT mean "best trade".
// ============================================================================

enum class RankingDirection
{
    ASCENDING,
    DESCENDING
};


// ============================================================================
// One input observation
// ============================================================================

struct CrossSectionalObservation
{
    std::string symbol;

    std::int64_t decision_time{0};

    double value{
        crossSectionalNaN()
    };

    bool available{false};
};


// ============================================================================
// Ranking result for one symbol
//
// rank:
//     Average rank is used for ties.
//     Therefore rank is double rather than size_t.
//
// percentile:
//     0.0 = lowest numerical value
//     1.0 = highest numerical value
//
// centered_percentile:
//    -1.0 = lowest numerical value
//     0.0 = center
//    +1.0 = highest numerical value
//
// Note:
//     Percentiles preserve numerical position independently of whether
//     ranking direction is ASCENDING or DESCENDING.
// ============================================================================

struct CrossSectionalRank
{
    std::string symbol;

    std::int64_t decision_time{0};

    double value{
        crossSectionalNaN()
    };

    double rank{
        crossSectionalNaN()
    };

    std::size_t universe_size{0};

    double percentile{
        crossSectionalNaN()
    };

    double centered_percentile{
        crossSectionalNaN()
    };

    bool available{false};
};

} // namespace devai::ranking