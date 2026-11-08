#include "devai/market/MarketDataLoader.hpp"

#include "devai/strategy/accounting/PnLAccountingIntegration.hpp"
#include "devai/strategy/risk/RiskStateEngine.hpp"

#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"
#include "devai/strategy/dev_ai_strategy/TerminalConditionEngine.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include "devai/strategy/outcome/ForwardReturnOutcomeEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{

using namespace devai::strategy;
using namespace devai::strategy::accounting;
using namespace devai::strategy::risk;
using namespace devai::strategy::outcome;
using namespace devai::market;


// ============================================================================
// Configuration
// ============================================================================

constexpr const char* DATA_FOLDER =
    "/home/hadoop/shareMarket_Data/minute";

constexpr const char* BENCHMARK_SYMBOL =
    "NIFTY%2050";

constexpr std::size_t MINIMUM_UNIVERSE_SIZE = 2;

// Keep Phase 2.10 deliberately bounded.
//
// HistoricalCETEReplay reconstructs the causal CETE pipeline independently
// for every replayAt() call. A short sequence is enough to prove integration
// without turning this validation into a full backtest.
constexpr std::size_t EPISODE_BAR_COUNT = 8;

constexpr std::size_t FORWARD_HORIZON = 3;

constexpr std::int64_t ONE_MINUTE_SECONDS = 60;

constexpr double INITIAL_EQUITY = 100000.0;


// ============================================================================
// Validation statistics
// ============================================================================

struct ValidationStats
{
    std::size_t checks{0};
    std::size_t failures{0};

    std::size_t replay_points{0};
    std::size_t observations_built{0};
    std::size_t trading_steps{0};

    std::size_t accounting_reconciliations{0};
    std::size_t risk_reconciliations{0};
    std::size_t terminal_evaluations{0};

    std::size_t available_forward_outcomes{0};
    std::size_t unavailable_forward_outcomes{0};

    std::size_t chronology_violations{0};
    std::size_t observation_violations{0};
    std::size_t accounting_violations{0};
    std::size_t risk_violations{0};
    std::size_t terminal_violations{0};
    std::size_t forward_outcome_violations{0};
};


// ============================================================================
// Test helpers
// ============================================================================

void require(
    bool condition,
    const std::string& message,
    ValidationStats& stats)
{
    ++stats.checks;

    if (!condition)
    {
        ++stats.failures;

        std::cerr
            << "[FAIL] "
            << message
            << '\n';

        throw std::runtime_error(
            message);
    }

    std::cout
        << "[PASS] "
        << message
        << '\n';
}


bool approximatelyEqual(
    double left,
    double right,
    double absolute_tolerance = 1.0e-8,
    double relative_tolerance = 1.0e-10)
{
    if (!std::isfinite(left) ||
        !std::isfinite(right))
    {
        return false;
    }

    const double difference =
        std::fabs(left - right);

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
// Find candle corresponding to a decision time
//
// CETE convention:
//
//     decision_time = source 1-minute candle timestamp + 60 seconds
//
// Therefore market price visible at decision time t is the close of the
// completed candle timestamped t - 60.
// ============================================================================

double priceAtDecisionTime(
    const std::vector<Candle>& candles,
    std::int64_t decision_time)
{
    const std::int64_t required_timestamp =
        decision_time -
        ONE_MINUTE_SECONDS;

    const auto iterator =
        std::lower_bound(
            candles.begin(),
            candles.end(),
            required_timestamp,
            [](
                const Candle& candle,
                std::int64_t timestamp)
            {
                return
                    candle.timestamp <
                    timestamp;
            });

    if (iterator == candles.end() ||
        iterator->timestamp !=
            required_timestamp)
    {
        throw std::runtime_error(
            "No exact 1-minute candle exists for decision time.");
    }

    if (!std::isfinite(
            iterator->close) ||
        iterator->close <= 0.0)
    {
        throw std::runtime_error(
            "Decision-time market price is invalid.");
    }

    return iterator->close;
}


// ============================================================================
// Deterministic validation action policy
//
// IMPORTANT:
//
// This is NOT Dev's learned policy.
//
// Phase 2.10 needs position changes so that real historical execution,
// brokerage, slippage, accounting and risk paths are exercised.
//
// Sequence:
//
//     step 0 : LONG
//     step 1 : HOLD
//     step 2 : HOLD
//     step 3 : SHORT       direct reversal
//     step 4 : HOLD
//     step 5 : EXIT
//     later  : HOLD
//
// No market feature, future return or future price is used to select actions.
// ============================================================================

DevAction validationAction(
    std::size_t step_index)
{
    switch (step_index)
    {
        case 0:
            return DevAction::LONG;

        case 3:
            return DevAction::SHORT;

        case 5:
            return DevAction::EXIT;

        default:
            return DevAction::HOLD;
    }
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
        << "DEVAI PHASE 2.10 — REAL HISTORICAL EPISODE VALIDATION\n"
        << "======================================================================\n";


    // ========================================================================
    // 1. Historical CETE replay configuration
    // ========================================================================

    HistoricalCETEReplayConfig replay_config;

    replay_config.data_folder =
        DATA_FOLDER;

    replay_config.benchmark_symbol =
        BENCHMARK_SYMBOL;

    replay_config.minimum_universe_size =
        MINIMUM_UNIVERSE_SIZE;

    // Two stocks are sufficient for structural cross-sectional validation.
    // This keeps repeated historical reconstruction practical.
    replay_config.maximum_stock_count =
        2;


    HistoricalCETEReplay replay(
        replay_config);


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


    std::cout
        << "\nSelected validation symbol : "
        << traded_symbol
        << "\nBenchmark                  : "
        << BENCHMARK_SYMBOL
        << "\n";


    // ========================================================================
    // 2. Load genuine real 1-minute history for the traded stock
    // ========================================================================

    MarketDataLoader loader(
        DATA_FOLDER);


    const std::filesystem::path stock_path =
        std::filesystem::path(DATA_FOLDER) /
        (traded_symbol +
         "_1min.txt");


    const auto stock_load =
        loader.loadPath(
            stock_path);


    require(
        stock_load.candles.size() >
            EPISODE_BAR_COUNT,
        "Real 1-minute history contains enough candles",
        stats);


    // ========================================================================
    // 3. Determine latest common causal decision time
    // ========================================================================

    const std::int64_t latest_common_time =
        replay.latestCommonDecisionTime();


    require(
        latest_common_time > 0,
        "Latest common historical decision time is valid",
        stats);


    // ========================================================================
    // 4. Select the final real consecutive candles ending at/before the
    //    common CETE decision time.
    //
    //    We deliberately derive decision times from actual stock candles.
    //    No synthetic timestamps are generated.
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
            EPISODE_BAR_COUNT)
        {
            break;
        }
    }


    require(
        candidate_candles.size() ==
            EPISODE_BAR_COUNT,
        "Selected required number of genuine historical bars",
        stats);


    std::reverse(
        candidate_candles.begin(),
        candidate_candles.end());


    // ========================================================================
    // 5. Build real causal Phase-5 snapshots
    // ========================================================================

    std::vector<DevEpisode::Snapshot>
        snapshots;

    snapshots.reserve(
        candidate_candles.size());


    std::vector<ForwardReturnBar>
        outcome_bars;

    outcome_bars.reserve(
        candidate_candles.size());


    std::int64_t previous_decision_time =
        0;


    for (const Candle* candle :
         candidate_candles)
    {
        const std::int64_t decision_time =
            candle->timestamp +
            ONE_MINUTE_SECONDS;


        if (previous_decision_time != 0 &&
            decision_time <=
                previous_decision_time)
        {
            ++stats.chronology_violations;
        }


        const auto point =
            replay.replayAt(
                decision_time);


        ++stats.replay_points;


        require(
            point.snapshot.decision_time ==
                decision_time,
            "Replay snapshot decision time matches requested real time",
            stats);


        require(
            point.snapshot.benchmark_symbol ==
                BENCHMARK_SYMBOL,
            "Replay snapshot benchmark identity is correct",
            stats);


        require(
            point.snapshot.stock_count >=
                MINIMUM_UNIVERSE_SIZE,
            "Replay snapshot contains sufficient stocks",
            stats);


        require(
            point.snapshot.numerical_integrity,
            "Replay snapshot has numerical integrity",
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
            point.stats.benchmark_contamination ==
                0,
            "Historical CETE replay reports zero benchmark contamination",
            stats);


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
            "Traded stock state is aligned to current decision time",
            stats);


        snapshots.push_back(
            point.snapshot);


        outcome_bars.push_back(
            ForwardReturnBar{
                decision_time,
                candle->close
            });


        previous_decision_time =
            decision_time;
    }


    require(
        stats.chronology_violations == 0,
        "Real replay sequence is strictly chronological",
        stats);


    // ========================================================================
    // 6. Construct DevEpisode
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
        "DevEpisode initializes from real CETE snapshots",
        stats);


    require(
        episode.totalSteps() ==
            snapshots.size(),
        "DevEpisode preserves complete historical sequence",
        stats);


    require(
        episode.startDecisionTime() ==
            snapshots.front().decision_time,
        "DevEpisode start time matches first real snapshot",
        stats);


    require(
        episode.endDecisionTime() ==
            snapshots.back().decision_time,
        "DevEpisode end time matches final real snapshot",
        stats);


    require(
        episode.benchmarkSymbol() ==
            BENCHMARK_SYMBOL,
        "DevEpisode preserves benchmark identity",
        stats);


    // ========================================================================
    // 7. Trading environment
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


    // TradingEnvironment requires every step() decision time to be strictly
    // newer than its reset time. Therefore initialize it from the genuine
    // historical decision immediately preceding the first episode decision.
    const std::int64_t first_episode_decision_time =
        episode.currentDecisionTime();

    const std::int64_t environment_reset_time =
        first_episode_decision_time -
        ONE_MINUTE_SECONDS;

    const double environment_reset_price =
        priceAtDecisionTime(
            stock_load.candles,
            environment_reset_time);

    const double first_episode_market_price =
        priceAtDecisionTime(
            stock_load.candles,
            first_episode_decision_time);


    environment.reset(
        traded_symbol,
        environment_reset_time,
        environment_reset_price);


    require(
        environment.state().initialized,
        "TradingEnvironment initializes on genuine preceding historical price",
        stats);


    require(
        environment.state().decision_time ==
            environment_reset_time,
        "TradingEnvironment reset time precedes first episode decision",
        stats);


    require(
        first_episode_decision_time >
            environment.state().decision_time,
        "First episode decision is strictly newer than environment reset",
        stats);


    require(
        std::isfinite(first_episode_market_price) &&
            first_episode_market_price > 0.0,
        "First episode market price is valid",
        stats);


    // ========================================================================
    // 8. Shared CETE accounting
    // ========================================================================

    PnLAccountingIntegration
        accounting;


    // ========================================================================
    // 9. Shared CETE risk state
    // ========================================================================

    RiskStateEngine
        risk_engine;


    // Initialize shared CETE risk at the same genuine pre-episode account
    // state used by TradingEnvironment. This preserves the initial-equity peak
    // before first-action execution costs/slippage are applied at T0.
    RiskStateInput
        initial_risk_input;

    initial_risk_input.decision_time =
        environment_reset_time;

    initial_risk_input.initial_equity =
        environment.state().initial_equity;

    initial_risk_input.current_equity =
        environment.state().equity;

    initial_risk_input.position =
        environment.state().position;

    initial_risk_input.quantity =
        environment.state().quantity;

    initial_risk_input.current_price =
        environment_reset_price;


    risk_engine.reset(
        initial_risk_input);


    require(
        risk_engine.state().numerical_integrity,
        "RiskStateEngine initializes from genuine preceding account state",
        stats);


    require(
        risk_engine.state().decision_time ==
            environment_reset_time,
        "RiskStateEngine reset time matches TradingEnvironment reset time",
        stats);


    require(
        approximatelyEqual(
            risk_engine.state().peak_equity,
            environment.state().peak_equity),
        "RiskStateEngine initial peak equity matches TradingEnvironment",
        stats);


    // ========================================================================
    // 10. Terminal policy
    //
    // Risk/equity policies are deliberately disabled here. The objective of
    // this test is to traverse the complete real historical sequence and prove
    // END_OF_DATA termination.
    // ========================================================================

    TerminalConditionConfig
        terminal_config;

    terminal_config.maximum_episode_steps =
        0;

    terminal_config.maximum_drawdown =
        0.0;

    terminal_config.minimum_equity_fraction =
        0.0;


    TerminalConditionEngine
        terminal_engine(
            terminal_config);


    // ========================================================================
    // 11. Observation builder
    // ========================================================================

    ObservationBuilder
        observation_builder;


    // ========================================================================
    // 12. Real historical episode loop
    // ========================================================================

    std::size_t completed_steps =
        0;


    while (true)
    {
        const auto&
            snapshot =
                episode.currentSnapshot();


        const std::int64_t decision_time =
            episode.currentDecisionTime();


        const double market_price =
            priceAtDecisionTime(
                stock_load.candles,
                decision_time);


        // --------------------------------------------------------------------
        // Observation BEFORE current action.
        //
        // This is the causal state Dev would actually see.
        // --------------------------------------------------------------------

        const DevPortfolioState
            portfolio_before =
                environment.portfolioState();


        const DevAIObservation
            observation =
                observation_builder.build(
                    snapshot,
                    traded_symbol,
                    portfolio_before);


        ++stats.observations_built;


        if (!observation.numerical_integrity ||
            !observation.ready)
        {
            ++stats.observation_violations;
        }


        require(
            observation.numerical_integrity,
            "Current real Dev observation has numerical integrity",
            stats);


        require(
            observation.ready,
            "Current real Dev observation is ready",
            stats);


        for (const double value :
             observation.market)
        {
            if (!std::isfinite(value))
            {
                ++stats.observation_violations;
            }
        }


        for (const double mask :
             observation.availability)
        {
            if (!std::isfinite(mask) ||
                (mask != 0.0 &&
                 mask != 1.0))
            {
                ++stats.observation_violations;
            }
        }


        // --------------------------------------------------------------------
        // Deterministic validation action.
        //
        // It depends ONLY on current step index.
        // No ForwardReturnOutcome exists in this execution path.
        // --------------------------------------------------------------------

        const DevAction action =
            validationAction(
                episode.stepIndex());


        const TradingStepResult
            step_result =
                environment.step(
                    action,
                    decision_time,
                    market_price);


        ++stats.trading_steps;
        ++completed_steps;


        require(
            step_result.decision_time ==
                decision_time,
            "Trading step remains aligned with current historical decision time",
            stats);


        require(
            approximatelyEqual(
                step_result.market_price,
                market_price),
            "TradingEnvironment uses exact current real market price",
            stats);


        // --------------------------------------------------------------------
        // Shared CETE P&L reconciliation
        // --------------------------------------------------------------------

        const auto&
            environment_state =
                environment.state();


        PnLReconciliationInput
            reconciliation_input;


        reconciliation_input.integration.
            initial_equity =
                environment_state.
                    initial_equity;


        // TradingEnvironment marks the current position from its
        // slippage-adjusted execution price. Therefore its realized +
        // unrealized gross P&L already contains the economic effect of
        // execution slippage. PnLAccountingIntegration, however, requires
        // gross P&L BEFORE slippage and receives slippage separately.
        //
        // Restore pre-slippage gross here so the shared accounting layer
        // deducts cumulative slippage exactly once.
        const double pre_slippage_gross_pnl =
            environment_state.realized_gross_pnl +
            environment_state.unrealized_gross_pnl +
            environment_state.cumulative_slippage_cost;


        reconciliation_input.integration.
            realized_gross_pnl =
                pre_slippage_gross_pnl;


        reconciliation_input.integration.
            unrealized_gross_pnl =
                0.0;


        // TradingEnvironment keeps brokerage/statutory charges separately
        // from slippage.
        reconciliation_input.integration.
            transaction_costs =
                environment_state.
                    cumulative_brokerage_charges;


        reconciliation_input.integration.
            slippage_cost =
                environment_state.
                    cumulative_slippage_cost;


        reconciliation_input.expected_net_pnl =
            environment_state.net_pnl;


        reconciliation_input.expected_equity =
            environment_state.equity;


        const auto reconciliation =
            accounting.reconcile(
                reconciliation_input);


        ++stats.accounting_reconciliations;


        if (!reconciliation.reconciled)
        {
            ++stats.accounting_violations;
        }


        require(
            reconciliation.reconciled,
            "Shared CETE accounting reconciles with TradingEnvironment",
            stats);


        require(
            reconciliation.integration.
                numerical_integrity,
            "Shared CETE accounting retains numerical integrity",
            stats);


        // --------------------------------------------------------------------
        // Shared CETE risk reconciliation
        // --------------------------------------------------------------------

        RiskStateInput
            risk_input;


        risk_input.decision_time =
            decision_time;

        risk_input.initial_equity =
            environment_state.
                initial_equity;

        risk_input.current_equity =
            environment_state.
                equity;

        risk_input.position =
            environment_state.
                position;

        risk_input.quantity =
            environment_state.
                quantity;

        risk_input.current_price =
            market_price;


        // RiskStateEngine already represents the genuine pre-episode state
        // at T-1. Therefore every episode decision advances it strictly
        // forward: update(T0), update(T1), update(T2), ...
        const RiskState&
            risk_state =
                risk_engine.update(
                    risk_input);


        ++stats.risk_reconciliations;


        if (!risk_state.
                numerical_integrity)
        {
            ++stats.risk_violations;
        }


        require(
            risk_state.
                numerical_integrity,
            "Shared CETE risk state has numerical integrity",
            stats);


        require(
            risk_state.
                decision_time ==
                environment_state.
                    decision_time,
            "Risk decision time reconciles with TradingEnvironment",
            stats);


        require(
            approximatelyEqual(
                risk_state.
                    initial_equity,
                environment_state.
                    initial_equity),
            "Risk initial equity reconciles with TradingEnvironment",
            stats);


        require(
            approximatelyEqual(
                risk_state.
                    current_equity,
                environment_state.
                    equity),
            "Risk equity reconciles with TradingEnvironment",
            stats);


        require(
            approximatelyEqual(
                risk_state.
                    current_drawdown,
                environment_state.
                    drawdown),
            "Risk drawdown reconciles with TradingEnvironment",
            stats);


        require(
            approximatelyEqual(
                risk_state.
                    position,
                environment_state.
                    position),
            "Risk position reconciles with TradingEnvironment",
            stats);


        require(
            approximatelyEqual(
                risk_state.
                    quantity,
                environment_state.
                    quantity),
            "Risk quantity reconciles with TradingEnvironment",
            stats);


        // --------------------------------------------------------------------
        // Terminal-condition evaluation
        // --------------------------------------------------------------------

        const bool has_next_state =
            episode.stepIndex() + 1 <
            episode.totalSteps();


        TerminalConditionInput
            terminal_input;


        terminal_input.decision_time =
            decision_time;

        terminal_input.completed_steps =
            completed_steps;

        terminal_input.has_next_state =
            has_next_state;

        terminal_input.initial_equity =
            environment_state.
                initial_equity;

        terminal_input.current_equity =
            environment_state.
                equity;

        terminal_input.current_drawdown =
            risk_state.
                current_drawdown;

        terminal_input.maximum_drawdown =
            risk_state.
                maximum_drawdown;


        const TerminalConditionResult
            terminal_result =
                terminal_engine.evaluate(
                    terminal_input);


        ++stats.terminal_evaluations;


        require(
            terminal_result.
                numerical_integrity,
            "Terminal-condition result has numerical integrity",
            stats);


        if (has_next_state)
        {
            if (terminal_result.terminal)
            {
                ++stats.terminal_violations;
            }


            require(
                !terminal_result.terminal,
                "Intermediate historical state does not terminate episode",
                stats);


            require(
                terminal_result.reason ==
                    TerminalReason::NONE,
                "Intermediate terminal reason is NONE",
                stats);
        }
        else
        {
            if (!terminal_result.terminal ||
                terminal_result.reason !=
                    TerminalReason::END_OF_DATA)
            {
                ++stats.terminal_violations;
            }


            require(
                terminal_result.terminal,
                "Final historical state terminates episode",
                stats);


            require(
                terminal_result.reason ==
                    TerminalReason::END_OF_DATA,
                "Final termination reason is END_OF_DATA",
                stats);


            break;
        }


        // --------------------------------------------------------------------
        // Advance only AFTER current observation/action/account/risk/terminal
        // processing is complete.
        // --------------------------------------------------------------------

        const std::int64_t old_time =
            episode.currentDecisionTime();


        episode.advance();


        require(
            episode.currentDecisionTime() >
                old_time,
            "DevEpisode advances strictly forward in historical time",
            stats);
    }


    // ========================================================================
    // 13. Forward-return outcomes
    //
    // IMPORTANT:
    //
    // This occurs AFTER the historical episode execution loop.
    //
    // ForwardReturnOutcome was never passed to:
    //
    //     ObservationBuilder
    //     TradingEnvironment
    //     RiskStateEngine
    //     TerminalConditionEngine
    //
    // This preserves the anti-leakage boundary.
    // ========================================================================

    ForwardReturnOutcomeEngine
        outcome_engine;


    const auto outcomes =
        outcome_engine.evaluateAll(
            outcome_bars,
            FORWARD_HORIZON);


    require(
        outcomes.size() ==
            outcome_bars.size(),
        "Forward outcome engine preserves one record per origin bar",
        stats);


    for (std::size_t i = 0;
         i < outcomes.size();
         ++i)
    {
        const auto& outcome =
            outcomes[i];


        require(
            outcome.origin_index == i,
            "Forward outcome origin index is correct",
            stats);


        require(
            outcome.origin_time ==
                outcome_bars[i].
                    decision_time,
            "Forward outcome origin time is correct",
            stats);


        if (i + FORWARD_HORIZON <
            outcome_bars.size())
        {
            ++stats.available_forward_outcomes;


            require(
                outcome.available,
                "Matured historical forward outcome is available",
                stats);


            require(
                outcome.numerical_integrity,
                "Matured forward outcome has numerical integrity",
                stats);


            require(
                outcome.outcome_index ==
                    i + FORWARD_HORIZON,
                "Forward outcome uses exact N-bar horizon",
                stats);


            const double expected_return =
                outcome_bars[
                    i + FORWARD_HORIZON].
                        close /
                outcome_bars[i].close -
                1.0;


            if (!approximatelyEqual(
                    outcome.forward_return,
                    expected_return))
            {
                ++stats.
                    forward_outcome_violations;
            }


            require(
                approximatelyEqual(
                    outcome.forward_return,
                    expected_return),
                "ForwardReturn_N matches exact realized historical return",
                stats);
        }
        else
        {
            ++stats.unavailable_forward_outcomes;


            require(
                !outcome.available,
                "Incomplete future horizon remains unavailable",
                stats);


            require(
                outcome.outcome_time == 0 &&
                outcome.outcome_close == 0.0 &&
                outcome.forward_return == 0.0,
                "Unavailable outcome does not fabricate future data",
                stats);
        }
    }


    require(
        stats.unavailable_forward_outcomes ==
            FORWARD_HORIZON,
        "Exactly final N origins remain unavailable",
        stats);


    // ========================================================================
    // 14. Episode reset / isolation
    // ========================================================================

    episode.reset();


    require(
        episode.stepIndex() == 0,
        "DevEpisode reset returns to first historical state",
        stats);


    require(
        episode.currentDecisionTime() ==
            snapshots.front().
                decision_time,
        "DevEpisode reset restores original start time",
        stats);


    RiskStateEngine
        fresh_risk_engine;


    RiskStateInput
        fresh_risk_input;


    fresh_risk_input.decision_time =
        snapshots.front().
            decision_time;

    fresh_risk_input.initial_equity =
        INITIAL_EQUITY;

    fresh_risk_input.current_equity =
        INITIAL_EQUITY;

    fresh_risk_input.position =
        0.0;

    fresh_risk_input.quantity =
        0.0;

    fresh_risk_input.current_price =
        first_episode_market_price;


    fresh_risk_engine.reset(
        fresh_risk_input);


    require(
        approximatelyEqual(
            fresh_risk_engine.state().
                current_drawdown,
            0.0),
        "New episode starts with zero current drawdown",
        stats);


    require(
        approximatelyEqual(
            fresh_risk_engine.state().
                maximum_drawdown,
            0.0),
        "New episode starts with zero maximum drawdown",
        stats);


    // ========================================================================
    // Final invariants
    // ========================================================================

    require(
        stats.observation_violations == 0,
        "Zero observation-integrity violations",
        stats);


    require(
        stats.accounting_violations == 0,
        "Zero accounting reconciliation violations",
        stats);


    require(
        stats.risk_violations == 0,
        "Zero risk reconciliation violations",
        stats);


    require(
        stats.terminal_violations == 0,
        "Zero terminal-condition violations",
        stats);


    require(
        stats.forward_outcome_violations == 0,
        "Zero forward-outcome violations",
        stats);


    // ========================================================================
    // Report
    // ========================================================================

    std::cout
        << "\n"
        << "======================================================================\n"
        << "PHASE 2.10 VALIDATION SUMMARY\n"
        << "======================================================================\n"
        << "Traded symbol                 : "
        << traded_symbol
        << '\n'
        << "Universe stocks               : "
        << symbols.size()
        << '\n'
        << "Historical replay points      : "
        << stats.replay_points
        << '\n'
        << "Observations built            : "
        << stats.observations_built
        << '\n'
        << "Trading steps                 : "
        << stats.trading_steps
        << '\n'
        << "Accounting reconciliations    : "
        << stats.accounting_reconciliations
        << '\n'
        << "Risk reconciliations          : "
        << stats.risk_reconciliations
        << '\n'
        << "Terminal evaluations          : "
        << stats.terminal_evaluations
        << '\n'
        << "Available forward outcomes    : "
        << stats.available_forward_outcomes
        << '\n'
        << "Unavailable forward outcomes  : "
        << stats.unavailable_forward_outcomes
        << '\n'
        << "Chronology violations         : "
        << stats.chronology_violations
        << '\n'
        << "Observation violations        : "
        << stats.observation_violations
        << '\n'
        << "Accounting violations         : "
        << stats.accounting_violations
        << '\n'
        << "Risk violations               : "
        << stats.risk_violations
        << '\n'
        << "Terminal violations           : "
        << stats.terminal_violations
        << '\n'
        << "Forward-outcome violations    : "
        << stats.forward_outcome_violations
        << '\n'
        << "Total checks                  : "
        << stats.checks
        << '\n'
        << "Failures                      : "
        << stats.failures
        << "\n\n"
        << "Anti-leakage contract\n"
        << "----------------------------------------------------------------------\n"
        << "ForwardReturn_N in observation : NO\n"
        << "Future price in observation    : NO\n"
        << "Future state exposed to Dev    : NO\n"
        << "Outcome calculated post-loop   : YES\n"
        << "Exact N-bar maturity required  : YES\n"
        << "\n"
        << "Economic reconciliation\n"
        << "----------------------------------------------------------------------\n"
        << "Brokerage / statutory costs    : TradingEnvironment -> CETE Accounting\n"
        << "Slippage cost                  : TradingEnvironment -> CETE Accounting\n"
        << "P&L / equity reconciliation    : VERIFIED\n"
        << "Risk / drawdown reconciliation : VERIFIED\n"
        << "\n"
        << "Episode behavior\n"
        << "----------------------------------------------------------------------\n"
        << "Real historical data           : YES\n"
        << "Synthetic future data          : NO\n"
        << "Chronological advancement      : VERIFIED\n"
        << "END_OF_DATA termination        : VERIFIED\n"
        << "Episode reset isolation        : VERIFIED\n"
        << "======================================================================\n";
}

} // namespace


// ============================================================================
// main
// ============================================================================

int main()
{
    try
    {
        runValidation();

        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 2.10 REAL HISTORICAL EPISODE VALIDATION: PASSED\n"
            << "============================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 2.10 REAL HISTORICAL EPISODE VALIDATION: FAILED\n"
            << "============================================================\n"
            << "Reason: "
            << exception.what()
            << '\n';

        return 1;
    }
}