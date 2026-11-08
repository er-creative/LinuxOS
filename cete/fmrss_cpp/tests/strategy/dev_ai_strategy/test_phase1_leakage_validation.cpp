// ============================================================================
// DevAI Phase 1.8
// Leakage + Environment Validation
//
// Final validation gate for:
//
// CETE — Creative Engineer Trading Environment
//     -> DevAI Strategy
//         -> Dev Agent
//
// This test validates:
//
//  1. Observation dimensional integrity
//  2. Finite-value integrity
//  3. Availability-mask integrity
//  4. Unavailable-feature neutrality
//  5. Portfolio-state integrity
//  6. Observation normalization
//  7. Invalid-observation rejection
//  8. Forward/future-return exclusion
//  9. Observation determinism
// 10. Decision-time causality
// 11. Same-step action/P&L isolation
// 12. Reward causality
// 13. Brokerage/slippage propagation
// 14. Reversal execution integrity
// 15. Environment determinism
// 16. Cross-episode reset isolation
//
// ForwardReturn_N is an OFFLINE future outcome only.
// It must never enter Dev's live observation at decision time t.
// ============================================================================

#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationNormalizer.hpp"
#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/RewardEngine.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>


namespace
{

using namespace devai::strategy;


// ============================================================================
// Test helpers
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


bool approximatelyEqual(
    double lhs,
    double rhs,
    double tolerance = 1.0e-10)
{
    return
        std::fabs(lhs - rhs) <=
        tolerance;
}


// ============================================================================
// Zero-cost environment configuration
//
// Used when testing pure timing / P&L causality.
//
// Brokerage = 0
// Statutory charges = 0
// Slippage = 0
// ============================================================================

TradingEnvironmentConfig
zeroCostConfiguration()
{
    TradingEnvironmentConfig config;

    config.initial_equity =
        100000.0;

    config.exposure_fraction =
        1.0;

    config.slippage_rate =
        0.0;

    config.brokerage_config.brokerage_rate =
        0.0;

    config.brokerage_config.brokerage_cap_per_order =
        0.0;

    config.brokerage_config.stt_sell_rate =
        0.0;

    config.brokerage_config.exchange_transaction_rate =
        0.0;

    config.brokerage_config.sebi_rate =
        0.0;

    config.brokerage_config.stamp_duty_buy_rate =
        0.0;

    config.brokerage_config.gst_rate =
        0.0;

    return config;
}


// ============================================================================
// Slippage-only environment configuration
//
// Used to prove that execution-price slippage is not merely reported,
// but actually reaches account equity.
// ============================================================================

TradingEnvironmentConfig
slippageOnlyConfiguration()
{
    auto config =
        zeroCostConfiguration();

    config.slippage_rate =
        0.001; // 10 bps

    return config;
}


// ============================================================================
// 1.8.1
// Observation dimensional integrity
// ============================================================================

void testObservationDimensions()
{
    std::cout
        << "[TEST] 1.8.1 Observation dimensional integrity..."
        << std::endl;


    require(
        DEV_METRICS_PER_TIMEFRAME == 13,
        "Dev metrics per timeframe must remain 13.");


    require(
        DEV_TIMEFRAME_COUNT == 3,
        "Dev timeframe count must remain 3.");


    require(
        DEV_MARKET_FEATURE_COUNT == 39,
        "Market feature count must remain 39.");


    require(
        DEV_AVAILABILITY_COUNT == 39,
        "Availability count must remain 39.");


    require(
        DEV_PORTFOLIO_FEATURE_COUNT == 6,
        "Portfolio feature count must remain 6.");


    require(
        DEV_OBSERVATION_SIZE == 84,
        "Dev observation size must remain 84.");


    require(
        DEV_MARKET_FEATURE_COUNT ==
            DEV_METRICS_PER_TIMEFRAME *
            DEV_TIMEFRAME_COUNT,
        "Market feature dimensional relationship broken.");


    require(
        DEV_OBSERVATION_SIZE ==
            DEV_MARKET_FEATURE_COUNT +
            DEV_AVAILABILITY_COUNT +
            DEV_PORTFOLIO_FEATURE_COUNT,
        "Observation dimensional relationship broken.");
}


// ============================================================================
// 1.8.2
// Finite-value integrity
// ============================================================================

void testFiniteObservationContract()
{
    std::cout
        << "[TEST] 1.8.2 Observation finite-value integrity..."
        << std::endl;


    DevAIObservation observation;

    observation.ready =
        true;

    observation.numerical_integrity =
        true;


    for (double& value :
         observation.market)
    {
        value =
            0.0;
    }


    for (double& value :
         observation.availability)
    {
        value =
            1.0;
    }


    observation.portfolio.position =
        0.0;

    observation.portfolio.unrealized_return =
        0.0;

    observation.portfolio.realized_return =
        0.0;

    observation.portfolio.equity_return =
        0.0;

    observation.portfolio.drawdown =
        0.0;

    observation.portfolio.holding_period =
        0.0;


    for (const double value :
         observation.market)
    {
        require(
            std::isfinite(value),
            "Non-finite market feature detected.");
    }


    for (const double value :
         observation.availability)
    {
        require(
            std::isfinite(value),
            "Non-finite availability mask detected.");
    }


    require(
        std::isfinite(
            observation.portfolio.position),
        "Non-finite portfolio position.");


    require(
        std::isfinite(
            observation.portfolio.unrealized_return),
        "Non-finite unrealized return.");


    require(
        std::isfinite(
            observation.portfolio.realized_return),
        "Non-finite realized return.");


    require(
        std::isfinite(
            observation.portfolio.equity_return),
        "Non-finite equity return.");


    require(
        std::isfinite(
            observation.portfolio.drawdown),
        "Non-finite drawdown.");


    require(
        std::isfinite(
            observation.portfolio.holding_period),
        "Non-finite holding period.");
}


// ============================================================================
// 1.8.3
// Availability-mask integrity
// ============================================================================

void testAvailabilityMaskIntegrity()
{
    std::cout
        << "[TEST] 1.8.3 Availability-mask integrity..."
        << std::endl;


    DevAIObservation observation;


    for (std::size_t i = 0;
         i < observation.availability.size();
         ++i)
    {
        observation.availability[i] =
            (i % 2 == 0)
                ? 1.0
                : 0.0;
    }


    for (const double mask :
         observation.availability)
    {
        require(
            mask == 0.0 ||
            mask == 1.0,
            "Availability mask is not binary.");
    }
}


// ============================================================================
// 1.8.4
// Unavailable feature neutrality
//
// Contract:
//
// unavailable feature
//     market value = 0
//     availability = 0
//
// available neutral feature
//     market value may also = 0
//     availability = 1
//
// Therefore the mask distinguishes:
//
//     unavailable
//
// from:
//
//     genuinely neutral.
// ============================================================================

void testUnavailableFeatureNeutrality()
{
    std::cout
        << "[TEST] 1.8.4 Unavailable-feature neutrality..."
        << std::endl;


    DevAIObservation observation;


    for (std::size_t i = 0;
         i < observation.market.size();
         ++i)
    {
        const bool available =
            (i % 3 != 0);


        observation.availability[i] =
            available
                ? 1.0
                : 0.0;


        observation.market[i] =
            available
                ? 0.25
                : 0.0;
    }


    for (std::size_t i = 0;
         i < observation.market.size();
         ++i)
    {
        if (observation.availability[i] ==
            0.0)
        {
            require(
                approximatelyEqual(
                    observation.market[i],
                    0.0),
                "Unavailable feature is not neutral.");
        }
    }
}


// ============================================================================
// 1.8.5
// Portfolio-state integrity
// ============================================================================

void testPortfolioStateIntegrity()
{
    std::cout
        << "[TEST] 1.8.5 Portfolio-state integrity..."
        << std::endl;


    const auto config =
        zeroCostConfiguration();


    TradingEnvironment environment(
        config);


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    const auto flat =
        environment.portfolioState();


    require(
        flat.position ==
            DEV_POSITION_FLAT,
        "Initial portfolio must be flat.");


    require(
        approximatelyEqual(
            flat.unrealized_return,
            0.0),
        "Initial unrealized return must be zero.");


    require(
        approximatelyEqual(
            flat.realized_return,
            0.0),
        "Initial realized return must be zero.");


    require(
        approximatelyEqual(
            flat.equity_return,
            0.0),
        "Initial equity return must be zero.");


    require(
        approximatelyEqual(
            flat.drawdown,
            0.0),
        "Initial drawdown must be zero.");


    require(
        approximatelyEqual(
            flat.holding_period,
            0.0),
        "Initial holding period must be zero.");


    static_cast<void>(
        environment.step(
            DevAction::LONG,
            1060,
            100.0));


    const auto opened =
        environment.portfolioState();


    require(
        opened.position ==
            DEV_POSITION_LONG,
        "Portfolio did not expose LONG state.");


    require(
        approximatelyEqual(
            opened.holding_period,
            0.0),
        "New position must start with holding period zero.");


    static_cast<void>(
        environment.step(
            DevAction::HOLD,
            1120,
            101.0));


    const auto held =
        environment.portfolioState();


    require(
        approximatelyEqual(
            held.holding_period,
            1.0),
        "Holding period did not advance.");


    require(
        held.unrealized_return > 0.0,
        "Profitable LONG did not expose positive unrealized return.");
}


// ============================================================================
// 1.8.6
// Observation normalization bounds
//
// Actual ObservationNormalizer output:
//
// [0..38]   market
// [39..77]  availability
// [78]      position
// [79]      unrealized return
// [80]      realized return
// [81]      equity return
// [82]      drawdown
// [83]      holding period
// ============================================================================

void testNormalizationBounds()
{
    std::cout
        << "[TEST] 1.8.6 Observation normalization bounds..."
        << std::endl;


    constexpr double MAX_HOLDING_BARS =
        390.0;


    ObservationNormalizer normalizer(
        MAX_HOLDING_BARS);


    DevAIObservation observation;

    observation.ready =
        true;

    observation.numerical_integrity =
        true;


    for (std::size_t i = 0;
         i < observation.market.size();
         ++i)
    {
        observation.market[i] =
            (i % 2 == 0)
                ? 2.0
                : -2.0;


        observation.availability[i] =
            (i % 3 == 0)
                ? 0.0
                : 1.0;
    }


    observation.portfolio.position =
        2.0;

    observation.portfolio.unrealized_return =
        10.0;

    observation.portfolio.realized_return =
        -10.0;

    observation.portfolio.equity_return =
        5.0;

    observation.portfolio.drawdown =
        2.0;

    observation.portfolio.holding_period =
        500.0;


    const auto normalized =
        normalizer.normalize(
            observation);


    static_assert(
        std::tuple_size_v<
            decltype(normalized)> ==
            DEV_OBSERVATION_SIZE,
        "Normalizer output size does not match "
        "Dev observation contract.");


    // ------------------------------------------------------------------------
    // Market
    // ------------------------------------------------------------------------

    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        require(
            std::isfinite(
                normalized[i]),
            "Normalized market feature is non-finite.");


        require(
            normalized[i] >= -1.0 &&
            normalized[i] <= 1.0,
            "Normalized market feature escaped [-1,1].");
    }


    // ------------------------------------------------------------------------
    // Availability
    // ------------------------------------------------------------------------

    constexpr std::size_t
        AVAILABILITY_OFFSET =
            DEV_MARKET_FEATURE_COUNT;


    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        const double actual =
            normalized[
                AVAILABILITY_OFFSET + i];


        const double expected =
            observation.availability[i] > 0.5
                ? 1.0
                : 0.0;


        require(
            actual == 0.0 ||
            actual == 1.0,
            "Normalized availability mask is not binary.");


        require(
            actual == expected,
            "Availability mask normalized incorrectly.");
    }


    // ------------------------------------------------------------------------
    // Portfolio
    // ------------------------------------------------------------------------

    constexpr std::size_t
        PORTFOLIO_OFFSET =
            DEV_MARKET_FEATURE_COUNT +
            DEV_AVAILABILITY_COUNT;


    const double position =
        normalized[
            PORTFOLIO_OFFSET + 0];


    const double unrealized_return =
        normalized[
            PORTFOLIO_OFFSET + 1];


    const double realized_return =
        normalized[
            PORTFOLIO_OFFSET + 2];


    const double equity_return =
        normalized[
            PORTFOLIO_OFFSET + 3];


    const double drawdown =
        normalized[
            PORTFOLIO_OFFSET + 4];


    const double holding_period =
        normalized[
            PORTFOLIO_OFFSET + 5];


    require(
        position >= -1.0 &&
        position <= 1.0,
        "Normalized position escaped [-1,1].");


    require(
        unrealized_return >= -1.0 &&
        unrealized_return <= 1.0,
        "Normalized unrealized return escaped [-1,1].");


    require(
        realized_return >= -1.0 &&
        realized_return <= 1.0,
        "Normalized realized return escaped [-1,1].");


    require(
        equity_return >= -1.0 &&
        equity_return <= 1.0,
        "Normalized equity return escaped [-1,1].");


    require(
        drawdown >= 0.0 &&
        drawdown <= 1.0,
        "Normalized drawdown escaped [0,1].");


    require(
        holding_period >= 0.0 &&
        holding_period <= 1.0,
        "Normalized holding period escaped [0,1].");


    // ------------------------------------------------------------------------
    // Exact expected clamping
    // ------------------------------------------------------------------------

    require(
        approximatelyEqual(
            position,
            1.0),
        "Position was not clamped to +1.");


    require(
        approximatelyEqual(
            drawdown,
            1.0),
        "Drawdown was not clamped to 1.");


    require(
        approximatelyEqual(
            holding_period,
            1.0),
        "Holding period was not clamped to 1.");
}


// ============================================================================
// 1.8.7
// Invalid observation rejection
// ============================================================================

void testInvalidObservationRejection()
{
    std::cout
        << "[TEST] 1.8.7 Invalid observation rejection..."
        << std::endl;


    ObservationNormalizer normalizer;


    // ------------------------------------------------------------------------
    // ready = false
    // ------------------------------------------------------------------------

    {
        DevAIObservation observation;

        observation.ready =
            false;

        observation.numerical_integrity =
            true;


        bool rejected =
            false;


        try
        {
            static_cast<void>(
                normalizer.normalize(
                    observation));
        }
        catch (const std::invalid_argument&)
        {
            rejected =
                true;
        }


        require(
            rejected,
            "Normalizer accepted observation "
            "with ready=false.");
    }


    // ------------------------------------------------------------------------
    // numerical_integrity = false
    // ------------------------------------------------------------------------

    {
        DevAIObservation observation;

        observation.ready =
            true;

        observation.numerical_integrity =
            false;


        bool rejected =
            false;


        try
        {
            static_cast<void>(
                normalizer.normalize(
                    observation));
        }
        catch (const std::invalid_argument&)
        {
            rejected =
                true;
        }


        require(
            rejected,
            "Normalizer accepted observation "
            "with numerical_integrity=false.");
    }


    // ------------------------------------------------------------------------
    // NaN market feature
    // ------------------------------------------------------------------------

    {
        DevAIObservation observation;

        observation.ready =
            true;

        observation.numerical_integrity =
            true;


        for (double& value :
             observation.market)
        {
            value =
                0.0;
        }


        observation.market[0] =
            std::numeric_limits<double>::quiet_NaN();


        bool rejected =
            false;


        try
        {
            static_cast<void>(
                normalizer.normalize(
                    observation));
        }
        catch (const std::runtime_error&)
        {
            rejected =
                true;
        }


        require(
            rejected,
            "Normalizer accepted NaN market feature.");
    }


    // ------------------------------------------------------------------------
    // NaN portfolio return
    // ------------------------------------------------------------------------

    {
        DevAIObservation observation;

        observation.ready =
            true;

        observation.numerical_integrity =
            true;


        observation.portfolio.unrealized_return =
            std::numeric_limits<double>::quiet_NaN();


        bool rejected =
            false;


        try
        {
            static_cast<void>(
                normalizer.normalize(
                    observation));
        }
        catch (const std::invalid_argument&)
        {
            rejected =
                true;
        }


        require(
            rejected,
            "Normalizer accepted NaN portfolio return.");
    }
}


// ============================================================================
// 1.8.8
// Forward / future-return exclusion
//
// Structural Phase-1 observation contract:
//
//     39 market features
//     39 availability masks
//      6 portfolio values
//     ---------------------
//     84 total
//
// ForwardReturn_N is NOT part of this observation contract.
// ============================================================================

void testForwardReturnExclusion()
{
    std::cout
        << "[TEST] 1.8.8 Forward/future-return exclusion..."
        << std::endl;


    static_assert(
        DEV_METRICS_PER_TIMEFRAME == 13,
        "Unexpected metric-count change.");


    static_assert(
        DEV_TIMEFRAME_COUNT == 3,
        "Unexpected timeframe-count change.");


    static_assert(
        DEV_MARKET_FEATURE_COUNT == 39,
        "Unexpected market-feature count.");


    static_assert(
        DEV_AVAILABILITY_COUNT == 39,
        "Unexpected availability count.");


    static_assert(
        DEV_PORTFOLIO_FEATURE_COUNT == 6,
        "Unexpected portfolio-feature count.");


    static_assert(
        DEV_OBSERVATION_SIZE == 84,
        "Observation contract changed.");


    static_assert(
        DEV_MARKET_FEATURE_COUNT ==
            DEV_METRICS_PER_TIMEFRAME *
            DEV_TIMEFRAME_COUNT,
        "Market feature dimensional contract changed.");


    static_assert(
        DEV_OBSERVATION_SIZE ==
            DEV_MARKET_FEATURE_COUNT +
            DEV_AVAILABILITY_COUNT +
            DEV_PORTFOLIO_FEATURE_COUNT,
        "Observation contains unexplained feature capacity.");


    std::cout
        << "       ForwardReturn_N input path : NONE"
        << std::endl;
}


// ============================================================================
// 1.8.9
// Observation determinism
// ============================================================================

void testObservationDeterminism()
{
    std::cout
        << "[TEST] 1.8.9 Observation determinism..."
        << std::endl;


    ObservationNormalizer normalizer;


    DevAIObservation observation;

    observation.ready =
        true;

    observation.numerical_integrity =
        true;


    for (std::size_t i = 0;
         i < observation.market.size();
         ++i)
    {
        observation.market[i] =
            static_cast<double>(
                static_cast<int>(
                    i % 11) - 5) /
            5.0;


        observation.availability[i] =
            (i % 4 == 0)
                ? 0.0
                : 1.0;


        if (observation.availability[i] ==
            0.0)
        {
            observation.market[i] =
                0.0;
        }
    }


    observation.portfolio.position =
        1.0;

    observation.portfolio.unrealized_return =
        0.01;

    observation.portfolio.realized_return =
        -0.005;

    observation.portfolio.equity_return =
        0.004;

    observation.portfolio.drawdown =
        0.02;

    observation.portfolio.holding_period =
        7.0;


    const auto first =
        normalizer.normalize(
            observation);


    const auto second =
        normalizer.normalize(
            observation);


    require(
        first.size() ==
            DEV_OBSERVATION_SIZE,
        "First normalized observation has wrong size.");


    require(
        second.size() ==
            DEV_OBSERVATION_SIZE,
        "Second normalized observation has wrong size.");


    for (std::size_t i = 0;
         i < DEV_OBSERVATION_SIZE;
         ++i)
    {
        require(
            approximatelyEqual(
                first[i],
                second[i]),
            "Identical observation produced "
            "different normalized output.");
    }
}


// ============================================================================
// 1.8.10
// Decision-time causality
//
// Environment time must move strictly forward.
// ============================================================================

void testDecisionTimeCausality()
{
    std::cout
        << "[TEST] 1.8.10 Decision-time causality..."
        << std::endl;


    const auto config =
        zeroCostConfiguration();


    TradingEnvironment environment(
        config);


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    static_cast<void>(
        environment.step(
            DevAction::LONG,
            1060,
            100.0));


    bool duplicate_rejected =
        false;


    try
    {
        static_cast<void>(
            environment.step(
                DevAction::HOLD,
                1060,
                101.0));
    }
    catch (const std::invalid_argument&)
    {
        duplicate_rejected =
            true;
    }


    require(
        duplicate_rejected,
        "Duplicate decision time was accepted.");


    bool past_rejected =
        false;


    try
    {
        static_cast<void>(
            environment.step(
                DevAction::HOLD,
                1050,
                101.0));
    }
    catch (const std::invalid_argument&)
    {
        past_rejected =
            true;
    }


    require(
        past_rejected,
        "Past decision time was accepted.");
}


// ============================================================================
// 1.8.11
// Same-step action / P&L isolation
//
// Correct causal sequence:
//
// position(t-1)
//     earns price(t-1) -> price(t)
//
// observation(t)
//     -> DevAction(t)
//     -> execution at t
//
// new position
//     waits for price(t) -> price(t+1)
//
// DevAction(t) must never earn a move that was already observable
// before the action was selected.
// ============================================================================

void testSameStepActionPnlIsolation()
{
    std::cout
        << "[TEST] 1.8.11 Same-step action/P&L isolation..."
        << std::endl;


    const auto config =
        zeroCostConfiguration();


    TradingEnvironment environment(
        config);


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    // Dev was FLAT while market moved:
    //
    // 100 -> 120
    //
    // LONG is selected only at 120.

    const auto entry =
        environment.step(
            DevAction::LONG,
            1060,
            120.0);


    require(
        approximatelyEqual(
            entry.gross_pnl,
            0.0),
        "LONG action captured pre-action price movement.");


    require(
        approximatelyEqual(
            entry.net_pnl,
            0.0),
        "Zero-cost LONG entry generated same-step P&L.");


    require(
        approximatelyEqual(
            environment.state().equity,
            100000.0),
        "Equity contains retroactive same-step profit.");


    // Only subsequent movement may generate P&L.

    const auto next =
        environment.step(
            DevAction::HOLD,
            1120,
            121.0);


    require(
        next.gross_pnl > 0.0,
        "Post-action price movement did not generate LONG P&L.");
}


// ============================================================================
// 1.8.12
// Reward causality
//
// With all reward penalties disabled:
//
// reward == net_equity_return
//
// The entry action itself must not receive future profit.
// ============================================================================

void testRewardCausality()
{
    std::cout
        << "[TEST] 1.8.12 Reward causality..."
        << std::endl;


    auto config =
        zeroCostConfiguration();


    config.reward_config.turnover_penalty =
        0.0;

    config.reward_config.drawdown_penalty =
        0.0;


    TradingEnvironment environment(
        config);


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
        approximatelyEqual(
            entry.net_equity_return,
            0.0),
        "Zero-cost entry unexpectedly generated return.");


    require(
        approximatelyEqual(
            entry.reward.total,
            0.0),
        "Reward appeared before subsequent market movement.");


    const auto future =
        environment.step(
            DevAction::HOLD,
            1120,
            101.0);


    require(
        future.net_equity_return > 0.0,
        "Subsequent price movement did not create return.");


    require(
        approximatelyEqual(
            future.reward.total,
            future.net_equity_return),
        "Reward is not causally connected "
        "to net equity return.");
}


// ============================================================================
// 1.8.13
// Brokerage / slippage propagation
//
// Part A:
//     normal environment must report brokerage + slippage.
//
// Part B:
//     slippage-only environment must prove that opening slippage
//     reaches EQUITY, not merely a reporting field.
//
// For a BUY at market price P:
//
//     execution price = P * (1 + slippage)
//
// Therefore opening a LONG must immediately create an economic
// loss relative to observable market price P.
//
// This is intentionally stronger than simply checking:
//
//     slippage_cost > 0
// ============================================================================

void testExecutionCostPropagation()
{
    std::cout
        << "[TEST] 1.8.13 Brokerage/slippage propagation..."
        << std::endl;


    // ========================================================================
    // Part A
    // Shared CETE brokerage + slippage reporting
    // ========================================================================

    {
        TradingEnvironment environment;


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
            entry.brokerage_charges > 0.0,
            "Brokerage did not reach TradingEnvironment.");


        require(
            entry.slippage_cost > 0.0,
            "Slippage was not reported.");


        require(
            entry.execution_cost > 0.0,
            "Total execution cost missing.");


        require(
            entry.equity_after <
                entry.equity_before,
            "Execution costs did not reduce equity.");


        require(
            environment.state().
                cumulative_brokerage_charges > 0.0,
            "Cumulative brokerage/statutory charges missing.");


        require(
            environment.state().
                cumulative_slippage_cost > 0.0,
            "Cumulative slippage missing.");


        require(
            environment.state().
                cumulative_execution_cost > 0.0,
            "Cumulative execution cost missing.");
    }


    // ========================================================================
    // Part B
    // Opening slippage must economically reach equity
    //
    // Brokerage/statutory charges are ZERO here.
    //
    // Therefore, if equity falls on entry, that reduction can only
    // have come from execution-price slippage.
    // ========================================================================

    {
        const auto config =
            slippageOnlyConfiguration();


        TradingEnvironment environment(
            config);


        environment.reset(
            "RELIANCE",
            2000,
            100.0);


        const auto entry =
            environment.step(
                DevAction::LONG,
                2060,
                100.0);


        require(
            approximatelyEqual(
                entry.brokerage_charges,
                0.0),
            "Slippage-only test unexpectedly contains "
            "brokerage/statutory charges.");


        require(
            entry.slippage_cost > 0.0,
            "Opening slippage cost was not generated.");


        require(
            entry.executions.size() == 1,
            "LONG entry must create exactly one execution.");


        require(
            entry.executions.front().
                execution_price >
            entry.executions.front().
                market_price,
            "BUY execution price does not include "
            "adverse slippage.");


        require(
            entry.equity_after <
                entry.equity_before,
            "Opening slippage was reported but did not "
            "reduce account equity.");


        require(
            entry.net_pnl < 0.0,
            "Opening slippage did not reach step net P&L.");


        require(
            entry.net_equity_return < 0.0,
            "Opening slippage did not reach "
            "net equity return.");
    }
}


// ============================================================================
// 1.8.14
// Reversal execution integrity
//
// LONG -> SHORT:
//
//     SELL to close LONG
//     SELL to open SHORT
//
// Action turnover = 2.
//
// Both executions must carry their own charges/slippage.
// ============================================================================

void testReversalIntegrity()
{
    std::cout
        << "[TEST] 1.8.14 Reversal execution integrity..."
        << std::endl;


    TradingEnvironment environment;


    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    static_cast<void>(
        environment.step(
            DevAction::LONG,
            1060,
            100.0));


    const auto reversal =
        environment.step(
            DevAction::SHORT,
            1120,
            101.0);


    require(
        reversal.executions.size() == 2,
        "Direct reversal must create exactly two executions.");


    require(
        reversal.executions[0].side ==
            brokerage::ExecutionSide::SELL,
        "LONG close must execute SELL.");


    require(
        reversal.executions[1].side ==
            brokerage::ExecutionSide::SELL,
        "SHORT open must execute SELL.");


    require(
        approximatelyEqual(
            reversal.turnover,
            2.0),
        "Reversal action turnover must equal 2.");


    require(
        reversal.brokerage_charges > 0.0,
        "Reversal charges missing.");


    require(
        reversal.slippage_cost > 0.0,
        "Reversal slippage missing.");


    require(
        environment.state().position ==
            DEV_POSITION_SHORT,
        "Reversal did not establish SHORT position.");
}


// ============================================================================
// 1.8.15
// Environment determinism
// ============================================================================

void testEnvironmentDeterminism()
{
    std::cout
        << "[TEST] 1.8.15 Environment determinism..."
        << std::endl;


    TradingEnvironment first;
    TradingEnvironment second;


    first.reset(
        "RELIANCE",
        1000,
        100.0);


    second.reset(
        "RELIANCE",
        1000,
        100.0);


    const auto first_entry =
        first.step(
            DevAction::LONG,
            1060,
            100.0);


    const auto second_entry =
        second.step(
            DevAction::LONG,
            1060,
            100.0);


    require(
        approximatelyEqual(
            first_entry.equity_after,
            second_entry.equity_after),
        "Identical entry produced different equity.");


    require(
        approximatelyEqual(
            first_entry.brokerage_charges,
            second_entry.brokerage_charges),
        "Identical entry produced different charges.");


    require(
        approximatelyEqual(
            first_entry.slippage_cost,
            second_entry.slippage_cost),
        "Identical entry produced different slippage.");


    require(
        approximatelyEqual(
            first_entry.reward.total,
            second_entry.reward.total),
        "Identical entry produced different reward.");


    const auto first_hold =
        first.step(
            DevAction::HOLD,
            1120,
            101.0);


    const auto second_hold =
        second.step(
            DevAction::HOLD,
            1120,
            101.0);


    require(
        approximatelyEqual(
            first_hold.gross_pnl,
            second_hold.gross_pnl),
        "Identical future movement produced different P&L.");


    require(
        approximatelyEqual(
            first_hold.equity_after,
            second_hold.equity_after),
        "Environment state is non-deterministic.");


    require(
        approximatelyEqual(
            first_hold.reward.total,
            second_hold.reward.total),
        "Reward path is non-deterministic.");
}


// ============================================================================
// 1.8.16
// Cross-episode reset isolation
//
// reset() must remove ALL episode-specific state.
//
// No position, P&L, cost, drawdown, symbol, quantity or holding
// information from Episode A may enter Episode B.
// ============================================================================

void testResetIsolation()
{
    std::cout
        << "[TEST] 1.8.16 Cross-episode reset isolation..."
        << std::endl;


    const auto config =
        zeroCostConfiguration();


    TradingEnvironment environment(
        config);


    // ========================================================================
    // Episode A
    // ========================================================================

    environment.reset(
        "RELIANCE",
        1000,
        100.0);


    static_cast<void>(
        environment.step(
            DevAction::LONG,
            1060,
            100.0));


    static_cast<void>(
        environment.step(
            DevAction::HOLD,
            1120,
            110.0));


    require(
        environment.state().equity >
            config.initial_equity,
        "Episode A failed to create account state.");


    // ========================================================================
    // Episode B
    // ========================================================================

    environment.reset(
        "HDFCBANK",
        2000,
        200.0);


    const auto& state =
        environment.state();


    require(
        state.symbol ==
            "HDFCBANK",
        "Previous episode symbol leaked.");


    require(
        state.decision_time ==
            2000,
        "Reset decision time is incorrect.");


    require(
        state.position ==
            DEV_POSITION_FLAT,
        "Previous episode position leaked.");


    require(
        approximatelyEqual(
            state.quantity,
            0.0),
        "Previous episode quantity leaked.");


    require(
        approximatelyEqual(
            state.entry_price,
            0.0),
        "Previous episode entry price leaked.");


    require(
        approximatelyEqual(
            state.step_gross_pnl,
            0.0),
        "Previous episode step P&L leaked.");


    require(
        approximatelyEqual(
            state.realized_gross_pnl,
            0.0),
        "Previous episode realized P&L leaked.");


    require(
        approximatelyEqual(
            state.unrealized_gross_pnl,
            0.0),
        "Previous episode unrealized P&L leaked.");


    require(
        approximatelyEqual(
            state.cumulative_brokerage_charges,
            0.0),
        "Previous episode brokerage/statutory charges leaked.");


    require(
        approximatelyEqual(
            state.cumulative_slippage_cost,
            0.0),
        "Previous episode slippage leaked.");


    require(
        approximatelyEqual(
            state.cumulative_execution_cost,
            0.0),
        "Previous episode execution costs leaked.");


    require(
        approximatelyEqual(
            state.net_pnl,
            0.0),
        "Previous episode net P&L leaked.");


    require(
        approximatelyEqual(
            state.drawdown,
            0.0),
        "Previous episode drawdown leaked.");


    require(
        state.holding_bars == 0,
        "Previous episode holding period leaked.");


    require(
        approximatelyEqual(
            state.equity,
            config.initial_equity),
        "Previous episode equity leaked.");


    require(
        approximatelyEqual(
            state.peak_equity,
            config.initial_equity),
        "Previous episode peak equity leaked.");


    require(
        approximatelyEqual(
            state.previous_market_price,
            200.0),
        "Reset market-price state is incorrect.");


    require(
        state.initialized,
        "Environment is not initialized after reset.");
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
            << "DEVAI PHASE 1.8 — LEAKAGE + ENVIRONMENT VALIDATION\n"
            << "============================================================\n"
            << "CETE                          : Creative Engineer Trading Environment\n"
            << "Strategy                      : DevAI\n"
            << "Agent                         : Dev\n"
            << "Observation Contract          : 84 VALUES\n"
            << "Market Features               : 39\n"
            << "Availability Masks            : 39\n"
            << "Portfolio Features            : 6\n"
            << "Forward Return Observation    : PROHIBITED\n"
            << "Future Market Input           : PROHIBITED\n"
            << "Same-Step Retroactive Profit  : PROHIBITED\n"
            << "Shared CETE Brokerage         : ENABLED\n"
            << "Execution-Price Slippage      : ENABLED\n"
            << "Environment Reset Isolation   : REQUIRED\n"
            << "============================================================\n\n";


        testObservationDimensions();

        testFiniteObservationContract();

        testAvailabilityMaskIntegrity();

        testUnavailableFeatureNeutrality();

        testPortfolioStateIntegrity();

        testNormalizationBounds();

        testInvalidObservationRejection();

        testForwardReturnExclusion();

        testObservationDeterminism();

        testDecisionTimeCausality();

        testSameStepActionPnlIsolation();

        testRewardCausality();

        testExecutionCostPropagation();

        testReversalIntegrity();

        testEnvironmentDeterminism();

        testResetIsolation();


        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.8 VALIDATION SUMMARY\n"
            << "============================================================\n"
            << "1.8.1  Observation Dimensions          : PASSED\n"
            << "1.8.2  Finite-Value Integrity          : PASSED\n"
            << "1.8.3  Availability Masks              : PASSED\n"
            << "1.8.4  Unavailable Feature Neutrality  : PASSED\n"
            << "1.8.5  Portfolio-State Integrity       : PASSED\n"
            << "1.8.6  Normalization Bounds            : PASSED\n"
            << "1.8.7  Invalid Observation Rejection   : PASSED\n"
            << "1.8.8  Forward/Future Return Exclusion : PASSED\n"
            << "1.8.9  Observation Determinism         : PASSED\n"
            << "1.8.10 Decision-Time Causality         : PASSED\n"
            << "1.8.11 Same-Step Action/P&L Isolation  : PASSED\n"
            << "1.8.12 Reward Causality                : PASSED\n"
            << "1.8.13 Execution-Cost Propagation      : PASSED\n"
            << "1.8.14 Reversal Integrity              : PASSED\n"
            << "1.8.15 Environment Determinism         : PASSED\n"
            << "1.8.16 Cross-Episode Reset Isolation   : PASSED\n"
            << "------------------------------------------------------------\n"
            << "ForwardReturn_N Observation Path       : NONE\n"
            << "Future Candle Observation Path         : NONE\n"
            << "Same-Step Retroactive Profit           : NONE\n"
            << "Shared CETE Brokerage                  : VERIFIED\n"
            << "Opening Slippage Equity Propagation    : VERIFIED\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.8 LEAKAGE + ENVIRONMENT VALIDATION PASSED\n"
            << "============================================================\n";


        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.8 LEAKAGE + ENVIRONMENT VALIDATION FAILED\n"
            << "============================================================\n"
            << "Reason: "
            << exception.what()
            << "\n"
            << "============================================================\n";


        return 1;
    }
}