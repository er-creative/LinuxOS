#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"
#include "devai/ranking/CrossSectionalMathematicalStateComposer.hpp"
#include "devai/ranking/CrossSectionalUniverseSnapshotBuilder.hpp"

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
    CrossSectionalMathematicalStateComposer;

using devai::ranking::
    CrossSectionalUniverseSnapshot;

using devai::ranking::
    CrossSectionalUniverseSnapshotBuilder;

using devai::statistics::
    MathematicalStateFeatures;


// ============================================================================
// Assertions
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


// ============================================================================
// Populate one Phase-4 horizon
// ============================================================================

void populateHorizon(
    devai::statistics::MathematicalHorizonState&
        horizon,
    double base)
{
    // Return

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


    // Volume

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


    // Stock / benchmark

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


    // Volatility evidence

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
// Build complete Phase-5.4 pipeline
// ============================================================================

CrossSectionalUniverseSnapshot
buildSnapshot(
    std::vector<MathematicalStateFeatures>
        phase4_states)
{
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


    CrossSectionalUniverseSnapshotBuilder
        builder;


    return
        builder.build(
            phase53);
}


// ============================================================================
// Complete universe
// ============================================================================

void testCompleteUniverse()
{
    constexpr std::int64_t
        decision_time = 1000;


    std::vector<MathematicalStateFeatures>
        states;


    // Deliberately non-alphabetical input order.

    states.push_back(
        makeState(
            "CCC",
            decision_time,
            2.0));


    states.push_back(
        makeState(
            "AAA",
            decision_time,
            1.0));


    states.push_back(
        makeState(
            "BBB",
            decision_time,
            3.0));


    const auto snapshot =
        buildSnapshot(
            states);


    require(
        snapshot.decision_time ==
            decision_time,
        "Universe decision time mismatch.");


    require(
        snapshot.benchmark_symbol ==
            "NIFTY%2050",
        "Universe benchmark mismatch.");


    require(
        snapshot.stock_count == 3,
        "Universe stock count mismatch.");


    require(
        snapshot.stocks.size() == 3,
        "Universe state count mismatch.");


    // Deterministic alphabetical ordering.

    require(
        snapshot.stocks[0].symbol ==
            "AAA",
        "First symbol must be AAA.");


    require(
        snapshot.stocks[1].symbol ==
            "BBB",
        "Second symbol must be BBB.");


    require(
        snapshot.stocks[2].symbol ==
            "CCC",
        "Third symbol must be CCC.");


    require(
        snapshot.fully_ready_stock_count ==
            3,
        "Fully-ready stock count mismatch.");


    require(
        snapshot.partially_ready_stock_count ==
            0,
        "Partial stock count mismatch.");


    require(
        snapshot.numerical_integrity,
        "Complete universe integrity failed.");


    require(
        snapshot.fully_ready,
        "Complete universe should be fully ready.");


    // Every metric should have all three stocks.

    require(
        snapshot.five_minute_universe_sizes.
            return_z_score == 3,
        "5m return-Z universe mismatch.");


    require(
        snapshot.five_minute_universe_sizes.
            residual_z_score == 3,
        "5m residual-Z universe mismatch.");


    require(
        snapshot.fifteen_minute_universe_sizes.
            volume_z_score == 3,
        "15m volume-Z universe mismatch.");


    // Ranking itself must remain unchanged after deterministic sorting.
    //
    // BBB has highest base value.

    require(
        snapshot.stocks[1].
            five_minute.
            rankings.
            return_value.
            rank == 1.0,
        "BBB ranking was changed by "
        "universe snapshot builder.");
}


// ============================================================================
// Metric-specific universe
// ============================================================================

void testMetricSpecificUniverse()
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


    // CCC loses only 5m residual statistics.

    ccc.five_minute.
        stock_benchmark_statistics.
        residual_statistics_ready =
        false;


    const auto snapshot =
        buildSnapshot(
            {
                aaa,
                bbb,
                ccc
            });


    require(
        snapshot.five_minute_universe_sizes.
            residual_z_score == 2,
        "5m residual-Z universe should be 2.");


    require(
        snapshot.five_minute_universe_sizes.
            residual_robust_z_score == 2,
        "5m robust residual-Z universe should be 2.");


    require(
        snapshot.five_minute_universe_sizes.
            return_z_score == 3,
        "5m return-Z universe should remain 3.");


    require(
        snapshot.five_minute_universe_sizes.
            volume_z_score == 3,
        "5m volume-Z universe should remain 3.");


    require(
        snapshot.fully_ready_stock_count ==
            2,
        "Expected two fully-ready stocks.");


    require(
        snapshot.partially_ready_stock_count ==
            1,
        "Expected one partially-ready stock.");


    require(
        snapshot.numerical_integrity,
        "Partial metric availability must not "
        "fail universe integrity.");


    require(
        !snapshot.fully_ready,
        "Partial universe must not be "
        "fully ready.");
}


// ============================================================================
// Benchmark exclusion
// ============================================================================

void testBenchmarkExclusion()
{
    constexpr std::int64_t
        decision_time = 3000;


    auto benchmark =
        makeState(
            "NIFTY%2050",
            decision_time,
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
        "Benchmark must be excluded from "
        "ranked stock universe.");
}


// ============================================================================
// Decision-time protection
// ============================================================================

void testDecisionTimeProtection()
{
    constexpr std::int64_t
        decision_time = 4000;


    CrossSectionalMathematicalRanker
        ranker;


    const auto phase52 =
        ranker.rank(
            {
                makeState(
                    "AAA",
                    decision_time,
                    1.0),

                makeState(
                    "BBB",
                    decision_time,
                    2.0)
            });


    CrossSectionalMathematicalStateComposer
        composer;


    auto phase53 =
        composer.compose(
            phase52);


    phase53[1].
        decision_time =
        decision_time + 1;


    CrossSectionalUniverseSnapshotBuilder
        builder;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            builder.build(
                phase53));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Different decision times must "
        "be rejected.");
}


// ============================================================================
// Universe-size consistency
// ============================================================================

void testUniverseSizeConsistency()
{
    constexpr std::int64_t
        decision_time = 5000;


    CrossSectionalMathematicalRanker
        ranker;


    const auto phase52 =
        ranker.rank(
            {
                makeState(
                    "AAA",
                    decision_time,
                    1.0),

                makeState(
                    "BBB",
                    decision_time,
                    2.0),

                makeState(
                    "CCC",
                    decision_time,
                    3.0)
            });


    CrossSectionalMathematicalStateComposer
        composer;


    auto phase53 =
        composer.compose(
            phase52);


    // Corrupt one metric's recorded universe size.

    phase53[0].
        five_minute.
        rankings.
        return_z_score.
        universe_size =
        2;


    CrossSectionalUniverseSnapshotBuilder
        builder;


    bool threw =
        false;


    try
    {
        static_cast<void>(
            builder.build(
                phase53));
    }
    catch (const std::runtime_error&)
    {
        threw =
            true;
    }


    require(
        threw,
        "Inconsistent metric universe sizes "
        "must be rejected.");
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
            << "PHASE 5.4 — CROSS-SECTIONAL UNIVERSE SNAPSHOT TEST\n"
            << "============================================================\n";


        testCompleteUniverse();

        std::cout
            << "Complete Universe Snapshot       : PASSED\n";


        testMetricSpecificUniverse();

        std::cout
            << "Metric-Specific Universe         : PASSED\n";


        testBenchmarkExclusion();

        std::cout
            << "Benchmark Exclusion              : PASSED\n";


        testDecisionTimeProtection();

        std::cout
            << "Decision-Time Integrity          : PASSED\n";


        testUniverseSizeConsistency();

        std::cout
            << "Universe-Size Consistency        : PASSED\n";


        std::cout
            << "============================================================\n"
            << "PHASE 5.4 CROSS-SECTIONAL UNIVERSE SNAPSHOT TEST PASSED\n"
            << "============================================================\n";


        return
            EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "PHASE 5.4 CROSS-SECTIONAL UNIVERSE SNAPSHOT TEST FAILED\n"
            << exception.what()
            << '\n';


        return
            EXIT_FAILURE;
    }
}