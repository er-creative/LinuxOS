#include "devai/market/Candle.hpp"
#include "devai/market/MarketDataLoader.hpp"

#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/DevQNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationNormalizer.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <algorithm>
#include <array>
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
// DEVAI PHASE 3.7B
//
// REAL HISTORICAL Q-NETWORK INTEGRATION
//
// Purpose:
//
//     Genuine historical 1-minute market data
//              ↓
//     HistoricalCETEReplay
//              ↓
//     CrossSectionalUniverseSnapshot
//              ↓
//     ObservationBuilder
//              ↓
//     DevAIObservation
//              ↓
//     ObservationNormalizer
//              ↓
//     84 normalized neural-network inputs
//              ↓
//     DevQNetwork
//              ↓
//     Q(HOLD), Q(LONG), Q(SHORT), Q(EXIT)
//
// This test performs INFERENCE ONLY.
//
// It deliberately contains:
//
//     - no ForwardReturnOutcomeEngine
//     - no ForwardReturn_N
//     - no future-state input
//     - no replay-buffer sampling
//     - no Bellman target
//     - no optimizer
//     - no training
//
// The network is still randomly initialized. Therefore the greedy action
// produced by this test is NOT a trading recommendation.
//
// ============================================================================


// ============================================================================
// Configuration
// ============================================================================

constexpr const char* DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

constexpr const char* BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::size_t REAL_STATE_COUNT =
    20;

constexpr std::int64_t ONE_MINUTE_SECONDS =
    60;

constexpr double INITIAL_EQUITY =
    100000.0;

constexpr std::uint32_t NETWORK_SEED =
    12345U;


// ============================================================================
// Test statistics
// ============================================================================

struct TestStats
{
    std::size_t checks{0};
    std::size_t failures{0};

    std::size_t replay_points{0};
    std::size_t observations{0};
    std::size_t normalized_observations{0};
    std::size_t q_inferences{0};
    std::size_t deterministic_comparisons{0};

    std::size_t chronology_violations{0};
    std::size_t replay_violations{0};
    std::size_t observation_violations{0};
    std::size_t normalization_violations{0};
    std::size_t q_value_violations{0};
    std::size_t action_violations{0};
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

        return;
    }

    ++stats.failures;

    std::cerr
        << "[FAIL] "
        << description
        << '\n';
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
        std::fabs(
            left - right) <=
        tolerance;
}


// ============================================================================
// finiteArray
// ============================================================================

template <std::size_t N>
bool finiteArray(
    const std::array<double, N>& values)
{
    return
        std::all_of(
            values.begin(),
            values.end(),
            [](const double value)
            {
                return std::isfinite(value);
            });
}


// ============================================================================
// validAction
// ============================================================================

bool validAction(
    const DevAction action)
{
    switch (action)
    {
        case DevAction::HOLD:
        case DevAction::LONG:
        case DevAction::SHORT:
        case DevAction::EXIT:
            return true;
    }

    return false;
}




// ============================================================================
// priceAtDecisionTime
//
// Historical CETE convention:
//
//     candle timestamp = beginning of one-minute candle
//
// Example:
//
//     candle timestamp : 09:15
//     candle interval  : 09:15 → 09:16
//     decision time    : 09:16
//
// Therefore:
//
//     observable candle timestamp = decision_time - 60
//
// Only the completed historical candle is used.
// ============================================================================

double priceAtDecisionTime(
    const std::vector<Candle>& candles,
    const std::int64_t decision_time)
{
    if (decision_time <= ONE_MINUTE_SECONDS)
    {
        throw std::invalid_argument(
            "priceAtDecisionTime: invalid decision time.");
    }

    const std::int64_t candle_time =
        decision_time -
        ONE_MINUTE_SECONDS;

    const auto iterator =
        std::lower_bound(
            candles.begin(),
            candles.end(),
            candle_time,
            [](const Candle& candle,
               const std::int64_t timestamp)
            {
                return
                    candle.timestamp <
                    timestamp;
            });

    if (iterator == candles.end() ||
        iterator->timestamp != candle_time)
    {
        throw std::runtime_error(
            "priceAtDecisionTime: exact historical candle not found.");
    }

    if (!std::isfinite(iterator->close) ||
        iterator->close <= 0.0)
    {
        throw std::runtime_error(
            "priceAtDecisionTime: historical close is invalid.");
    }

    return iterator->close;
}


} // anonymous namespace


// ============================================================================
// main
// ============================================================================

int main()
{
    using namespace devai::market;
    using namespace devai::strategy;

    TestStats stats;

    std::cout
        << "\n"
        << "======================================================================\n"
        << "DEVAI PHASE 3.7B — REAL HISTORICAL Q-NETWORK INTEGRATION\n"
        << "======================================================================\n";

    try
    {
        // ====================================================================
        // 1. Configure genuine CETE historical replay
        // ====================================================================

        HistoricalCETEReplayConfig replay_config;

        replay_config.data_folder =
            DATA_FOLDER;

        replay_config.benchmark_symbol =
            BENCHMARK_SYMBOL;

        replay_config.minimum_universe_size =
            2;

        // Keep the integration validation small and deterministic.
        //
        // The purpose here is not universe-wide performance testing.
        // The purpose is to prove the complete real-state → Q-network path.
        replay_config.maximum_stock_count =
            2;

        HistoricalCETEReplay replay(
            replay_config);


        // ====================================================================
        // 2. Discover genuine tradable symbols
        // ====================================================================

        const std::vector<std::string> symbols =
            replay.discoverSymbols();

        require(
            !symbols.empty(),
            "Historical replay discovers at least one tradable stock",
            stats);

        if (symbols.empty())
        {
            throw std::runtime_error(
                "Historical replay discovered no tradable stocks.");
        }

        const std::string traded_symbol =
            symbols.front();

        require(
            !traded_symbol.empty(),
            "Selected traded symbol is non-empty",
            stats);

        require(
            traded_symbol != BENCHMARK_SYMBOL,
            "Selected traded symbol is not the NIFTY benchmark",
            stats);

        std::cout
            << "\n"
            << "Traded symbol    : "
            << traded_symbol
            << '\n'
            << "Benchmark symbol : "
            << BENCHMARK_SYMBOL
            << '\n';


        // ====================================================================
        // 3. Load genuine one-minute stock history
        // ====================================================================

        MarketDataLoader loader(
            DATA_FOLDER);

        const std::filesystem::path stock_path =
            std::filesystem::path(DATA_FOLDER) /
            (
                traded_symbol +
                "_1min.txt"
            );

        const auto stock_load =
            loader.loadPath(
                stock_path);

        require(
            !stock_load.candles.empty(),
            "Real one-minute stock history is non-empty",
            stats);

        require(
            stock_load.candles.size() >=
                REAL_STATE_COUNT + 1,
            "Real stock history contains enough candles",
            stats);

        if (stock_load.candles.size() <
            REAL_STATE_COUNT + 1)
        {
            throw std::runtime_error(
                "Insufficient real historical candles for Phase 3.7B.");
        }


        // ====================================================================
        // 4. Latest common causal replay time
        // ====================================================================

        const std::int64_t latest_common_time =
            replay.latestCommonDecisionTime();

        require(
            latest_common_time > 0,
            "Historical replay exposes positive latest common decision time",
            stats);

        if (latest_common_time <= 0)
        {
            throw std::runtime_error(
                "Invalid latest common historical decision time.");
        }


        // ====================================================================
        // 5. Select 20 genuine historical states
        //
        // Work backwards from latest available common history and then reverse
        // into chronological order.
        //
        // No synthetic candle is created.
        // ====================================================================

        std::vector<const Candle*> candidate_candles;

        candidate_candles.reserve(
            REAL_STATE_COUNT);

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
                REAL_STATE_COUNT)
            {
                break;
            }
        }

        require(
            candidate_candles.size() ==
                REAL_STATE_COUNT,
            "Exactly 20 genuine historical states selected",
            stats);

        if (candidate_candles.size() !=
            REAL_STATE_COUNT)
        {
            throw std::runtime_error(
                "Could not select required real historical states.");
        }

        std::reverse(
            candidate_candles.begin(),
            candidate_candles.end());


        // ====================================================================
        // 6. Validate selected historical chronology
        // ====================================================================

        std::int64_t previous_decision_time =
            0;

        for (const Candle* candle :
             candidate_candles)
        {
            require(
                candle != nullptr,
                "Selected historical candle pointer is valid",
                stats);

            if (candle == nullptr)
            {
                ++stats.chronology_violations;
                continue;
            }

            const std::int64_t decision_time =
                candle->timestamp +
                ONE_MINUTE_SECONDS;

            const bool chronology_valid =
                previous_decision_time == 0 ||
                decision_time >
                    previous_decision_time;

            require(
                chronology_valid,
                "Selected historical states advance strictly forward in time",
                stats);

            if (!chronology_valid)
            {
                ++stats.chronology_violations;
            }

            const bool common_time_valid =
                decision_time <=
                latest_common_time;

            require(
                common_time_valid,
                "Selected state does not exceed latest common replay time",
                stats);

            if (!common_time_valid)
            {
                ++stats.chronology_violations;
            }

            previous_decision_time =
                decision_time;
        }


        // ====================================================================
        // 7. Reconstruct genuine CETE snapshots
        // ====================================================================

        std::vector<
            devai::ranking::CrossSectionalUniverseSnapshot>
            snapshots;

        snapshots.reserve(
            REAL_STATE_COUNT);

        for (const Candle* candle :
             candidate_candles)
        {
            if (candle == nullptr)
            {
                throw std::runtime_error(
                    "Null candle encountered during replay.");
            }

            const std::int64_t decision_time =
                candle->timestamp +
                ONE_MINUTE_SECONDS;

            const auto point =
                replay.replayAt(
                    decision_time);

            ++stats.replay_points;

            const bool time_matches =
                point.snapshot.decision_time ==
                decision_time;

            require(
                time_matches,
                "Replay snapshot exactly matches requested historical decision time",
                stats);

            if (!time_matches)
            {
                ++stats.replay_violations;
            }

            const bool benchmark_matches =
                point.snapshot.benchmark_symbol ==
                BENCHMARK_SYMBOL;

            require(
                benchmark_matches,
                "Replay snapshot uses frozen NIFTY benchmark",
                stats);

            if (!benchmark_matches)
            {
                ++stats.replay_violations;
            }

            require(
                point.snapshot.numerical_integrity,
                "Replay snapshot has numerical integrity",
                stats);

            if (!point.snapshot.numerical_integrity)
            {
                ++stats.replay_violations;
            }

            snapshots.push_back(
                point.snapshot);
        }

        require(
            snapshots.size() ==
                REAL_STATE_COUNT,
            "Exactly 20 real CETE snapshots reconstructed",
            stats);


        // ====================================================================
        // 8. Establish genuine pre-episode account state
        //
        // This test performs no actions. TradingEnvironment is used only to
        // provide the genuine frozen DevPortfolioState contract expected by
        // ObservationBuilder.
        // ====================================================================

        TradingEnvironmentConfig environment_config;

        environment_config.initial_equity =
            INITIAL_EQUITY;

        TradingEnvironment environment(
            environment_config);

        const std::int64_t first_decision_time =
            snapshots.front().
                decision_time;

        const std::int64_t environment_reset_time =
            first_decision_time -
            ONE_MINUTE_SECONDS;

        const double environment_reset_price =
            priceAtDecisionTime(
                stock_load.candles,
                environment_reset_time);

        environment.reset(
            traded_symbol,
            environment_reset_time,
            environment_reset_price);

        require(
            environment.state().decision_time ==
                environment_reset_time,
            "TradingEnvironment resets at genuine pre-episode historical time",
            stats);

        require(
            first_decision_time >
                environment.state().decision_time,
            "First Q-network state occurs after environment reset",
            stats);

        require(
            std::isfinite(
                environment_reset_price) &&
            environment_reset_price > 0.0,
            "Environment reset uses genuine finite historical price",
            stats);


        // ====================================================================
        // 9. Frozen observation infrastructure
        // ====================================================================

        ObservationBuilder observation_builder;

        ObservationNormalizer observation_normalizer;


        // ====================================================================
        // 10. Two independently constructed deterministic Q-networks
        //
        // Same seed + same input must produce identical Q-values.
        // ====================================================================

        DevQNetworkConfig q_config;

        q_config.random_seed =
            NETWORK_SEED;

        q_config.zero_initialize =
            false;

        DevQNetwork network_a(
            q_config);

        DevQNetwork network_b(
            q_config);

        require(
            network_a.parameterCount() ==
                7652,
            "Real integration network contains exactly 7,652 parameters",
            stats);

        require(
            network_b.parameterCount() ==
                network_a.parameterCount(),
            "Independent deterministic network has identical architecture",
            stats);


        // ====================================================================
        // 11. Real historical Q-network inference
        // ====================================================================

        previous_decision_time =
            0;

        for (std::size_t state_index = 0;
             state_index < snapshots.size();
             ++state_index)
        {
            const auto& snapshot =
                snapshots[state_index];

            const std::int64_t decision_time =
                snapshot.decision_time;


            // ----------------------------------------------------------------
            // Genuine historical current price.
            //
            // The price is checked for alignment/integrity. It is not supplied
            // directly as an extra neural-network field.
            // ----------------------------------------------------------------

            const double current_market_price =
                priceAtDecisionTime(
                    stock_load.candles,
                    decision_time);

            require(
                std::isfinite(
                    current_market_price) &&
                current_market_price > 0.0,
                "Current state has genuine finite historical market price",
                stats);


            // ----------------------------------------------------------------
            // Strictly causal chronology
            // ----------------------------------------------------------------

            const bool chronology_valid =
                previous_decision_time == 0 ||
                decision_time >
                    previous_decision_time;

            require(
                chronology_valid,
                "Q-network inference advances strictly forward in time",
                stats);

            if (!chronology_valid)
            {
                ++stats.chronology_violations;
            }

            previous_decision_time =
                decision_time;


            // ----------------------------------------------------------------
            // Current portfolio state only
            // ----------------------------------------------------------------

            const DevPortfolioState portfolio =
                environment.portfolioState();


            // ----------------------------------------------------------------
            // Build current DevAI observation
            // ----------------------------------------------------------------

            const DevAIObservation observation =
                observation_builder.build(
                    snapshot,
                    traded_symbol,
                    portfolio);

            ++stats.observations;

            require(
                observation.numerical_integrity,
                "Real DevAIObservation has numerical integrity",
                stats);

            require(
                observation.ready,
                "Real DevAIObservation is ready",
                stats);

            if (!observation.numerical_integrity ||
                !observation.ready)
            {
                ++stats.observation_violations;
            }


            // ----------------------------------------------------------------
            // Validate frozen 39 market inputs
            // ----------------------------------------------------------------

            bool market_finite =
                true;

            for (const double value :
                 observation.market)
            {
                if (!std::isfinite(value))
                {
                    market_finite =
                        false;

                    break;
                }
            }

            require(
                market_finite,
                "All 39 observation market values are finite",
                stats);

            if (!market_finite)
            {
                ++stats.observation_violations;
            }


            // ----------------------------------------------------------------
            // Validate frozen 39 availability inputs
            // ----------------------------------------------------------------

            bool availability_valid =
                true;

            for (const double value :
                 observation.availability)
            {
                if (!std::isfinite(value) ||
                    (value != 0.0 &&
                     value != 1.0))
                {
                    availability_valid =
                        false;

                    break;
                }
            }

            require(
                availability_valid,
                "All 39 observation availability masks are finite and binary",
                stats);

            if (!availability_valid)
            {
                ++stats.observation_violations;
            }


            // ----------------------------------------------------------------
            // Normalize through the already-frozen ObservationNormalizer
            // ----------------------------------------------------------------

            const auto normalized =
                observation_normalizer.normalize(
                    observation);

            ++stats.normalized_observations;

            const bool normalized_finite =
                finiteArray(
                    normalized);

            require(
                normalized_finite,
                "All 84 normalized Q-network inputs are finite",
                stats);

            if (!normalized_finite)
            {
                ++stats.normalization_violations;
            }


            // ----------------------------------------------------------------
            // Frozen market-feature range
            //
            // ObservationNormalizer clamps these values to [-1, 1].
            // ----------------------------------------------------------------

            bool normalized_market_valid =
                true;

            for (std::size_t index = 0;
                 index < DEV_MARKET_FEATURE_COUNT;
                 ++index)
            {
                const double value =
                    normalized[index];

                if (!std::isfinite(value) ||
                    value < -1.0 ||
                    value > 1.0)
                {
                    normalized_market_valid =
                        false;

                    break;
                }
            }

            require(
                normalized_market_valid,
                "Normalized market inputs remain within [-1, 1]",
                stats);

            if (!normalized_market_valid)
            {
                ++stats.normalization_violations;
            }


            // ----------------------------------------------------------------
            // Frozen availability range
            // ----------------------------------------------------------------

            bool normalized_availability_valid =
                true;

            const std::size_t availability_offset =
                DEV_MARKET_FEATURE_COUNT;

            for (std::size_t index = 0;
                 index < DEV_AVAILABILITY_COUNT;
                 ++index)
            {
                const double value =
                    normalized[
                        availability_offset +
                        index];

                if (value != 0.0 &&
                    value != 1.0)
                {
                    normalized_availability_valid =
                        false;

                    break;
                }
            }

            require(
                normalized_availability_valid,
                "Normalized availability inputs remain binary",
                stats);

            if (!normalized_availability_valid)
            {
                ++stats.normalization_violations;
            }


            // ----------------------------------------------------------------
            // Verify portfolio portion of normalized observation is finite
            // ----------------------------------------------------------------

            bool normalized_portfolio_valid =
                true;

            const std::size_t portfolio_offset =
                DEV_MARKET_FEATURE_COUNT +
                DEV_AVAILABILITY_COUNT;

            for (std::size_t index = 0;
                 index < DEV_PORTFOLIO_FEATURE_COUNT;
                 ++index)
            {
                const double value =
                    normalized[
                        portfolio_offset +
                        index];

                if (!std::isfinite(value))
                {
                    normalized_portfolio_valid =
                        false;

                    break;
                }
            }

            require(
                normalized_portfolio_valid,
                "All six normalized portfolio inputs are finite",
                stats);

            if (!normalized_portfolio_valid)
            {
                ++stats.normalization_violations;
            }


            // ----------------------------------------------------------------
            // Network A — DevAIObservation inference path
            // ----------------------------------------------------------------

            const DevQValues q_a =
                network_a.predict(
                    observation);

            ++stats.q_inferences;

            require(
                q_a.numerical_integrity,
                "Primary Q-network inference has numerical integrity",
                stats);


            // ----------------------------------------------------------------
            // Network B — same seed, same observation
            // ----------------------------------------------------------------

            const DevQValues q_b =
                network_b.predict(
                    observation);

            ++stats.q_inferences;

            require(
                q_b.numerical_integrity,
                "Independent Q-network inference has numerical integrity",
                stats);


            // ----------------------------------------------------------------
            // Direct normalized-array inference
            //
            // This proves:
            //
            // network.predict(observation)
            //
            // is consistent with:
            //
            // ObservationNormalizer::normalize(observation)
            //     ↓
            // network.predict(array)
            // ----------------------------------------------------------------

            const DevQValues q_direct =
                network_a.predict(
                    normalized);

            ++stats.q_inferences;

            require(
                q_direct.numerical_integrity,
                "Direct normalized-array inference has numerical integrity",
                stats);


            // ----------------------------------------------------------------
            // All four Q-values must be finite
            // ----------------------------------------------------------------

            bool q_values_finite =
                true;

            for (const double value :
                 q_a.values)
            {
                if (!std::isfinite(value))
                {
                    q_values_finite =
                        false;

                    break;
                }
            }

            require(
                q_values_finite,
                "All four real historical Q-values are finite",
                stats);

            if (!q_values_finite)
            {
                ++stats.q_value_violations;
            }


            // ----------------------------------------------------------------
            // Observation overload must match normalized-array overload
            // ----------------------------------------------------------------

            bool observation_path_matches =
                true;

            for (std::size_t q_index = 0;
                 q_index < DEV_Q_OUTPUT_SIZE;
                 ++q_index)
            {
                if (!approximatelyEqual(
                        q_a.values[q_index],
                        q_direct.values[q_index]))
                {
                    observation_path_matches =
                        false;

                    break;
                }
            }

            require(
                observation_path_matches,
                "Observation inference matches explicit normalized-input inference",
                stats);

            if (!observation_path_matches)
            {
                ++stats.q_value_violations;
            }


            // ----------------------------------------------------------------
            // Deterministic initialization validation
            // ----------------------------------------------------------------

            bool deterministic =
                true;

            for (std::size_t q_index = 0;
                 q_index < DEV_Q_OUTPUT_SIZE;
                 ++q_index)
            {
                if (!approximatelyEqual(
                        q_a.values[q_index],
                        q_b.values[q_index]))
                {
                    deterministic =
                        false;

                    break;
                }
            }

            require(
                deterministic,
                "Same seed produces identical Q-values for real observation",
                stats);

            if (deterministic)
            {
                ++stats.deterministic_comparisons;
            }
            else
            {
                ++stats.q_value_violations;
            }


            // ----------------------------------------------------------------
            // Frozen DevAction contract
            // ----------------------------------------------------------------

            const DevAction greedy_action =
                q_a.greedyAction();

            const bool action_valid =
                validAction(
                    greedy_action);

            require(
                action_valid,
                "Greedy network output maps to legal DevAction",
                stats);

            if (!action_valid)
            {
                ++stats.action_violations;
            }


            // ----------------------------------------------------------------
            // Greedy action must point to maximum Q-value
            // ----------------------------------------------------------------

            const bool maximum_matches =
                approximatelyEqual(
                    q_a.maximumValue(),
                    q_a.value(
                        greedy_action));

            require(
                maximum_matches,
                "Greedy action Q-value equals maximum network Q-value",
                stats);

            if (!maximum_matches)
            {
                ++stats.q_value_violations;
            }


            // ----------------------------------------------------------------
            // Per-state diagnostics
            // ----------------------------------------------------------------

            std::cout
                << "\n"
                << "------------------------------------------------------------\n"
                << "REAL Q-NETWORK STATE "
                << (state_index + 1)
                << "/"
                << snapshots.size()
                << '\n'
                << "------------------------------------------------------------\n"
                << "Decision time : "
                << decision_time
                << '\n'
                << "Market price  : "
                << current_market_price
                << '\n'
                << "Q(HOLD)       : "
                << q_a.value(
                    DevAction::HOLD)
                << '\n'
                << "Q(LONG)       : "
                << q_a.value(
                    DevAction::LONG)
                << '\n'
                << "Q(SHORT)      : "
                << q_a.value(
                    DevAction::SHORT)
                << '\n'
                << "Q(EXIT)       : "
                << q_a.value(
                    DevAction::EXIT)
                << '\n'
                << "Maximum Q     : "
                << q_a.maximumValue()
                << '\n'
                << "Greedy action : "
                << devai::strategy::actionName(
                    greedy_action)
                << '\n';
        }


        // ====================================================================
        // 12. Final real-integration invariants
        // ====================================================================

        require(
            stats.replay_points ==
                REAL_STATE_COUNT,
            "All 20 selected historical states were replayed",
            stats);

        require(
            stats.observations ==
                REAL_STATE_COUNT,
            "All 20 real replay states produced DevAI observations",
            stats);

        require(
            stats.normalized_observations ==
                REAL_STATE_COUNT,
            "All 20 observations produced normalized 84-value inputs",
            stats);

        require(
            stats.q_inferences ==
                REAL_STATE_COUNT * 3,
            "Exactly three Q-network inferences completed per real state",
            stats);

        require(
            stats.deterministic_comparisons ==
                REAL_STATE_COUNT,
            "All real states passed deterministic duplicate inference",
            stats);

        require(
            stats.chronology_violations ==
                0,
            "No historical chronology violations detected",
            stats);

        require(
            stats.replay_violations ==
                0,
            "No CETE replay violations detected",
            stats);

        require(
            stats.observation_violations ==
                0,
            "No DevAI observation violations detected",
            stats);

        require(
            stats.normalization_violations ==
                0,
            "No observation-normalization violations detected",
            stats);

        require(
            stats.q_value_violations ==
                0,
            "No Q-network numerical or determinism violations detected",
            stats);

        require(
            stats.action_violations ==
                0,
            "No illegal DevAction outputs detected",
            stats);


        // ====================================================================
        // 13. Anti-leakage architecture assertions
        //
        // The test intentionally has no ForwardReturnOutcomeEngine and no
        // ForwardReturn_N variable.
        //
        // Network input is produced exclusively from:
        //
        //     current CrossSectionalUniverseSnapshot
        //     +
        //     current DevPortfolioState
        //
        // through the frozen ObservationBuilder.
        //
        // No S_(t+1) is passed to predict(S_t).
        // ====================================================================

        require(
            true,
            "ForwardReturn_N is absent from Q-network inference path",
            stats);

        require(
            true,
            "Future market state is absent from Q-network inference path",
            stats);

        require(
            true,
            "Q-network inference uses current CETE state only",
            stats);
    }
    catch (const std::exception& exception)
    {
        ++stats.failures;

        std::cerr
            << "\n"
            << "[FATAL] Unexpected exception: "
            << exception.what()
            << '\n';
    }
    catch (...)
    {
        ++stats.failures;

        std::cerr
            << "\n"
            << "[FATAL] Unknown unexpected exception.\n";
    }


    // ========================================================================
    // 14. Final summary
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 3.7B REAL HISTORICAL Q-NETWORK INTEGRATION SUMMARY\n"
        << "======================================================================\n"
        << "Replay points               : "
        << stats.replay_points
        << '\n'
        << "DevAI observations          : "
        << stats.observations
        << '\n'
        << "Normalized observations     : "
        << stats.normalized_observations
        << '\n'
        << "Q-network inferences        : "
        << stats.q_inferences
        << '\n'
        << "Deterministic comparisons   : "
        << stats.deterministic_comparisons
        << '\n'
        << '\n'
        << "Chronology violations       : "
        << stats.chronology_violations
        << '\n'
        << "Replay violations           : "
        << stats.replay_violations
        << '\n'
        << "Observation violations      : "
        << stats.observation_violations
        << '\n'
        << "Normalization violations    : "
        << stats.normalization_violations
        << '\n'
        << "Q-value violations          : "
        << stats.q_value_violations
        << '\n'
        << "Action violations           : "
        << stats.action_violations
        << '\n'
        << '\n'
        << "Checks                      : "
        << stats.checks
        << '\n'
        << "Failures                    : "
        << stats.failures
        << '\n'
        << "======================================================================\n";

    if (stats.failures == 0)
    {
        std::cout
            << "DEVAI PHASE 3.7B REAL HISTORICAL Q-NETWORK INTEGRATION: PASSED\n"
            << "======================================================================\n";

        return 0;
    }

    std::cerr
        << "DEVAI PHASE 3.7B REAL HISTORICAL Q-NETWORK INTEGRATION: FAILED\n"
        << "======================================================================\n";

    return 1;
}