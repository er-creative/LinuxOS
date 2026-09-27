#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"
#include "devai/ranking/CrossSectionalMathematicalStateComposer.hpp"

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
    CrossSectionalMathematicalRanker;

using devai::ranking::
    CrossSectionalMathematicalState;

using devai::ranking::
    CrossSectionalMathematicalStateComposer;

using devai::statistics::
    MathematicalStateFeatures;


// ============================================================================
// Helpers
// ============================================================================

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


const CrossSectionalMathematicalState&
findSymbol(
    const std::vector<
        CrossSectionalMathematicalState>&
        states,
    const std::string& symbol)
{
    for (const auto& state :
         states)
    {
        if (state.symbol ==
            symbol)
        {
            return state;
        }
    }


    throw std::runtime_error(
        "Symbol not found: " +
        symbol);
}


// ============================================================================
// Populate one Phase-4 horizon
// ============================================================================

void populateHorizon(
    devai::statistics::MathematicalHorizonState&
        horizon,
    double base)
{
    // ------------------------------------------------------------------------
    // Return
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
    // Volume
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
    // Stock / benchmark
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
    // Volatility evidence preserved by Phase 4.5
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
// Phase-4 state
// ============================================================================

MathematicalStateFeatures
makeState(
    const std::string& symbol,
    std::int64_t decision_time,
    double base)
{
    MathematicalStateFeatures
        state;


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
// Full multi-timeframe state
// ============================================================================

void testCompleteState()
{
    constexpr std::int64_t
        decision_time = 1000;


    std::vector<MathematicalStateFeatures>
        phase4_states;


    phase4_states.push_back(
        makeState(
            "AAA",
            decision_time,
            1.0));


    phase4_states.push_back(
        makeState(
            "BBB",
            decision_time,
            3.0));


    phase4_states.push_back(
        makeState(
            "CCC",
            decision_time,
            2.0));


    CrossSectionalMathematicalRanker
        ranker;


    const auto phase52 =
        ranker.rank(
            phase4_states);


    CrossSectionalMathematicalStateComposer
        composer;


    const auto phase53 =
        composer.compose(
            phase52);


    require(
        phase53.size() == 3,
        "Phase-5.3 output size mismatch.");


    const auto& aaa =
        findSymbol(
            phase53,
            "AAA");


    const auto& bbb =
        findSymbol(
            phase53,
            "BBB");


    const auto& ccc =
        findSymbol(
            phase53,
            "CCC");


    require(
        aaa.one_minute_ready &&
        aaa.five_minute_ready &&
        aaa.fifteen_minute_ready &&
        aaa.fully_ready,
        "AAA should be fully ready.");


    require(
        bbb.one_minute_ready &&
        bbb.five_minute_ready &&
        bbb.fifteen_minute_ready &&
        bbb.fully_ready,
        "BBB should be fully ready.");


    require(
        ccc.one_minute_ready &&
        ccc.five_minute_ready &&
        ccc.fifteen_minute_ready &&
        ccc.fully_ready,
        "CCC should be fully ready.");


    require(
        bbb.five_minute.
            rankings.
            return_value.
            rank == 1.0,
        "Lossless 5m return rank mismatch.");


    require(
        bbb.five_minute.
            rankings.
            residual_z_score.
            rank == 1.0,
        "Lossless 5m residual-Z rank mismatch.");


    require(
        aaa.five_minute.
            rankings.
            return_value.
            rank == 3.0,
        "AAA return rank mismatch.");


    require(
        ccc.five_minute.
            rankings.
            return_value.
            rank == 2.0,
        "CCC return rank mismatch.");


    require(
        bbb.five_minute.
            readiness.
            available_metric_count == 13,
        "5m available metric count mismatch.");


    require(
        bbb.five_minute.
            readiness.
            total_metric_count == 13,
        "5m total metric count mismatch.");


    require(
        bbb.numerical_integrity,
        "Complete state numerical integrity failed.");
}


// ============================================================================
// Partial metric readiness
// ============================================================================

void testPartialMetricReadiness()
{
    constexpr std::int64_t
        decision_time = 2000;


    auto aaa =
        makeState(
            "AAA",
            decision_time,
            1.0);


    auto bbb =
        makeState(
            "BBB",
            decision_time,
            3.0);


    auto ccc =
        makeState(
            "CCC",
            decision_time,
            2.0);


    // Remove only CCC's 5m residual-statistical readiness.

    ccc.five_minute.
        stock_benchmark_statistics.
        residual_statistics_ready =
        false;


    std::vector<MathematicalStateFeatures>
        phase4_states{
            aaa,
            bbb,
            ccc
        };


    CrossSectionalMathematicalRanker
        ranker;


    const auto phase52 =
        ranker.rank(
            phase4_states);


    CrossSectionalMathematicalStateComposer
        composer;


    const auto phase53 =
        composer.compose(
            phase52);


    const auto& result =
        findSymbol(
            phase53,
            "CCC");


    require(
        !result.five_minute.
            readiness.
            residual_z_score.
            available,
        "CCC 5m residual-Z should be unavailable.");


    require(
        !result.five_minute.
            readiness.
            residual_robust_z_score.
            available,
        "CCC 5m robust residual-Z should be unavailable.");


    require(
        result.five_minute.
            readiness.
            return_z_score.
            available,
        "CCC 5m return-Z should remain available.");


    require(
        result.five_minute.
            readiness.
            residual_z_score.
            universe_size == 2,
        "Residual-Z universe size mismatch.");


    require(
        result.five_minute.
            readiness.
            return_z_score.
            universe_size == 3,
        "Return-Z universe size mismatch.");


    require(
        result.five_minute.
            readiness.
            available_metric_count == 11,
        "CCC should have 11 available 5m metrics.");


    require(
        !result.five_minute_ready,
        "CCC 5m should not be fully ready.");


    require(
        result.one_minute_ready,
        "CCC 1m should remain ready.");


    require(
        result.fifteen_minute_ready,
        "CCC 15m should remain ready.");


    require(
        result.numerical_integrity,
        "Unavailable metric must not cause "
        "numerical-integrity failure.");


    require(
        !result.fully_ready,
        "CCC complete multi-timeframe state "
        "must not be fully ready.");
}


// ============================================================================
// Corrupted rank protection
// ============================================================================

void testNumericalIntegrityProtection()
{
    constexpr std::int64_t
        decision_time = 3000;


    std::vector<MathematicalStateFeatures>
        phase4_states{
            makeState(
                "AAA",
                decision_time,
                1.0),

            makeState(
                "BBB",
                decision_time,
                2.0)
        };


    CrossSectionalMathematicalRanker
        ranker;


    auto phase52 =
        ranker.rank(
            phase4_states);


    // Deliberately corrupt one available rank.

    phase52[0].
        five_minute.
        return_z_score.
        percentile =
        2.0;


    CrossSectionalMathematicalStateComposer
        composer;


    const auto phase53 =
        composer.compose(
            phase52);


    const auto& aaa =
        findSymbol(
            phase53,
            "AAA");


    require(
        !aaa.five_minute.
            readiness.
            numerical_integrity,
        "Corrupted percentile must fail integrity.");


    require(
        !aaa.numerical_integrity,
        "Global numerical integrity must fail.");


    require(
        !aaa.fully_ready,
        "Corrupted state must not be fully ready.");
}


// ============================================================================
// Decision-time protection
// ============================================================================

void testDecisionTimeProtection()
{
    constexpr std::int64_t
        decision_time = 4000;


    std::vector<MathematicalStateFeatures>
        phase4_states{
            makeState(
                "AAA",
                decision_time,
                1.0),

            makeState(
                "BBB",
                decision_time,
                2.0)
        };


    CrossSectionalMathematicalRanker
        ranker;


    auto phase52 =
        ranker.rank(
            phase4_states);


    phase52[1].
        decision_time =
        decision_time + 1;


    CrossSectionalMathematicalStateComposer
        composer;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            composer.compose(
                phase52));
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
    constexpr std::int64_t
        decision_time = 5000;


    std::vector<MathematicalStateFeatures>
        phase4_states{
            makeState(
                "AAA",
                decision_time,
                1.0),

            makeState(
                "BBB",
                decision_time,
                2.0)
        };


    CrossSectionalMathematicalRanker
        ranker;


    auto phase52 =
        ranker.rank(
            phase4_states);


    phase52[1].
        symbol =
        "AAA";


    CrossSectionalMathematicalStateComposer
        composer;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            composer.compose(
                phase52));
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
            << "PHASE 5.3 — MULTI-TIMEFRAME CROSS-SECTIONAL STATE TEST\n"
            << "============================================================\n";


        testCompleteState();

        std::cout
            << "Complete Multi-Timeframe State   : PASSED\n";


        testPartialMetricReadiness();

        std::cout
            << "Metric-Specific Readiness        : PASSED\n";


        testNumericalIntegrityProtection();

        std::cout
            << "Numerical-Integrity Protection   : PASSED\n";


        testDecisionTimeProtection();

        std::cout
            << "Decision-Time Integrity          : PASSED\n";


        testDuplicateSymbolProtection();

        std::cout
            << "Duplicate-Symbol Protection      : PASSED\n";


        std::cout
            << "============================================================\n"
            << "PHASE 5.3 MULTI-TIMEFRAME CROSS-SECTIONAL STATE TEST PASSED\n"
            << "============================================================\n";


        return
            EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "PHASE 5.3 MULTI-TIMEFRAME CROSS-SECTIONAL STATE TEST FAILED\n"
            << exception.what()
            << '\n';


        return
            EXIT_FAILURE;
    }
}