#include "devai/ranking/CrossSectionalRanker.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using devai::ranking::CrossSectionalObservation;
using devai::ranking::CrossSectionalRank;
using devai::ranking::CrossSectionalRanker;
using devai::ranking::RankingDirection;


// ============================================================================
// Test helpers
// ============================================================================

constexpr double EPSILON =
    1.0e-12;


void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(
            message);
    }
}


void requireNear(
    double actual,
    double expected,
    const std::string& message)
{
    if (!std::isfinite(actual) ||
        std::fabs(
            actual -
            expected) >
        EPSILON)
    {
        throw std::runtime_error(
            message +
            " | actual=" +
            std::to_string(actual) +
            " expected=" +
            std::to_string(expected));
    }
}


const CrossSectionalRank&
findSymbol(
    const std::vector<CrossSectionalRank>& results,
    const std::string& symbol)
{
    for (const auto& result :
         results)
    {
        if (result.symbol ==
            symbol)
        {
            return result;
        }
    }


    throw std::runtime_error(
        "Symbol not found in result: " +
        symbol);
}


// ============================================================================
// Test 1 — descending ranking
// ============================================================================

void testDescendingRanking()
{
    CrossSectionalRanker ranker;


    constexpr std::int64_t time =
        1000;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 10.0, true},
            {"BBB", time, 30.0, true},
            {"CCC", time, 20.0, true}
        };


    const auto results =
        ranker.rank(
            observations,
            RankingDirection::DESCENDING);


    require(
        results.size() == 3,
        "Descending ranking output size mismatch.");


    const auto& aaa =
        findSymbol(
            results,
            "AAA");


    const auto& bbb =
        findSymbol(
            results,
            "BBB");


    const auto& ccc =
        findSymbol(
            results,
            "CCC");


    requireNear(
        bbb.rank,
        1.0,
        "BBB descending rank mismatch.");


    requireNear(
        ccc.rank,
        2.0,
        "CCC descending rank mismatch.");


    requireNear(
        aaa.rank,
        3.0,
        "AAA descending rank mismatch.");


    // Numerical percentiles:
    //
    // AAA = lowest  -> 0.0
    // CCC = middle  -> 0.5
    // BBB = highest -> 1.0

    requireNear(
        aaa.percentile,
        0.0,
        "AAA percentile mismatch.");


    requireNear(
        ccc.percentile,
        0.5,
        "CCC percentile mismatch.");


    requireNear(
        bbb.percentile,
        1.0,
        "BBB percentile mismatch.");


    requireNear(
        aaa.centered_percentile,
        -1.0,
        "AAA centered percentile mismatch.");


    requireNear(
        ccc.centered_percentile,
        0.0,
        "CCC centered percentile mismatch.");


    requireNear(
        bbb.centered_percentile,
        1.0,
        "BBB centered percentile mismatch.");
}


// ============================================================================
// Test 2 — ascending ranking
// ============================================================================

void testAscendingRanking()
{
    CrossSectionalRanker ranker;


    constexpr std::int64_t time =
        1000;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 10.0, true},
            {"BBB", time, 30.0, true},
            {"CCC", time, 20.0, true}
        };


    const auto results =
        ranker.rank(
            observations,
            RankingDirection::ASCENDING);


    requireNear(
        findSymbol(
            results,
            "AAA").rank,
        1.0,
        "AAA ascending rank mismatch.");


    requireNear(
        findSymbol(
            results,
            "CCC").rank,
        2.0,
        "CCC ascending rank mismatch.");


    requireNear(
        findSymbol(
            results,
            "BBB").rank,
        3.0,
        "BBB ascending rank mismatch.");


    // Percentiles do NOT reverse with ranking direction.

    requireNear(
        findSymbol(
            results,
            "AAA").percentile,
        0.0,
        "AAA ascending percentile mismatch.");


    requireNear(
        findSymbol(
            results,
            "BBB").percentile,
        1.0,
        "BBB ascending percentile mismatch.");
}


// ============================================================================
// Test 3 — average-rank ties
// ============================================================================

void testAverageRankTies()
{
    CrossSectionalRanker ranker;


    constexpr std::int64_t time =
        2000;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 5.0, true},
            {"BBB", time, 4.0, true},
            {"CCC", time, 4.0, true},
            {"DDD", time, 2.0, true}
        };


    const auto results =
        ranker.rank(
            observations,
            RankingDirection::DESCENDING);


    requireNear(
        findSymbol(
            results,
            "AAA").rank,
        1.0,
        "AAA tie test rank mismatch.");


    requireNear(
        findSymbol(
            results,
            "BBB").rank,
        2.5,
        "BBB average tie rank mismatch.");


    requireNear(
        findSymbol(
            results,
            "CCC").rank,
        2.5,
        "CCC average tie rank mismatch.");


    requireNear(
        findSymbol(
            results,
            "DDD").rank,
        4.0,
        "DDD tie test rank mismatch.");


    requireNear(
        findSymbol(
            results,
            "BBB").percentile,
        0.5,
        "BBB tie percentile mismatch.");


    requireNear(
        findSymbol(
            results,
            "CCC").percentile,
        0.5,
        "CCC tie percentile mismatch.");
}


// ============================================================================
// Test 4 — unavailable observations
// ============================================================================

void testUnavailableObservation()
{
    CrossSectionalRanker ranker;


    constexpr std::int64_t time =
        3000;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 10.0, true},
            {"BBB", time, 0.0, false},
            {"CCC", time, 20.0, true}
        };


    const auto results =
        ranker.rank(
            observations,
            RankingDirection::DESCENDING);


    const auto& aaa =
        findSymbol(
            results,
            "AAA");


    const auto& bbb =
        findSymbol(
            results,
            "BBB");


    const auto& ccc =
        findSymbol(
            results,
            "CCC");


    require(
        aaa.universe_size == 2 &&
        bbb.universe_size == 2 &&
        ccc.universe_size == 2,
        "Metric-specific universe size mismatch.");


    require(
        !bbb.available,
        "Unavailable symbol unexpectedly ranked.");


    require(
        std::isnan(
            bbb.rank),
        "Unavailable symbol rank should be NaN.");


    require(
        std::isnan(
            bbb.percentile),
        "Unavailable symbol percentile should be NaN.");


    requireNear(
        ccc.rank,
        1.0,
        "CCC unavailable test rank mismatch.");


    requireNear(
        aaa.rank,
        2.0,
        "AAA unavailable test rank mismatch.");
}


// ============================================================================
// Test 5 — insufficient universe
// ============================================================================

void testInsufficientUniverse()
{
    CrossSectionalRanker ranker(
        3);


    constexpr std::int64_t time =
        4000;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 10.0, true},
            {"BBB", time, 20.0, true},
            {"CCC", time, 0.0, false}
        };


    const auto results =
        ranker.rank(
            observations);


    for (const auto& result :
         results)
    {
        require(
            result.universe_size == 2,
            "Insufficient-universe size mismatch.");


        require(
            !result.available,
            "Insufficient universe should not produce available ranks.");


        require(
            std::isnan(
                result.rank),
            "Insufficient universe rank should be NaN.");
    }
}


// ============================================================================
// Test 6 — decision-time mismatch
// ============================================================================

void testDecisionTimeMismatch()
{
    CrossSectionalRanker ranker;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", 5000, 10.0, true},
            {"BBB", 5001, 20.0, true}
        };


    bool threw =
        false;


    try
    {
        static_cast<void>(
            ranker.rank(
                observations));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Different decision times must be rejected.");
}


// ============================================================================
// Test 7 — duplicate symbol
// ============================================================================

void testDuplicateSymbol()
{
    CrossSectionalRanker ranker;


    constexpr std::int64_t time =
        6000;


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 10.0, true},
            {"AAA", time, 20.0, true}
        };


    bool threw =
        false;


    try
    {
        static_cast<void>(
            ranker.rank(
                observations));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Duplicate symbols must be rejected.");
}


// ============================================================================
// Test 8 — non-finite available value
// ============================================================================

void testNonFiniteAvailableValue()
{
    CrossSectionalRanker ranker;


    constexpr std::int64_t time =
        7000;


    const double nan_value =
        std::numeric_limits<double>::
            quiet_NaN();


    const std::vector<CrossSectionalObservation>
        observations{
            {"AAA", time, 10.0, true},
            {"BBB", time, nan_value, true}
        };


    bool threw =
        false;


    try
    {
        static_cast<void>(
            ranker.rank(
                observations));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Available NaN value must be rejected.");
}


// ============================================================================
// Main
// ============================================================================

} // namespace


int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 5.1 — CROSS-SECTIONAL RANKER TEST\n"
            << "============================================================\n";


        testDescendingRanking();

        std::cout
            << "Descending Ranking              : PASSED\n";


        testAscendingRanking();

        std::cout
            << "Ascending Ranking               : PASSED\n";


        testAverageRankTies();

        std::cout
            << "Average-Rank Ties               : PASSED\n";


        testUnavailableObservation();

        std::cout
            << "Unavailable Observation         : PASSED\n";


        testInsufficientUniverse();

        std::cout
            << "Insufficient Universe           : PASSED\n";


        testDecisionTimeMismatch();

        std::cout
            << "Decision-Time Integrity         : PASSED\n";


        testDuplicateSymbol();

        std::cout
            << "Duplicate-Symbol Protection     : PASSED\n";


        testNonFiniteAvailableValue();

        std::cout
            << "Non-Finite Protection           : PASSED\n";


        std::cout
            << "============================================================\n"
            << "PHASE 5.1 CROSS-SECTIONAL RANKER TEST PASSED\n"
            << "============================================================\n";


        return
            EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "PHASE 5.1 CROSS-SECTIONAL RANKER TEST FAILED\n"
            << exception.what()
            << '\n';


        return
            EXIT_FAILURE;
    }
}