#include "devai/market/Candle.hpp"
#include "devai/market/MarketDataLoader.hpp"

#include "devai/strategy/dev_ai_strategy/BaselineDevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalExperienceGenerator.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using namespace devai::market;
using namespace devai::strategy;


// ============================================================================
// Configuration
// ============================================================================

constexpr const char* DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

constexpr const char* BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::size_t MINIMUM_UNIVERSE_SIZE =
    2;

// 20 genuine CETE states produce exactly 19 RL transitions.
constexpr std::size_t EPISODE_STATE_COUNT =
    20;

constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr double INITIAL_EQUITY =
    100000.0;


// ============================================================================
// Validation statistics
// ============================================================================

struct ValidationStats
{
    std::size_t checks{0};

    std::size_t failures{0};

    std::size_t replay_points{0};

    std::size_t generated_transitions{0};

    std::size_t lookahead_violations{0};

    std::size_t chronology_violations{0};

    std::size_t price_alignment_violations{0};

    std::size_t observation_violations{0};

    std::size_t transition_violations{0};
};


// ============================================================================
// require
// ============================================================================

void require(
    const bool condition,
    const std::string& description,
    ValidationStats& stats)
{
    ++stats.checks;

    if (condition)
    {
        std::cout
            << "[PASS] "
            << description
            << '\n';

        return;
    }

    ++stats.failures;

    std::cerr
        << "[FAIL] "
        << description
        << '\n';

    throw std::runtime_error(
        description);
}


// ============================================================================
// approximatelyEqual
// ============================================================================

bool approximatelyEqual(
    const double left,
    const double right,
    const double absolute_tolerance = 1.0e-8,
    const double relative_tolerance = 1.0e-10)
{
    if (!std::isfinite(left) ||
        !std::isfinite(right))
    {
        return false;
    }

    const double difference =
        std::fabs(
            left - right);

    if (difference <=
        absolute_tolerance)
    {
        return true;
    }

    const double scale =
        std::max(
            std::fabs(left),
            std::fabs(right));

    return
        difference <=
        relative_tolerance * scale;
}


// ============================================================================
// priceAtDecisionTime
//
// CETE historical convention:
//
//     candle timestamp T
//
// represents:
//
//     [T, T + 60)
//
// Therefore state/decision time:
//
//     T + 60
//
// uses the completed candle close from T.
//
// No future candle is accessed.
// ============================================================================

double priceAtDecisionTime(
    const std::vector<Candle>& candles,
    const std::int64_t decision_time)
{
    if (decision_time <=
        ONE_MINUTE_SECONDS)
    {
        throw std::invalid_argument(
            "priceAtDecisionTime: invalid decision time.");
    }


    const std::int64_t candle_timestamp =
        decision_time -
        ONE_MINUTE_SECONDS;


    const auto iterator =
        std::lower_bound(
            candles.begin(),
            candles.end(),
            candle_timestamp,
            [](const Candle& candle,
               const std::int64_t timestamp)
            {
                return
                    candle.timestamp <
                    timestamp;
            });


    if (iterator ==
            candles.end() ||
        iterator->timestamp !=
            candle_timestamp)
    {
        throw std::runtime_error(
            "priceAtDecisionTime: exact historical candle not found.");
    }


    if (!std::isfinite(
            iterator->close) ||
        iterator->close <= 0.0)
    {
        throw std::runtime_error(
            "priceAtDecisionTime: invalid historical close.");
    }


    return
        iterator->close;
}


// ============================================================================
// Main validation
// ============================================================================

void runValidation()
{
    ValidationStats stats;


    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.6B — REAL HISTORICAL EXPERIENCE GENERATION\n"
        << "======================================================================\n";


    // ========================================================================
    // 1. Configure genuine CETE historical replay
    //
    // Use two stocks for structural validation.
    //
    // This is sufficient for the cross-sectional ranking contract and keeps
    // repeated Phase-4/Phase-5 historical reconstruction practical.
    // ========================================================================

    HistoricalCETEReplayConfig replay_config;

    replay_config.data_folder =
        DATA_FOLDER;

    replay_config.benchmark_symbol =
        BENCHMARK_SYMBOL;

    replay_config.minimum_universe_size =
        MINIMUM_UNIVERSE_SIZE;

    replay_config.maximum_stock_count =
        2;


    HistoricalCETEReplay replay(
        replay_config);


    // ========================================================================
    // 2. Discover genuine tradable stocks
    // ========================================================================

    const auto symbols =
        replay.discoverSymbols();


    require(
        symbols.size() >=
            MINIMUM_UNIVERSE_SIZE,
        "Historical replay discovered sufficient real stocks",
        stats);


    require(
        std::find(
            symbols.begin(),
            symbols.end(),
            BENCHMARK_SYMBOL) ==
            symbols.end(),
        "Benchmark is excluded from tradable stock universe",
        stats);


    const std::string traded_symbol =
        symbols.front();


    require(
        !traded_symbol.empty(),
        "Real traded symbol is valid",
        stats);


    std::cout
        << "\n"
        << "Selected validation symbol : "
        << traded_symbol
        << '\n'
        << "Benchmark                  : "
        << BENCHMARK_SYMBOL
        << '\n'
        << "Required real states       : "
        << EPISODE_STATE_COUNT
        << '\n'
        << "Expected transitions       : "
        << (EPISODE_STATE_COUNT - 1)
        << "\n\n";


    // ========================================================================
    // 3. Load genuine 1-minute history
    // ========================================================================

    MarketDataLoader loader(
        DATA_FOLDER);


    const std::filesystem::path stock_path =
        std::filesystem::path(
            DATA_FOLDER) /
        (
            traded_symbol +
            "_1min.txt"
        );


    require(
        std::filesystem::exists(
            stock_path),
        "Real traded-stock 1-minute file exists",
        stats);


    const auto stock_load =
        loader.loadPath(
            stock_path);


    require(
        stock_load.candles.size() >
            EPISODE_STATE_COUNT,
        "Real 1-minute history contains sufficient candles",
        stats);


    // ========================================================================
    // 4. Find latest common CETE decision time
    // ========================================================================

    const std::int64_t latest_common_time =
        replay.latestCommonDecisionTime();


    require(
        latest_common_time > 0,
        "Latest common historical CETE decision time is valid",
        stats);


    // ========================================================================
    // 5. Select genuine historical candles
    //
    // IMPORTANT:
    //
    // We derive every decision time from a real candle.
    //
    // We DO NOT generate:
    //
    //     start + 60
    //     start + 120
    //     ...
    //
    // This means real market gaps/session boundaries are respected.
    //
    // We also need one genuine preceding candle for TradingEnvironment reset.
    // ========================================================================

    std::vector<const Candle*>
        candidate_candles;


    for (auto iterator =
             stock_load.candles.rbegin();
         iterator !=
             stock_load.candles.rend();
         ++iterator)
    {
        const std::int64_t decision_time =
            iterator->timestamp +
            ONE_MINUTE_SECONDS;


        if (decision_time >
            latest_common_time)
        {
            continue;
        }


        candidate_candles.push_back(
            &(*iterator));


        if (candidate_candles.size() ==
            EPISODE_STATE_COUNT)
        {
            break;
        }
    }


    require(
        candidate_candles.size() ==
            EPISODE_STATE_COUNT,
        "Selected exactly 20 genuine historical states",
        stats);


    std::reverse(
        candidate_candles.begin(),
        candidate_candles.end());


    // ========================================================================
    // 6. Build real causal CETE Phase-5 snapshots
    // ========================================================================

    std::vector<DevEpisode::Snapshot>
        snapshots;

    snapshots.reserve(
        EPISODE_STATE_COUNT);


    std::vector<HistoricalMarketPrice>
        market_prices;

    market_prices.reserve(
        EPISODE_STATE_COUNT - 1);


    std::int64_t previous_decision_time =
        0;


    for (std::size_t index = 0;
         index < candidate_candles.size();
         ++index)
    {
        const Candle& candle =
            *candidate_candles[index];


        const std::int64_t decision_time =
            candle.timestamp +
            ONE_MINUTE_SECONDS;


        if (previous_decision_time != 0 &&
            decision_time <=
                previous_decision_time)
        {
            ++stats.chronology_violations;
        }


        // --------------------------------------------------------------------
        // Reconstruct the complete causal CETE state at this exact historical
        // decision time.
        //
        // HistoricalCETEReplay must not consume future outcome information.
        // --------------------------------------------------------------------

        const auto point =
            replay.replayAt(
                decision_time);


        ++stats.replay_points;


        require(
            point.snapshot.decision_time ==
                decision_time,
            "Real replay snapshot matches requested decision time",
            stats);


        require(
            point.snapshot.benchmark_symbol ==
                BENCHMARK_SYMBOL,
            "Real replay snapshot preserves benchmark identity",
            stats);


        require(
            point.snapshot.stock_count >=
                MINIMUM_UNIVERSE_SIZE,
            "Real replay snapshot contains sufficient stocks",
            stats);


        require(
            point.snapshot.numerical_integrity,
            "Real replay snapshot has numerical integrity",
            stats);


        require(
            point.stats.lookahead_violations ==
                0,
            "Historical CETE replay reports zero look-ahead violations",
            stats);


        require(
            point.stats.decision_time_violations ==
                0,
            "Historical CETE replay reports zero decision-time violations",
            stats);


        require(
            point.stats.symbol_violations ==
                0,
            "Historical CETE replay reports zero symbol violations",
            stats);


        require(
            point.stats.benchmark_contamination ==
                0,
            "Historical CETE replay reports zero benchmark contamination",
            stats);


        require(
            point.stats.duplicate_symbol_violations ==
                0,
            "Historical CETE replay reports zero duplicate-symbol violations",
            stats);


        if (point.stats.lookahead_violations !=
            0)
        {
            stats.lookahead_violations +=
                point.stats.lookahead_violations;
        }


        const auto stock_iterator =
            std::find_if(
                point.snapshot.stocks.begin(),
                point.snapshot.stocks.end(),
                [&](const auto& stock)
                {
                    return
                        stock.symbol ==
                        traded_symbol;
                });


        require(
            stock_iterator !=
                point.snapshot.stocks.end(),
            "Traded stock exists in real CETE snapshot",
            stats);


        require(
            stock_iterator->decision_time ==
                decision_time,
            "Traded stock state is aligned to real decision time",
            stats);


        require(
            stock_iterator->numerical_integrity,
            "Traded stock mathematical state has numerical integrity",
            stats);


        snapshots.push_back(
            point.snapshot);


        // --------------------------------------------------------------------
        // HistoricalExperienceGenerator needs market prices only for source
        // states:
        //
        //     S0 -> S1
        //     ...
        //     S18 -> S19
        //
        // Therefore 20 states require 19 source-state prices.
        // --------------------------------------------------------------------

        if (index <
            candidate_candles.size() - 1)
        {
            const double causal_price =
                priceAtDecisionTime(
                    stock_load.candles,
                    decision_time);


            if (!approximatelyEqual(
                    causal_price,
                    candle.close))
            {
                ++stats.price_alignment_violations;
            }


            require(
                approximatelyEqual(
                    causal_price,
                    candle.close),
                "Historical experience price equals current completed candle close",
                stats);


            market_prices.push_back(
                HistoricalMarketPrice{
                    decision_time,
                    causal_price
                });
        }


        previous_decision_time =
            decision_time;
    }


    require(
        stats.replay_points ==
            EPISODE_STATE_COUNT,
        "Exactly 20 real CETE replay points were reconstructed",
        stats);


    require(
        snapshots.size() ==
            EPISODE_STATE_COUNT,
        "Exactly 20 real CETE snapshots were collected",
        stats);


    require(
        market_prices.size() ==
            EPISODE_STATE_COUNT - 1,
        "Exactly 19 causal source-state prices were collected",
        stats);


    require(
        stats.chronology_violations ==
            0,
        "Real historical snapshot sequence is strictly chronological",
        stats);


    require(
        stats.lookahead_violations ==
            0,
        "Real historical reconstruction has zero look-ahead violations",
        stats);


    require(
        stats.price_alignment_violations ==
            0,
        "All real market prices are aligned to their current states",
        stats);


    // ========================================================================
    // 7. Construct real DevEpisode
    // ========================================================================

    DevEpisodeConfig episode_config;

    episode_config.minimum_universe_size =
        MINIMUM_UNIVERSE_SIZE;


    DevEpisode episode(
        snapshots,
        episode_config);


    episode.reset();


    require(
        episode.isInitialized(),
        "DevEpisode initializes from genuine CETE states",
        stats);


    require(
        episode.totalSteps() ==
            EPISODE_STATE_COUNT,
        "DevEpisode preserves all 20 genuine historical states",
        stats);


    require(
        episode.startDecisionTime() ==
            snapshots.front().decision_time,
        "DevEpisode start matches first genuine historical state",
        stats);


    require(
        episode.endDecisionTime() ==
            snapshots.back().decision_time,
        "DevEpisode end matches final genuine historical state",
        stats);


    require(
        episode.benchmarkSymbol() ==
            BENCHMARK_SYMBOL,
        "DevEpisode preserves benchmark identity",
        stats);


    // ========================================================================
    // 8. Determine genuine preceding reset state
    //
    // Do NOT simply invent:
    //
    //     episode.startDecisionTime() - 60
    //
    // and assume data exists.
    //
    // We request the exact preceding completed historical candle through the
    // frozen CETE convention.
    // ========================================================================

    const std::int64_t environment_reset_time =
        episode.startDecisionTime() -
        ONE_MINUTE_SECONDS;


    const double environment_reset_price =
        priceAtDecisionTime(
            stock_load.candles,
            environment_reset_time);


    require(
        environment_reset_time >
            0,
        "Environment reset time is valid",
        stats);


    require(
        environment_reset_time <
            episode.startDecisionTime(),
        "Environment reset precedes first Dev state",
        stats);


    require(
        std::isfinite(
            environment_reset_price) &&
            environment_reset_price > 0.0,
        "Environment reset uses genuine positive historical price",
        stats);


    // ========================================================================
    // 9. TradingEnvironment
    // ========================================================================

    TradingEnvironmentConfig environment_config;

    environment_config.initial_equity =
        INITIAL_EQUITY;

    environment_config.exposure_fraction =
        1.0;

    environment_config.slippage_rate =
        0.0002;


    TradingEnvironment environment(
        environment_config);


    // ========================================================================
    // 10. Baseline policy
    //
    // Phase 3.6 validates experience generation, not learning.
    //
    // BaselineDevAgent uses HOLD only and does not inspect future outcomes.
    // ========================================================================

    BaselineDevAgent policy;


    // ========================================================================
    // 11. Replay buffer
    //
    // Capacity deliberately exceeds the 19 generated transitions so every
    // transition remains directly inspectable after generation.
    // ========================================================================

    DevReplayBuffer replay_buffer(
        64,
        12345U);


    require(
        replay_buffer.empty(),
        "Replay buffer begins empty",
        stats);


    // ========================================================================
    // 12. Real Historical Experience Generator
    // ========================================================================

    HistoricalExperienceGenerator generator(
        traded_symbol,
        episode,
        environment,
        policy,
        replay_buffer);


    const auto generation =
        generator.generate(
            environment_reset_time,
            environment_reset_price,
            market_prices);


    stats.generated_transitions =
        generation.transitions_generated;


    // ========================================================================
    // 13. Generation contract
    // ========================================================================

    require(
        generation.symbol ==
            traded_symbol,
        "Generation result preserves genuine traded symbol",
        stats);


    require(
        generation.episode_start_time ==
            snapshots.front().decision_time,
        "Generation start matches first real CETE state",
        stats);


    require(
        generation.episode_end_time ==
            snapshots.back().decision_time,
        "Generation end matches final real CETE state",
        stats);


    require(
        generation.episode_state_count ==
            EPISODE_STATE_COUNT,
        "Generation processes all 20 real historical states",
        stats);


    require(
        generation.transitions_generated ==
            EPISODE_STATE_COUNT - 1,
        "20 real historical states generate exactly 19 transitions",
        stats);


    require(
        generation.transitions_stored ==
            EPISODE_STATE_COUNT - 1,
        "All 19 real historical transitions are stored",
        stats);


    require(
        generation.replay_size_before ==
            0,
        "Real experience generation begins with empty replay memory",
        stats);


    require(
        generation.replay_size_after ==
            EPISODE_STATE_COUNT - 1,
        "Replay memory contains exactly 19 real historical transitions",
        stats);


    require(
        generation.terminal_reached,
        "Real experience generation reaches genuine final state",
        stats);


    require(
        generation.numerical_integrity,
        "Real historical experience generation reports numerical integrity",
        stats);


    require(
        episode.isTerminal(),
        "Real DevEpisode is terminal after complete generation",
        stats);


    require(
        policy.selectionCount() ==
            EPISODE_STATE_COUNT - 1,
        "Dev policy is called exactly once per real transition",
        stats);


    // ========================================================================
    // 14. Validate every real DevTransition
    // ========================================================================

    require(
        replay_buffer.size() ==
            EPISODE_STATE_COUNT - 1,
        "Replay buffer retains all 19 real DevTransitions",
        stats);


    DevTransitionValidator transition_validator;


    for (std::size_t index = 0;
         index < replay_buffer.size();
         ++index)
    {
        const auto& transition =
            replay_buffer.at(index);


        const std::int64_t expected_time =
            snapshots[index].decision_time;

        const std::int64_t expected_next_time =
            snapshots[index + 1].decision_time;


        if (transition.decision_time !=
                expected_time ||
            transition.next_decision_time !=
                expected_next_time)
        {
            ++stats.transition_violations;
        }


        require(
            transition.decision_time ==
                expected_time,
            "Transition decision time matches genuine source state",
            stats);


        require(
            transition.next_decision_time ==
                expected_next_time,
            "Transition next time matches genuine destination state",
            stats);


        require(
            transition.next_decision_time >
                transition.decision_time,
            "Real transition chronology is strictly forward",
            stats);


        require(
            transition.action ==
                DevAction::HOLD,
            "Baseline policy produces HOLD without future information",
            stats);


        require(
            std::isfinite(
                transition.reward),
            "Real historical transition reward is finite",
            stats);


        require(
            approximatelyEqual(
                transition.reward,
                0.0),
            "Flat baseline historical transition has zero reward",
            stats);


        bool transition_valid =
            true;


        try
        {
            transition_validator.validate(
                transition);
        }
        catch (const std::exception&)
        {
            transition_valid =
                false;

            ++stats.transition_violations;
        }


        require(
            transition_valid,
            "Real historical DevTransition passes frozen validator",
            stats);


        const bool should_be_terminal =
            index ==
            replay_buffer.size() - 1;


        require(
            transition.terminal ==
                should_be_terminal,
            should_be_terminal
                ? "Final real transition is terminal"
                : "Intermediate real transition is non-terminal",
            stats);


        // --------------------------------------------------------------------
        // Observation anti-leakage structural check.
        //
        // DevAIObservation itself contains only the frozen:
        //
        //     market features
        //     availability masks
        //     portfolio state
        //
        // ForwardReturn_N is not a member of DevTransition or
        // DevAIObservation.
        //
        // Here we additionally require the generated observations themselves
        // to remain valid.
        // --------------------------------------------------------------------

        if (!transition.observation.numerical_integrity ||
            !transition.observation.ready ||
            !transition.next_observation.numerical_integrity ||
            !transition.next_observation.ready)
        {
            ++stats.observation_violations;
        }


        require(
            transition.observation.numerical_integrity,
            "Current real transition observation has numerical integrity",
            stats);


        require(
            transition.observation.ready,
            "Current real transition observation is ready",
            stats);


        require(
            transition.next_observation.numerical_integrity,
            "Next real transition observation has numerical integrity",
            stats);


        require(
            transition.next_observation.ready,
            "Next real transition observation is ready",
            stats);
    }


    // ========================================================================
    // 15. Boundary validation
    // ========================================================================

    const auto& first_transition =
        replay_buffer.at(0);

    const auto& final_transition =
        replay_buffer.at(
            replay_buffer.size() - 1);


    require(
        first_transition.decision_time ==
            snapshots.front().decision_time,
        "First transition begins at first genuine CETE state",
        stats);


    require(
        final_transition.decision_time ==
            snapshots[snapshots.size() - 2].decision_time,
        "Final transition begins at genuine penultimate CETE state",
        stats);


    require(
        final_transition.next_decision_time ==
            snapshots.back().decision_time,
        "Final transition ends at genuine final CETE state",
        stats);


    require(
        final_transition.terminal,
        "Final real transition carries terminal flag",
        stats);


    require(
        stats.transition_violations ==
            0,
        "Zero real historical transition violations detected",
        stats);


    require(
        stats.observation_violations ==
            0,
        "Zero real historical observation violations detected",
        stats);


    // ========================================================================
    // 16. Final report
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.6B REAL HISTORICAL EXPERIENCE SUMMARY\n"
        << "======================================================================\n"
        << "Traded symbol                : "
        << traded_symbol
        << '\n'
        << "Benchmark                    : "
        << BENCHMARK_SYMBOL
        << '\n'
        << "Real CETE states             : "
        << snapshots.size()
        << '\n'
        << "Real DevTransitions          : "
        << replay_buffer.size()
        << '\n'
        << "Replay points reconstructed  : "
        << stats.replay_points
        << '\n'
        << "Look-ahead violations        : "
        << stats.lookahead_violations
        << '\n'
        << "Chronology violations        : "
        << stats.chronology_violations
        << '\n'
        << "Price alignment violations   : "
        << stats.price_alignment_violations
        << '\n'
        << "Observation violations       : "
        << stats.observation_violations
        << '\n'
        << "Transition violations        : "
        << stats.transition_violations
        << '\n'
        << "Checks                       : "
        << stats.checks
        << '\n'
        << "Failures                     : "
        << stats.failures
        << '\n'
        << "======================================================================\n";


    require(
        stats.failures == 0,
        "All Phase 3.6B real historical checks passed",
        stats);


    std::cout
        << "\n"
        << "DEVAI PHASE 3.6B REAL HISTORICAL EXPERIENCE GENERATION: PASSED\n"
        << "======================================================================\n";
}


} // namespace


int main()
{
    try
    {
        runValidation();

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "======================================================================\n"
            << "DEVAI PHASE 3.6B REAL HISTORICAL EXPERIENCE GENERATION: FAILED\n"
            << "Reason: "
            << exception.what()
            << '\n'
            << "======================================================================\n";

        return 1;
    }
    catch (...)
    {
        std::cerr
            << "\n"
            << "======================================================================\n"
            << "DEVAI PHASE 3.6B REAL HISTORICAL EXPERIENCE GENERATION: FAILED\n"
            << "Reason: unknown exception\n"
            << "======================================================================\n";

        return 1;
    }
}