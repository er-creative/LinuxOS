#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using devai::ranking::
    CrossSectionalMathematicalFeatures;

using devai::ranking::
    CrossSectionalMathematicalRanker;

using devai::statistics::
    MathematicalStateFeatures;


// ============================================================================
// Helpers
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
            message);
    }
}


const CrossSectionalMathematicalFeatures&
findSymbol(
    const std::vector<
        CrossSectionalMathematicalFeatures>&
        results,
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
        "Symbol not found: " +
        symbol);
}


// ============================================================================
// Populate one horizon with deterministic artificial Phase-4 values.
// ============================================================================

void populateHorizon(
    devai::statistics::MathematicalHorizonState& horizon,
    double base)
{
    // ------------------------------------------------------------------------
    // Return statistics
    // ------------------------------------------------------------------------

    horizon.return_statistics.
        return_value =
        base;

    horizon.return_statistics.
        z_score =
        base + 1.0;

    horizon.return_statistics.
        robust_z_score =
        base + 2.0;

    horizon.return_statistics.
        has_observation =
        true;

    horizon.return_statistics.
        ready =
        true;


    // ------------------------------------------------------------------------
    // Volume statistics
    // ------------------------------------------------------------------------

    horizon.volume_statistics.
        volume.value =
        1000.0 + base;

    horizon.volume_statistics.
        volume.z_score =
        base + 3.0;

    horizon.volume_statistics.
        volume.has_observation =
        true;

    horizon.volume_statistics.
        volume.ready =
        true;


    horizon.volume_statistics.
        rvol20.value =
        1.0 + base;

    horizon.volume_statistics.
        rvol20.z_score =
        base + 4.0;

    horizon.volume_statistics.
        rvol20.has_observation =
        true;

    horizon.volume_statistics.
        rvol20.ready =
        true;


    // ------------------------------------------------------------------------
    // Stock / benchmark statistics
    // ------------------------------------------------------------------------

    horizon.stock_benchmark_statistics.
        relative_return =
        base + 5.0;

    horizon.stock_benchmark_statistics.
        residual_return =
        base + 6.0;

    horizon.stock_benchmark_statistics.
        residual_z_score =
        base + 7.0;

    horizon.stock_benchmark_statistics.
        residual_robust_z_score =
        base + 8.0;

    horizon.stock_benchmark_statistics.
        rolling_beta =
        base + 9.0;

    horizon.stock_benchmark_statistics.
        rolling_correlation =
        0.10 * base;

    horizon.stock_benchmark_statistics.
        has_observation =
        true;

    horizon.stock_benchmark_statistics.
        regression_ready =
        true;

    horizon.stock_benchmark_statistics.
        residual_statistics_ready =
        true;

    horizon.stock_benchmark_statistics.
        fully_ready =
        true;


    // ------------------------------------------------------------------------
    // Market regime numerical evidence
    // ------------------------------------------------------------------------

    horizon.market_regime.
        atr14_percent =
        base + 10.0;

    horizon.market_regime.
        realized_volatility_20 =
        base + 11.0;

    horizon.market_regime.
        volatility_ready =
        true;
}


// ============================================================================
// Artificial Phase-4 state
// ============================================================================

MathematicalStateFeatures makeState(
    const std::string& symbol,
    std::int64_t decision_time,
    double base)
{
    MathematicalStateFeatures state;


    state.stock_symbol =
        symbol;


    state.benchmark_symbol =
        "NIFTY%2050";


    state.decision_time =
        decision_time;


    populateHorizon(
        state.one_minute,
        base);


    populateHorizon(
        state.five_minute,
        base);


    populateHorizon(
        state.fifteen_minute,
        base);


    return state;
}


// ============================================================================
// Test complete ranking
// ============================================================================

void testCompleteRanking()
{
    constexpr std::int64_t time =
        1000;


    std::vector<MathematicalStateFeatures>
        states;


    states.push_back(
        makeState(
            "AAA",
            time,
            1.0));


    states.push_back(
        makeState(
            "BBB",
            time,
            3.0));


    states.push_back(
        makeState(
            "CCC",
            time,
            2.0));


    CrossSectionalMathematicalRanker
        ranker;


    const auto result =
        ranker.rank(
            states);


    require(
        result.size() == 3,
        "Output size mismatch.");


    const auto& aaa =
        findSymbol(
            result,
            "AAA");


    const auto& bbb =
        findSymbol(
            result,
            "BBB");


    const auto& ccc =
        findSymbol(
            result,
            "CCC");


    // Highest numerical value -> descending rank 1.

    requireNear(
        bbb.five_minute.
            return_value.rank,
        1.0,
        "BBB return rank mismatch.");


    requireNear(
        ccc.five_minute.
            return_value.rank,
        2.0,
        "CCC return rank mismatch.");


    requireNear(
        aaa.five_minute.
            return_value.rank,
        3.0,
        "AAA return rank mismatch.");


    requireNear(
        bbb.five_minute.
            return_value.percentile,
        1.0,
        "BBB percentile mismatch.");


    requireNear(
        ccc.five_minute.
            return_value.percentile,
        0.5,
        "CCC percentile mismatch.");


    requireNear(
        aaa.five_minute.
            return_value.percentile,
        0.0,
        "AAA percentile mismatch.");


    // Check another independent metric.

    requireNear(
        bbb.five_minute.
            residual_z_score.rank,
        1.0,
        "BBB residual-Z rank mismatch.");


    requireNear(
        aaa.five_minute.
            residual_z_score.rank,
        3.0,
        "AAA residual-Z rank mismatch.");


    // Check another timeframe.

    requireNear(
        bbb.fifteen_minute.
            volume_z_score.rank,
        1.0,
        "BBB 15m volume-Z rank mismatch.");
}


// ============================================================================
// Metric-specific availability
// ============================================================================

void testMetricSpecificAvailability()
{
    constexpr std::int64_t time =
        2000;


    auto aaa =
        makeState(
            "AAA",
            time,
            1.0);


    auto bbb =
        makeState(
            "BBB",
            time,
            3.0);


    auto ccc =
        makeState(
            "CCC",
            time,
            2.0);


    // CCC loses only the 5m residual-statistical metric.

    ccc.five_minute.
        stock_benchmark_statistics.
        residual_statistics_ready =
        false;


    std::vector<MathematicalStateFeatures>
        states{
            aaa,
            bbb,
            ccc
        };


    CrossSectionalMathematicalRanker
        ranker;


    const auto result =
        ranker.rank(
            states);


    const auto& result_ccc =
        findSymbol(
            result,
            "CCC");


    // Residual Z excludes CCC.

    require(
        !result_ccc.
            five_minute.
            residual_z_score.
            available,
        "CCC residual Z should be unavailable.");


    require(
        result_ccc.
            five_minute.
            residual_z_score.
            universe_size == 2,
        "Residual-Z universe should contain 2 stocks.");


    // Return ranking still contains CCC.

    require(
        result_ccc.
            five_minute.
            return_z_score.
            available,
        "CCC return Z should remain available.");


    require(
        result_ccc.
            five_minute.
            return_z_score.
            universe_size == 3,
        "Return-Z universe should contain 3 stocks.");
}


// ============================================================================
// Decision-time protection
// ============================================================================

void testDecisionTimeProtection()
{
    std::vector<MathematicalStateFeatures>
        states;


    states.push_back(
        makeState(
            "AAA",
            3000,
            1.0));


    states.push_back(
        makeState(
            "BBB",
            3001,
            2.0));


    CrossSectionalMathematicalRanker
        ranker;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            ranker.rank(
                states));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Decision-time mismatch must be rejected.");
}


// ============================================================================
// Duplicate-symbol protection
// ============================================================================

void testDuplicateSymbolProtection()
{
    constexpr std::int64_t time =
        4000;


    std::vector<MathematicalStateFeatures>
        states;


    states.push_back(
        makeState(
            "AAA",
            time,
            1.0));


    states.push_back(
        makeState(
            "AAA",
            time,
            2.0));


    CrossSectionalMathematicalRanker
        ranker;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            ranker.rank(
                states));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Duplicate stock symbol must be rejected.");
}


// ============================================================================
// Benchmark protection
// ============================================================================

void testBenchmarkProtection()
{
    constexpr std::int64_t time =
        5000;


    auto benchmark =
        makeState(
            "NIFTY%2050",
            time,
            1.0);


    CrossSectionalMathematicalRanker
        ranker;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            ranker.rank(
                {benchmark}));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Benchmark must not be ranked as its own stock.");
}

} // namespace


// ============================================================================
// Main
// ============================================================================

int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "PHASE 5.2 — CROSS-SECTIONAL MATHEMATICAL RANKER TEST\n"
            << "============================================================\n";


        testCompleteRanking();

        std::cout
            << "Complete Mathematical Ranking    : PASSED\n";


        testMetricSpecificAvailability();

        std::cout
            << "Metric-Specific Availability     : PASSED\n";


        testDecisionTimeProtection();

        std::cout
            << "Decision-Time Integrity          : PASSED\n";


        testDuplicateSymbolProtection();

        std::cout
            << "Duplicate-Symbol Protection      : PASSED\n";


        testBenchmarkProtection();

        std::cout
            << "Benchmark Exclusion              : PASSED\n";


        std::cout
            << "============================================================\n"
            << "PHASE 5.2 CROSS-SECTIONAL MATHEMATICAL RANKER TEST PASSED\n"
            << "============================================================\n";


        return
            EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "PHASE 5.2 CROSS-SECTIONAL MATHEMATICAL RANKER TEST FAILED\n"
            << exception.what()
            << '\n';


        return
            EXIT_FAILURE;
    }
}