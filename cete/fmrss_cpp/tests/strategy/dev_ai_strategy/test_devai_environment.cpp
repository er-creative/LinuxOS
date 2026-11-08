#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationNormalizer.hpp"
#include "devai/strategy/dev_ai_strategy/RewardEngine.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>


namespace
{

constexpr double EPSILON =
    1.0e-10;


// ============================================================================
// Utility
// ============================================================================

bool almostEqual(
    double lhs,
    double rhs,
    double epsilon = EPSILON)
{
    return
        std::fabs(lhs - rhs) <=
        epsilon;
}


void require(
    bool condition,
    const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(
            message);
    }
}


// ============================================================================
// 1.1 / 1.2 — Observation contract
// ============================================================================

void testObservationContract()
{
    using namespace devai::strategy;


    static_assert(
        DEV_METRICS_PER_TIMEFRAME == 13);

    static_assert(
        DEV_TIMEFRAME_COUNT == 3);

    static_assert(
        DEV_MARKET_FEATURE_COUNT == 39);

    static_assert(
        DEV_AVAILABILITY_COUNT == 39);

    static_assert(
        DEV_PORTFOLIO_FEATURE_COUNT == 6);

    static_assert(
        DEV_OBSERVATION_SIZE == 84);


    require(
        marketIndex(
            DevTimeframe::ONE_MINUTE,
            DevMetric::RETURN_VALUE) == 0,
        "1m first metric index failed.");


    require(
        marketIndex(
            DevTimeframe::FIVE_MINUTE,
            DevMetric::RETURN_VALUE) == 13,
        "5m first metric index failed.");


    require(
        marketIndex(
            DevTimeframe::FIFTEEN_MINUTE,
            DevMetric::RETURN_VALUE) == 26,
        "15m first metric index failed.");


    require(
        marketIndex(
            DevTimeframe::ONE_MINUTE,
            DevMetric::ROLLING_CORRELATION) == 12,
        "1m last metric index failed.");


    require(
        marketIndex(
            DevTimeframe::FIVE_MINUTE,
            DevMetric::ROLLING_CORRELATION) == 25,
        "5m last metric index failed.");


    require(
        marketIndex(
            DevTimeframe::FIFTEEN_MINUTE,
            DevMetric::ROLLING_CORRELATION) == 38,
        "15m last metric index failed.");
}


// ============================================================================
// 1.4 — Observation normalization
// ============================================================================

void testNormalization()
{
    using namespace devai::strategy;


    DevAIObservation observation;

    observation.symbol =
        "RELIANCE";

    observation.benchmark_symbol =
        "NIFTY%2050";

    observation.decision_time =
        1000;

    observation.numerical_integrity =
        true;

    observation.ready =
        true;


    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        observation.market[i] =
            0.5;

        observation.availability[i] =
            1.0;
    }


    observation.portfolio.position =
        1.0;

    observation.portfolio.unrealized_return =
        0.01;

    observation.portfolio.realized_return =
        0.02;

    observation.portfolio.equity_return =
        0.03;

    observation.portfolio.drawdown =
        0.04;

    observation.portfolio.holding_period =
        10.0;


    ObservationNormalizer normalizer(
        100.0);


    const auto normalized =
        normalizer.normalize(
            observation);


    require(
        normalized.size() ==
            DEV_OBSERVATION_SIZE,
        "Normalized observation size failed.");


    for (const double value :
         normalized)
    {
        require(
            std::isfinite(value),
            "Normalized observation contains NaN/Inf.");
    }


    require(
        almostEqual(
            normalized[0],
            0.5),
        "Centered percentile was modified incorrectly.");


    // All 39 market features should remain 0.5.
    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        require(
            almostEqual(
                normalized[i],
                0.5),
            "Market normalization failed.");
    }


    // All 39 availability masks should remain 1.0.
    const std::size_t availability_start =
        DEV_MARKET_FEATURE_COUNT;


    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        require(
            almostEqual(
                normalized[
                    availability_start + i],
                1.0),
            "Availability-mask normalization failed.");
    }


    // Position is the first portfolio feature.
    const std::size_t portfolio_start =
        DEV_MARKET_FEATURE_COUNT +
        DEV_AVAILABILITY_COUNT;


    require(
        almostEqual(
            normalized[portfolio_start],
            1.0),
        "Portfolio position normalization failed.");


    // Drawdown is portfolio feature #5.
    require(
        almostEqual(
            normalized[
                portfolio_start + 4],
            0.04),
        "Drawdown normalization failed.");


    // Holding period:
    //
    // 10 / 100 = 0.10
    require(
        almostEqual(
            normalized[
                portfolio_start + 5],
            0.10),
        "Holding-period normalization failed.");
}


// ============================================================================
// 1.5 — Action mapping
// ============================================================================

void testActions()
{
    using namespace devai::strategy;


    require(
        DEV_ACTION_COUNT == 4,
        "Dev action count must equal four.");


    require(
        targetPosition(
            DevAction::LONG,
            0.0) == 1.0,
        "LONG action failed.");


    require(
        targetPosition(
            DevAction::SHORT,
            0.0) == -1.0,
        "SHORT action failed.");


    require(
        targetPosition(
            DevAction::EXIT,
            1.0) == 0.0,
        "EXIT action failed.");


    require(
        targetPosition(
            DevAction::EXIT,
            -1.0) == 0.0,
        "EXIT from short failed.");


    require(
        targetPosition(
            DevAction::HOLD,
            -1.0) == -1.0,
        "HOLD short action failed.");


    require(
        targetPosition(
            DevAction::HOLD,
            1.0) == 1.0,
        "HOLD long action failed.");
}


// ============================================================================
// 1.6 — Reward Engine
// ============================================================================

void testReward()
{
    using namespace devai::strategy;


    RewardConfig config;

    config.turnover_penalty =
        0.001;

    config.drawdown_penalty =
        0.10;


    RewardEngine engine(
        config);


    RewardInput input;

    input.net_equity_return =
        0.01;

    input.turnover =
        1.0;

    input.previous_drawdown =
        0.02;

    input.current_drawdown =
        0.03;


    const auto reward =
        engine.calculate(
            input);


    require(
        reward.pnl_component > 0.0,
        "Reward P&L component failed.");


    require(
        reward.turnover_component < 0.0,
        "Turnover penalty failed.");


    require(
        reward.drawdown_component < 0.0,
        "Drawdown penalty failed.");


    require(
        std::isfinite(
            reward.total),
        "Reward total is non-finite.");


    const double expected_pnl =
        0.01;

    const double expected_turnover =
        -0.001;

    const double expected_drawdown =
        -0.001;

    const double expected_total =
        expected_pnl +
        expected_turnover +
        expected_drawdown;


    require(
        almostEqual(
            reward.pnl_component,
            expected_pnl),
        "Reward P&L value incorrect.");


    require(
        almostEqual(
            reward.turnover_component,
            expected_turnover),
        "Reward turnover value incorrect.");


    require(
        almostEqual(
            reward.drawdown_component,
            expected_drawdown),
        "Reward drawdown value incorrect.");


    require(
        almostEqual(
            reward.total,
            expected_total),
        "Reward total calculation incorrect.");
}


// ============================================================================
// 1.7 / 1.8 — Critical anti-leakage test
//
// Enter LONG at price 100.
//
// The entry step MUST NOT receive profit from a future price.
//
// Only the next environment step at price 110 may generate the +10%
// market return.
//
// This protects:
//
//     Observation(t)
//          ↓
//       Action(t)
//          ↓
//     Market moves
//          ↓
//      Reward(t+1)
//
// ============================================================================

void testNoSameStepFutureProfit()
{
    using namespace devai::strategy;


    TradingEnvironmentConfig config;

    config.initial_equity =
        100000.0;

    config.transaction_cost_rate =
        0.0;

    config.slippage_rate =
        0.0;


    RewardConfig reward_config;

    reward_config.turnover_penalty =
        0.0;

    reward_config.drawdown_penalty =
        0.0;


    TradingEnvironment environment(
        config,
        reward_config);


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    // ========================================================================
    // Dev enters LONG at current price 100.
    //
    // No future move has happened yet.
    // ========================================================================

    const auto entry =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);


    require(
        almostEqual(
            entry.market_return,
            0.0),
        "Entry step contains unexpected market return.");


    require(
        almostEqual(
            entry.gross_pnl,
            0.0),
        "Entry action received profit "
        "before future move occurred.");


    require(
        almostEqual(
            entry.net_pnl,
            0.0),
        "Entry action received net P&L "
        "before future move occurred.");


    require(
        almostEqual(
            entry.reward.total,
            0.0),
        "Entry reward contains future information.");


    require(
        entry.previous_position == 0.0,
        "Initial position should be flat.");


    require(
        entry.new_position == 1.0,
        "LONG action failed to establish "
        "a long position.");


    // ========================================================================
    // Future move becomes observable only now.
    //
    // 100 -> 110 = +10%
    // ========================================================================

    const auto next =
        environment.step(
            DevAction::HOLD,
            1120,
            110.0);


    require(
        almostEqual(
            next.market_return,
            0.10),
        "Future market return calculation failed.");


    require(
        next.previous_position == 1.0,
        "Previous LONG position was not preserved.");


    require(
        next.new_position == 1.0,
        "HOLD failed to preserve LONG position.");


    require(
        next.gross_pnl > 0.0,
        "Long position failed to receive "
        "subsequent market return.");


    require(
        next.net_pnl > 0.0,
        "Profitable long move produced "
        "non-positive net P&L.");


    require(
        next.reward.total > 0.0,
        "Profitable future move produced "
        "non-positive reward.");
}


// ============================================================================
// 1.7 — Short-position validation
//
// This version explicitly consumes and validates the [[nodiscard]]
// EnvironmentStepResult returned by the SHORT entry step.
// ============================================================================

void testShortPosition()
{
    using namespace devai::strategy;


    TradingEnvironmentConfig config;

    config.initial_equity =
        100000.0;

    config.transaction_cost_rate =
        0.0;

    config.slippage_rate =
        0.0;


    RewardConfig reward_config;

    reward_config.turnover_penalty =
        0.0;

    reward_config.drawdown_penalty =
        0.0;


    TradingEnvironment environment(
        config,
        reward_config);


    environment.reset(
        "SBIN",
        1000,
        100.0);


    // ========================================================================
    // Establish SHORT at price 100.
    //
    // The returned value is deliberately checked rather than discarded.
    // ========================================================================

    const auto entry =
        environment.step(
            DevAction::SHORT,
            1060,
            100.0);


    require(
        almostEqual(
            entry.market_return,
            0.0),
        "SHORT entry contains unexpected "
        "market return.");


    require(
        almostEqual(
            entry.gross_pnl,
            0.0),
        "SHORT entry received profit "
        "before future move occurred.");


    require(
        almostEqual(
            entry.reward.total,
            0.0),
        "SHORT entry reward contains "
        "future information.");


    require(
        entry.previous_position == 0.0,
        "SHORT entry previous position "
        "should be flat.");


    require(
        entry.new_position == -1.0,
        "SHORT action failed to establish "
        "a short position.");


    // ========================================================================
    // Price subsequently falls:
    //
    // 100 -> 90
    //
    // Existing SHORT should profit.
    // ========================================================================

    const auto result =
        environment.step(
            DevAction::HOLD,
            1120,
            90.0);


    require(
        almostEqual(
            result.market_return,
            -0.10),
        "SHORT market-return calculation failed.");


    require(
        result.previous_position == -1.0,
        "Previous SHORT position was not preserved.");


    require(
        result.new_position == -1.0,
        "HOLD failed to preserve SHORT position.");


    require(
        result.gross_pnl > 0.0,
        "Short position did not profit "
        "from subsequent price decline.");


    require(
        result.net_pnl > 0.0,
        "Profitable short move produced "
        "non-positive net P&L.");


    require(
        result.reward.total > 0.0,
        "Profitable short move produced "
        "non-positive reward.");
}


// ============================================================================
// 1.7 — EXIT validation
// ============================================================================

void testExitPosition()
{
    using namespace devai::strategy;


    TradingEnvironmentConfig config;

    config.initial_equity =
        100000.0;

    config.transaction_cost_rate =
        0.0;

    config.slippage_rate =
        0.0;


    RewardConfig reward_config;

    reward_config.turnover_penalty =
        0.0;

    reward_config.drawdown_penalty =
        0.0;


    TradingEnvironment environment(
        config,
        reward_config);


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    const auto entry =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);


    require(
        entry.new_position == 1.0,
        "LONG position establishment failed.");


    const auto exit =
        environment.step(
            DevAction::EXIT,
            1120,
            105.0);


    require(
        exit.previous_position == 1.0,
        "EXIT did not observe previous "
        "LONG position.");


    require(
        exit.new_position == 0.0,
        "EXIT failed to flatten position.");


    require(
        exit.gross_pnl > 0.0,
        "EXIT failed to capture preceding "
        "LONG price move.");


    require(
        almostEqual(
            exit.state.position,
            0.0),
        "Environment remained invested "
        "after EXIT.");
}


// ============================================================================
// 1.7 — Transaction-cost validation
// ============================================================================

void testTransactionCost()
{
    using namespace devai::strategy;


    TradingEnvironmentConfig config;

    config.initial_equity =
        100000.0;

    config.transaction_cost_rate =
        0.001;

    config.slippage_rate =
        0.0;


    RewardConfig reward_config;

    reward_config.turnover_penalty =
        0.0;

    reward_config.drawdown_penalty =
        0.0;


    TradingEnvironment environment(
        config,
        reward_config);


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    const auto entry =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);


    // 100000 * 1.0 turnover * 0.001
    // = 100 transaction cost.

    require(
        almostEqual(
            entry.transaction_cost,
            100.0),
        "Transaction-cost calculation failed.");


    require(
        almostEqual(
            entry.net_pnl,
            -100.0),
        "Entry net P&L did not include "
        "transaction cost.");


    require(
        entry.reward.total < 0.0,
        "Transaction cost did not reduce reward.");
}


// ============================================================================
// 1.8 — Decision-time monotonicity
//
// The environment must reject:
//
//     new_time <= previous_time
//
// The result is marked [[maybe_unused]] because the expected code path throws
// before a result can be returned.
// ============================================================================

void testDecisionTimeProtection()
{
    using namespace devai::strategy;


    TradingEnvironment environment;


    environment.reset(
        "HDFCBANK",
        1000,
        100.0);


    bool rejected =
        false;


    try
    {
        [[maybe_unused]]
        const auto result =
            environment.step(
                DevAction::HOLD,
                1000,
                101.0);
    }
    catch (const std::invalid_argument&)
    {
        rejected =
            true;
    }


    require(
        rejected,
        "Environment accepted non-increasing "
        "decision time.");
}


// ============================================================================
// 1.8 — Invalid future/current price protection
// ============================================================================

void testInvalidPriceProtection()
{
    using namespace devai::strategy;


    TradingEnvironment environment;


    environment.reset(
        "HDFCBANK",
        1000,
        100.0);


    bool rejected =
        false;


    try
    {
        [[maybe_unused]]
        const auto result =
            environment.step(
                DevAction::HOLD,
                1060,
                0.0);
    }
    catch (const std::invalid_argument&)
    {
        rejected =
            true;
    }


    require(
        rejected,
        "Environment accepted zero price.");
}


// ============================================================================
// 1.8 — Environment must be initialized before step()
// ============================================================================

void testInitializationProtection()
{
    using namespace devai::strategy;


    TradingEnvironment environment;


    bool rejected =
        false;


    try
    {
        [[maybe_unused]]
        const auto result =
            environment.step(
                DevAction::HOLD,
                1060,
                100.0);
    }
    catch (const std::runtime_error&)
    {
        rejected =
            true;
    }


    require(
        rejected,
        "Environment allowed step() "
        "before reset().");
}


// ============================================================================
// Main
// ============================================================================

} // anonymous namespace


int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1 — RL ENVIRONMENT FOUNDATION TEST\n"
            << "============================================================\n";


        // ====================================================================
        // Phase 1.1 / 1.2
        // ====================================================================

        testObservationContract();


        std::cout
            << "1.1 Observation-Space Specification : PASSED\n"
            << "1.2 DevAIObservation Structure      : PASSED\n";


        // ====================================================================
        // Phase 1.3
        //
        // ObservationBuilder requires a real CETE Phase-5.4 universe snapshot.
        // It will be certified separately by the real CETE -> Dev integration
        // validation.
        // ====================================================================

        std::cout
            << "1.3 ObservationBuilder              : PENDING REAL CETE TEST\n";


        // ====================================================================
        // Phase 1.4
        // ====================================================================

        testNormalization();


        std::cout
            << "1.4 Observation Normalization       : PASSED\n";


        // ====================================================================
        // Phase 1.5
        // ====================================================================

        testActions();


        std::cout
            << "1.5 Action-Space Specification      : PASSED\n";


        // ====================================================================
        // Phase 1.6
        // ====================================================================

        testReward();


        std::cout
            << "1.6 Reward Engine                    : PASSED\n";


        // ====================================================================
        // Phase 1.7
        // ====================================================================

        testNoSameStepFutureProfit();

        testShortPosition();

        testExitPosition();

        testTransactionCost();


        std::cout
            << "1.7 Trading Environment             : PASSED\n";


        // ====================================================================
        // Phase 1.8
        // ====================================================================

        testDecisionTimeProtection();

        testInvalidPriceProtection();

        testInitializationProtection();


        std::cout
            << "1.8 Leakage + Environment Validation: PASSED\n";


        // ====================================================================
        // Summary
        // ====================================================================

        std::cout
            << "------------------------------------------------------------\n"
            << "Observation market features         : "
            << devai::strategy::DEV_MARKET_FEATURE_COUNT
            << '\n'

            << "Observation availability masks      : "
            << devai::strategy::DEV_AVAILABILITY_COUNT
            << '\n'

            << "Observation portfolio features      : "
            << devai::strategy::DEV_PORTFOLIO_FEATURE_COUNT
            << '\n'

            << "Observation total size              : "
            << devai::strategy::DEV_OBSERVATION_SIZE
            << '\n'

            << "Action count                        : "
            << devai::strategy::DEV_ACTION_COUNT
            << '\n'

            << "Agent                               : Dev\n"

            << "------------------------------------------------------------\n"

            << "DEVAI PHASE 1 FOUNDATION UNIT TESTS PASSED\n"

            << "ObservationBuilder real CETE integration still pending.\n"

            << "============================================================\n";


        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1 TEST FAILED\n"
            << "============================================================\n"
            << exception.what()
            << '\n'
            << "============================================================\n";


        return EXIT_FAILURE;
    }
}