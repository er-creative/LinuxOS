#include "devai/strategy/dev_ai_strategy/BaselineDevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalExperienceGenerator.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include "devai/ranking/CrossSectionalMathematicalState.hpp"
#include "devai/ranking/CrossSectionalRank.hpp"
#include "devai/ranking/CrossSectionalUniverseSnapshot.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using namespace devai::strategy;
using namespace devai::ranking;


// ============================================================================
// Test statistics
// ============================================================================

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};
};


// ============================================================================
// require
// ============================================================================

void require(
    const bool condition,
    const std::string& description,
    TestStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << description
            << '\n';
    }
    else
    {
        ++stats.failures;

        std::cerr
            << "[FAIL] "
            << description
            << '\n';
    }
}


// ============================================================================
// expectThrow
// ============================================================================

template <typename Function>
void expectThrow(
    Function&& function,
    const std::string& description,
    TestStats& stats)
{
    bool threw =
        false;

    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw =
            true;
    }

    require(
        threw,
        description,
        stats);
}


// ============================================================================
// approximatelyEqual
// ============================================================================

bool approximatelyEqual(
    const double left,
    const double right,
    const double tolerance = 1.0e-12)
{
    return
        std::fabs(left - right) <=
        tolerance;
}


// ============================================================================
// makeRank
// ============================================================================

CrossSectionalRank makeRank(
    const std::string& symbol,
    const std::int64_t decision_time,
    const double centered_percentile,
    const std::size_t universe_size = 2)
{
    CrossSectionalRank rank;

    rank.symbol =
        symbol;

    rank.decision_time =
        decision_time;

    rank.value =
        centered_percentile;

    rank.rank =
        centered_percentile >= 0.0
            ? 1.0
            : 2.0;

    rank.universe_size =
        universe_size;

    rank.percentile =
        (centered_percentile + 1.0) /
        2.0;

    rank.centered_percentile =
        centered_percentile;

    rank.available =
        true;

    return rank;
}


// ============================================================================
// populateHorizon
// ============================================================================

void populateHorizon(
    CrossSectionalHorizonState& horizon,
    const std::string& symbol,
    const std::int64_t decision_time,
    const double base_value)
{
    auto& rankings =
        horizon.rankings;


    rankings.return_value =
        makeRank(
            symbol,
            decision_time,
            base_value);

    rankings.return_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.01);

    rankings.return_robust_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.02);

    rankings.volume_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.03);

    rankings.rvol20_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.04);

    rankings.relative_return =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.05);

    rankings.residual_return =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.06);

    rankings.residual_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.07);

    rankings.residual_robust_z_score =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.08);

    rankings.atr14_percent =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.09);

    rankings.realized_volatility_20 =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.10);

    rankings.rolling_beta =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.11);

    rankings.rolling_correlation =
        makeRank(
            symbol,
            decision_time,
            base_value + 0.12);


    auto& readiness =
        horizon.readiness;


    readiness.return_value =
        {true, 2};

    readiness.return_z_score =
        {true, 2};

    readiness.return_robust_z_score =
        {true, 2};

    readiness.volume_z_score =
        {true, 2};

    readiness.rvol20_z_score =
        {true, 2};

    readiness.relative_return =
        {true, 2};

    readiness.residual_return =
        {true, 2};

    readiness.residual_z_score =
        {true, 2};

    readiness.residual_robust_z_score =
        {true, 2};

    readiness.atr14_percent =
        {true, 2};

    readiness.realized_volatility_20 =
        {true, 2};

    readiness.rolling_beta =
        {true, 2};

    readiness.rolling_correlation =
        {true, 2};


    readiness.available_metric_count =
        13;

    readiness.total_metric_count =
        13;

    readiness.numerical_integrity =
        true;

    readiness.fully_ready =
        true;
}


// ============================================================================
// makeStockState
// ============================================================================

CrossSectionalMathematicalState makeStockState(
    const std::string& symbol,
    const std::int64_t decision_time,
    const double base_value)
{
    CrossSectionalMathematicalState state;

    state.symbol =
        symbol;

    state.benchmark_symbol =
        "NIFTY%2050";

    state.decision_time =
        decision_time;


    populateHorizon(
        state.one_minute,
        symbol,
        decision_time,
        base_value);

    populateHorizon(
        state.five_minute,
        symbol,
        decision_time,
        base_value + 0.10);

    populateHorizon(
        state.fifteen_minute,
        symbol,
        decision_time,
        base_value + 0.20);


    state.one_minute_ready =
        true;

    state.five_minute_ready =
        true;

    state.fifteen_minute_ready =
        true;

    state.numerical_integrity =
        true;

    state.fully_ready =
        true;


    return state;
}


// ============================================================================
// populateUniverseSizes
// ============================================================================

void populateUniverseSizes(
    CrossSectionalMetricUniverseSizes& sizes,
    const std::size_t universe_size)
{
    sizes.return_value =
        universe_size;

    sizes.return_z_score =
        universe_size;

    sizes.return_robust_z_score =
        universe_size;

    sizes.volume_z_score =
        universe_size;

    sizes.rvol20_z_score =
        universe_size;

    sizes.relative_return =
        universe_size;

    sizes.residual_return =
        universe_size;

    sizes.residual_z_score =
        universe_size;

    sizes.residual_robust_z_score =
        universe_size;

    sizes.atr14_percent =
        universe_size;

    sizes.realized_volatility_20 =
        universe_size;

    sizes.rolling_beta =
        universe_size;

    sizes.rolling_correlation =
        universe_size;
}


// ============================================================================
// makeSnapshot
// ============================================================================

CrossSectionalUniverseSnapshot makeSnapshot(
    const std::int64_t decision_time,
    const double shift)
{
    CrossSectionalUniverseSnapshot snapshot;

    snapshot.decision_time =
        decision_time;

    snapshot.benchmark_symbol =
        "NIFTY%2050";


    snapshot.stocks.push_back(
        makeStockState(
            "HDFCBANK",
            decision_time,
            -0.50 + shift));


    snapshot.stocks.push_back(
        makeStockState(
            "RELIANCE",
            decision_time,
            0.20 + shift));


    snapshot.stock_count =
        snapshot.stocks.size();

    snapshot.fully_ready_stock_count =
        snapshot.stock_count;

    snapshot.partially_ready_stock_count =
        0;


    populateUniverseSizes(
        snapshot.one_minute_universe_sizes,
        snapshot.stock_count);

    populateUniverseSizes(
        snapshot.five_minute_universe_sizes,
        snapshot.stock_count);

    populateUniverseSizes(
        snapshot.fifteen_minute_universe_sizes,
        snapshot.stock_count);


    snapshot.numerical_integrity =
        true;

    snapshot.fully_ready =
        true;


    return snapshot;
}


// ============================================================================
// makeEpisode
//
// Five states produce four complete transitions.
// ============================================================================

DevEpisode makeEpisode()
{
    constexpr std::int64_t T0 =
        1'000'000;


    std::vector<DevEpisode::Snapshot>
        snapshots;


    snapshots.push_back(
        makeSnapshot(
            T0,
            0.00));

    snapshots.push_back(
        makeSnapshot(
            T0 + 60,
            0.01));

    snapshots.push_back(
        makeSnapshot(
            T0 + 120,
            0.02));

    snapshots.push_back(
        makeSnapshot(
            T0 + 180,
            0.03));

    snapshots.push_back(
        makeSnapshot(
            T0 + 240,
            0.04));


    DevEpisodeConfig config;

    config.minimum_universe_size =
        2;


    return DevEpisode(
        std::move(snapshots),
        config);
}


// ============================================================================
// makePrices
// ============================================================================

std::vector<HistoricalMarketPrice>
makePrices()
{
    return
    {
        {1'000'000, 101.0},
        {1'000'060, 102.0},
        {1'000'120, 103.0},
        {1'000'180, 104.0}
    };
}


// ============================================================================
// Test 1 — complete generation
// ============================================================================

void testCompleteGeneration(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        12345U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    const auto result =
        generator.generate(
            999'940,
            100.0,
            makePrices());


    require(
        result.symbol == "RELIANCE",
        "Generation result preserves symbol",
        stats);


    require(
        result.episode_state_count == 5,
        "Historical episode contains five states",
        stats);


    require(
        result.transitions_generated == 4,
        "Five states generate exactly four transitions",
        stats);


    require(
        result.transitions_stored == 4,
        "All generated transitions are stored",
        stats);


    require(
        result.replay_size_before == 0,
        "Replay buffer begins empty",
        stats);


    require(
        result.replay_size_after == 4,
        "Replay buffer contains four historical transitions",
        stats);


    require(
        replay_buffer.size() == 4,
        "Replay-buffer size matches generated experience",
        stats);


    require(
        result.terminal_reached,
        "Generation reaches genuine final episode state",
        stats);


    require(
        result.numerical_integrity,
        "Generation reports numerical integrity",
        stats);


    require(
        episode.isTerminal(),
        "Episode is terminal after generation",
        stats);


    require(
        policy.selectionCount() == 4,
        "Policy is called once per generated transition",
        stats);


    require(
        generator.symbol() == "RELIANCE",
        "Generator preserves traded symbol",
        stats);
}


// ============================================================================
// Test 2 — transition chronology
// ============================================================================

void testTransitionChronology(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        7U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    (void)generator.generate(
        999'940,
        100.0,
        makePrices());


    require(
        replay_buffer.at(0).decision_time ==
            1'000'000,
        "First experience begins at first historical state",
        stats);


    require(
        replay_buffer.at(0).next_decision_time ==
            1'000'060,
        "First experience points to next historical state",
        stats);


    require(
        replay_buffer.at(1).decision_time ==
            1'000'060,
        "Second experience begins at second state",
        stats);


    require(
        replay_buffer.at(2).decision_time ==
            1'000'120,
        "Third experience begins at third state",
        stats);


    require(
        replay_buffer.at(3).decision_time ==
            1'000'180,
        "Final experience begins at penultimate state",
        stats);


    require(
        replay_buffer.at(3).next_decision_time ==
            1'000'240,
        "Final experience points to genuine final state",
        stats);


    require(
        !replay_buffer.at(0).terminal,
        "First experience is non-terminal",
        stats);


    require(
        !replay_buffer.at(1).terminal,
        "Second experience is non-terminal",
        stats);


    require(
        !replay_buffer.at(2).terminal,
        "Third experience is non-terminal",
        stats);


    require(
        replay_buffer.at(3).terminal,
        "Final experience is terminal",
        stats);
}


// ============================================================================
// Test 3 — baseline HOLD reward integrity
// ============================================================================

void testRewardIntegrity(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        99U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    (void)generator.generate(
        999'940,
        100.0,
        makePrices());


    for (std::size_t index = 0;
         index < replay_buffer.size();
         ++index)
    {
        const auto& transition =
            replay_buffer.at(index);


        require(
            transition.action ==
                DevAction::HOLD,
            "Baseline historical experience preserves HOLD action",
            stats);


        require(
            std::isfinite(
                transition.reward),
            "Historical transition reward is finite",
            stats);


        require(
            approximatelyEqual(
                transition.reward,
                0.0),
            "Flat baseline HOLD produces zero reward",
            stats);
    }
}


// ============================================================================
// Test 4 — invalid price count
// ============================================================================

void testInvalidPriceCount(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        1U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    const std::vector<HistoricalMarketPrice>
        too_few_prices{
            {1'000'000, 101.0},
            {1'000'060, 102.0}
        };


    expectThrow(
        [&]()
        {
            (void)generator.generate(
                999'940,
                100.0,
                too_few_prices);
        },
        "Incorrect historical price count is rejected",
        stats);
}


// ============================================================================
// Test 5 — future-price misalignment protection
// ============================================================================

void testPriceAlignmentProtection(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        2U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    auto prices =
        makePrices();


    // Deliberately corrupt first timestamp:
    //
    // S(t) would receive a price from t+60.
    prices[0].decision_time =
        1'000'060;


    expectThrow(
        [&]()
        {
            (void)generator.generate(
                999'940,
                100.0,
                prices);
        },
        "Future-price/state misalignment is rejected",
        stats);
}


// ============================================================================
// Test 6 — invalid prices
// ============================================================================

void testInvalidPrices(
    TestStats& stats)
{
    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;

        DevReplayBuffer replay_buffer(
            32,
            3U);


        HistoricalExperienceGenerator generator(
            "RELIANCE",
            episode,
            environment,
            policy,
            replay_buffer);


        auto prices =
            makePrices();

        prices[1].market_price =
            0.0;


        expectThrow(
            [&]()
            {
                (void)generator.generate(
                    999'940,
                    100.0,
                    prices);
            },
            "Zero historical market price is rejected",
            stats);
    }


    {
        DevEpisode episode =
            makeEpisode();

        TradingEnvironment environment;

        BaselineDevAgent policy;

        DevReplayBuffer replay_buffer(
            32,
            4U);


        HistoricalExperienceGenerator generator(
            "RELIANCE",
            episode,
            environment,
            policy,
            replay_buffer);


        auto prices =
            makePrices();

        prices[2].market_price =
            -10.0;


        expectThrow(
            [&]()
            {
                (void)generator.generate(
                    999'940,
                    100.0,
                    prices);
            },
            "Negative historical market price is rejected",
            stats);
    }
}


// ============================================================================
// Test 7 — chronological price protection
// ============================================================================

void testChronologyProtection(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        5U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    auto prices =
        makePrices();


    prices[2].decision_time =
        prices[1].decision_time;


    expectThrow(
        [&]()
        {
            (void)generator.generate(
                999'940,
                100.0,
                prices);
        },
        "Non-increasing historical price chronology is rejected",
        stats);
}


// ============================================================================
// Test 8 — invalid reset
// ============================================================================

void testInvalidReset(
    TestStats& stats)
{
    DevEpisode episode =
        makeEpisode();

    TradingEnvironment environment;

    BaselineDevAgent policy;

    DevReplayBuffer replay_buffer(
        32,
        6U);


    HistoricalExperienceGenerator generator(
        "RELIANCE",
        episode,
        environment,
        policy,
        replay_buffer);


    expectThrow(
        [&]()
        {
            (void)generator.generate(
                1'000'000,
                100.0,
                makePrices());
        },
        "Reset at episode start time is rejected",
        stats);


    expectThrow(
        [&]()
        {
            (void)generator.generate(
                999'940,
                0.0,
                makePrices());
        },
        "Zero environment reset price is rejected",
        stats);
}


// ============================================================================
// Main
// ============================================================================

} // namespace


int main()
{
    TestStats stats;


    std::cout
        << "\n"
        << "============================================================\n"
        << "DEVAI PHASE 3.6 — HISTORICAL EXPERIENCE GENERATION TEST\n"
        << "============================================================\n";


    try
    {
        testCompleteGeneration(
            stats);

        testTransitionChronology(
            stats);

        testRewardIntegrity(
            stats);

        testInvalidPriceCount(
            stats);

        testPriceAlignmentProtection(
            stats);

        testInvalidPrices(
            stats);

        testChronologyProtection(
            stats);

        testInvalidReset(
            stats);
    }
    catch (const std::exception& exception)
    {
        ++stats.failures;

        std::cerr
            << "\n[FATAL] Unexpected exception: "
            << exception.what()
            << '\n';
    }
    catch (...)
    {
        ++stats.failures;

        std::cerr
            << "\n[FATAL] Unknown unexpected exception.\n";
    }


    std::cout
        << "\n"
        << "============================================================\n"
        << "PHASE 3.6 TEST SUMMARY\n"
        << "============================================================\n"
        << "Checks   : "
        << stats.checks
        << '\n'
        << "Failures : "
        << stats.failures
        << '\n'
        << "============================================================\n";


    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.6 HISTORICAL EXPERIENCE GENERATION: PASSED\n"
            << "============================================================\n";

        return 0;
    }


    std::cerr
        << "DEVAI PHASE 3.6 HISTORICAL EXPERIENCE GENERATION: FAILED\n"
        << "============================================================\n";

    return 1;
}