#include "devai/ranking/CrossSectionalRanker.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>


namespace devai::ranking
{

namespace
{

struct WorkingObservation
{
    std::string symbol;

    std::int64_t decision_time{0};

    double value{
        crossSectionalNaN()
    };
};


struct RankedWorkingObservation
{
    WorkingObservation observation;

    double ascending_rank{
        crossSectionalNaN()
    };

    double requested_rank{
        crossSectionalNaN()
    };
};

} // namespace


// ============================================================================
// Constructor
// ============================================================================

CrossSectionalRanker::CrossSectionalRanker(
    std::size_t minimum_universe_size)
    :
    minimum_universe_size_(
        minimum_universe_size)
{
    if (minimum_universe_size_ < 2)
    {
        throw std::invalid_argument(
            "CrossSectionalRanker minimum universe size must be >= 2.");
    }
}


// ============================================================================
// Configuration
// ============================================================================

std::size_t
CrossSectionalRanker::minimumUniverseSize() const noexcept
{
    return minimum_universe_size_;
}


// ============================================================================
// Value equality
//
// Ranking values are expected to be deterministic outputs from CETE.
//
// A small relative tolerance prevents microscopic floating-point differences
// from breaking what is effectively a mathematical tie.
// ============================================================================

bool CrossSectionalRanker::sameValue(
    double lhs,
    double rhs) noexcept
{
    return lhs == rhs;
}


// ============================================================================
// Percentile
//
// ascending_average_rank:
//     1       = smallest numerical value
//     N       = largest numerical value
//
// percentile:
//     0       = smallest
//     1       = largest
//
// Tied values naturally receive the same percentile because they receive
// the same average ascending rank.
// ============================================================================

double CrossSectionalRanker::calculatePercentile(
    double ascending_average_rank,
    std::size_t universe_size) noexcept
{
    if (universe_size <= 1 ||
        !std::isfinite(
            ascending_average_rank))
    {
        return
            crossSectionalNaN();
    }


    return
        (
            ascending_average_rank -
            1.0
        )
        /
        static_cast<double>(
            universe_size - 1);
}


// ============================================================================
// Centered percentile
//
// [0, 1] -> [-1, +1]
// ============================================================================

double CrossSectionalRanker::calculateCenteredPercentile(
    double percentile) noexcept
{
    if (!std::isfinite(percentile))
    {
        return
            crossSectionalNaN();
    }


    return
        2.0 * percentile -
        1.0;
}


// ============================================================================
// Input validation
//
// Structural rules:
//
// 1. Non-empty symbols.
// 2. No duplicate symbols.
// 3. All observations must belong to the same decision time.
//
// Metric-specific unavailable observations are legal.
// Their values are ignored by the ranking calculation.
// ============================================================================

void CrossSectionalRanker::validateInput(
    const std::vector<CrossSectionalObservation>& observations)
{
    if (observations.empty())
    {
        return;
    }


    std::unordered_set<std::string>
        symbols;


    symbols.reserve(
        observations.size());


    const std::int64_t decision_time =
        observations.front().
            decision_time;


    for (const auto& observation :
         observations)
    {
        if (observation.symbol.empty())
        {
            throw std::invalid_argument(
                "Cross-sectional observation contains an empty symbol.");
        }


        if (!symbols.insert(
                observation.symbol).
                second)
        {
            throw std::invalid_argument(
                "Cross-sectional observations contain duplicate symbol: " +
                observation.symbol);
        }


        if (observation.decision_time !=
            decision_time)
        {
            throw std::invalid_argument(
                "Cross-sectional observations contain different decision times.");
        }


        if (observation.available &&
            !std::isfinite(
                observation.value))
        {
            throw std::invalid_argument(
                "Available cross-sectional observation contains "
                "non-finite value for symbol: " +
                observation.symbol);
        }
    }
}


// ============================================================================
// Rank
// ============================================================================

std::vector<CrossSectionalRank>
CrossSectionalRanker::rank(
    const std::vector<CrossSectionalObservation>& observations,
    RankingDirection direction) const
{
    validateInput(
        observations);


    std::vector<CrossSectionalRank>
        output;


    output.reserve(
        observations.size());


    if (observations.empty())
    {
        return output;
    }


    // ========================================================================
    // First preserve every symbol in output.
    //
    // Unavailable observations remain present but have:
    //
    // available      = false
    // rank           = NaN
    // percentile     = NaN
    // centered pct   = NaN
    //
    // universe_size is assigned after the eligible universe is known.
    // ========================================================================

    for (const auto& observation :
         observations)
    {
        CrossSectionalRank result;

        result.symbol =
            observation.symbol;

        result.decision_time =
            observation.decision_time;

        result.value =
            observation.value;

        result.available =
            false;

        output.push_back(
            std::move(result));
    }


    // ========================================================================
    // Build metric-specific eligible universe.
    // ========================================================================

    std::vector<WorkingObservation>
        eligible;


    eligible.reserve(
        observations.size());


    for (const auto& observation :
         observations)
    {
        if (!observation.available)
        {
            continue;
        }


        eligible.push_back(
            WorkingObservation{
                observation.symbol,
                observation.decision_time,
                observation.value
            });
    }


    const std::size_t universe_size =
        eligible.size();


    for (auto& result :
         output)
    {
        result.universe_size =
            universe_size;
    }


    // ========================================================================
    // Insufficient cross-sectional universe.
    //
    // We preserve all symbols but do not fabricate ranks.
    // ========================================================================

    if (universe_size <
        minimum_universe_size_)
    {
        return output;
    }


    // ========================================================================
    // Sort numerically ascending first.
    //
    // Symbol is used ONLY as deterministic ordering for equal values.
    // It does not determine rank because ties receive average rank.
    // ========================================================================

    std::sort(
        eligible.begin(),
        eligible.end(),
        [](
            const WorkingObservation& lhs,
            const WorkingObservation& rhs)
        {
            if (lhs.value <
                rhs.value)
            {
                return true;
            }


            if (lhs.value >
                rhs.value)
            {
                return false;
            }


            return
                lhs.symbol <
                rhs.symbol;
        });


    // ========================================================================
    // Assign average ascending ranks.
    //
    // Example:
    //
    // values:
    //     2, 4, 4, 5
    //
    // ascending positions:
    //     1, 2, 3, 4
    //
    // average ranks:
    //     1, 2.5, 2.5, 4
    // ========================================================================

    std::vector<RankedWorkingObservation>
        ranked;


    ranked.reserve(
        universe_size);


    std::size_t begin =
        0;


    while (begin <
           universe_size)
    {
        std::size_t end =
            begin + 1;


        while (
            end <
                universe_size &&

            sameValue(
                eligible[begin].value,
                eligible[end].value))
        {
            ++end;
        }


        const double first_rank =
            static_cast<double>(
                begin + 1);


        const double last_rank =
            static_cast<double>(
                end);


        const double average_ascending_rank =
            (
                first_rank +
                last_rank
            )
            /
            2.0;


        for (std::size_t index = begin;
             index < end;
             ++index)
        {
            double requested_rank =
                average_ascending_rank;


            if (direction ==
                RankingDirection::DESCENDING)
            {
                requested_rank =
                    static_cast<double>(
                        universe_size + 1)
                    -
                    average_ascending_rank;
            }


            ranked.push_back(
                RankedWorkingObservation{
                    eligible[index],
                    average_ascending_rank,
                    requested_rank
                });
        }


        begin =
            end;
    }


    // ========================================================================
    // Transfer ranking information back to the original symbol order.
    //
    // Phase 5 should not reorder the caller's universe implicitly.
    // ========================================================================

    for (const auto& ranked_observation :
         ranked)
    {
        const auto iterator =
            std::find_if(
                output.begin(),
                output.end(),
                [&ranked_observation](
                    const CrossSectionalRank& result)
                {
                    return
                        result.symbol ==
                        ranked_observation.
                            observation.symbol;
                });


        if (iterator ==
            output.end())
        {
            throw std::logic_error(
                "Internal cross-sectional ranking symbol mismatch.");
        }


        const double percentile =
            calculatePercentile(
                ranked_observation.
                    ascending_rank,
                universe_size);


        iterator->rank =
            ranked_observation.
                requested_rank;


        iterator->percentile =
            percentile;


        iterator->centered_percentile =
            calculateCenteredPercentile(
                percentile);


        iterator->available =
            true;
    }


    return output;
}

} // namespace devai::ranking