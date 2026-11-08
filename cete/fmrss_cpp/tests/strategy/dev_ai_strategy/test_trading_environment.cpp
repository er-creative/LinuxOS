#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using namespace devai::strategy;


// ============================================================
// TEST HELPERS
// ============================================================

void require(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}


bool approximatelyEqual(
    double lhs,
    double rhs,
    double tolerance = 1e-8)
{
    return
        std::abs(lhs - rhs) <= tolerance;
}


TradingEnvironmentConfig
zeroCostConfiguration()
{
    TradingEnvironmentConfig config;

    config.slippage_rate = 0.0;

    config.brokerage_config.brokerage_rate = 0.0;

    config.brokerage_config.
        brokerage_cap_per_order = 0.0;

    config.brokerage_config.
        stt_sell_rate = 0.0;

    config.brokerage_config.
        exchange_transaction_rate = 0.0;

    config.brokerage_config.
        sebi_rate = 0.0;

    config.brokerage_config.
        stamp_duty_buy_rate = 0.0;

    config.brokerage_config.
        gst_rate = 0.0;

    return config;
}


// ============================================================
// RESET
// ============================================================

void testReset()
{
    std::cout
        << "[TEST] Environment reset..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto& state =
        environment.state();

    require(
        state.initialized,
        "Environment not initialized.");

    require(
        state.symbol == "RELIANCE",
        "Symbol mismatch.");

    require(
        state.position ==
            DEV_POSITION_FLAT,
        "Initial position not flat.");

    require(
        approximatelyEqual(
            state.equity,
            100000.0),
        "Initial equity mismatch.");

    require(
        approximatelyEqual(
            state.net_pnl,
            0.0),
        "Initial net P&L mismatch.");
}


// ============================================================
// FLAT HOLD
// ============================================================

void testFlatHold()
{
    std::cout
        << "[TEST] Flat HOLD..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::HOLD,
            1060,
            105.0);

    require(
        result.executions.empty(),
        "Flat HOLD created execution.");

    require(
        approximatelyEqual(
            result.gross_pnl,
            0.0),
        "Flat HOLD generated P&L.");

    require(
        approximatelyEqual(
            result.net_pnl,
            0.0),
        "Flat HOLD changed equity.");
}


// ============================================================
// LONG ENTRY
// ============================================================

void testLongEntry()
{
    std::cout
        << "[TEST] LONG entry..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);

    require(
        result.executions.size() == 1,
        "LONG entry must create one execution.");

    require(
        result.executions[0].side ==
            brokerage::ExecutionSide::BUY,
        "LONG entry must be BUY.");

    require(
        environment.state().position ==
            DEV_POSITION_LONG,
        "LONG position not established.");

    require(
        result.executions[0].
            execution_price > 100.0,
        "BUY slippage not applied.");

    require(
        result.brokerage_charges > 0.0,
        "Brokerage/statutory charges missing.");

    require(
        result.slippage_cost > 0.0,
        "Slippage missing.");

    require(
        result.equity_after <
            result.equity_before,
        "Entry charges not reflected in equity.");
}


// ============================================================
// SHORT ENTRY
// ============================================================

void testShortEntry()
{
    std::cout
        << "[TEST] SHORT entry..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::SHORT,
            1060,
            100.0);

    require(
        result.executions.size() == 1,
        "SHORT entry must create one execution.");

    require(
        result.executions[0].side ==
            brokerage::ExecutionSide::SELL,
        "SHORT entry must be SELL.");

    require(
        result.executions[0].
            execution_price < 100.0,
        "SELL slippage not applied.");

    require(
        environment.state().position ==
            DEV_POSITION_SHORT,
        "SHORT position not established.");
}


// ============================================================
// ANTI-LOOKAHEAD
// ============================================================

void testNoSameStepFutureProfit()
{
    std::cout
        << "[TEST] Same-step future-profit protection..."
        << std::endl;

    auto config =
        zeroCostConfiguration();

    TradingEnvironment environment(config);

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::LONG,
            1060,
            110.0);

    // Price moved 100 -> 110 before LONG action.
    //
    // Dev was flat during that move.
    require(
        approximatelyEqual(
            result.gross_pnl,
            0.0),
        "Action received already-observed price movement.");

    require(
        approximatelyEqual(
            environment.state().equity,
            100000.0),
        "Look-ahead P&L detected.");
}


// ============================================================
// LONG P&L
// ============================================================

void testLongGrossPnl()
{
    std::cout
        << "[TEST] LONG gross P&L..."
        << std::endl;

    auto config =
        zeroCostConfiguration();

    TradingEnvironment environment(config);

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    static_cast<void>(
        environment.step(
            DevAction::LONG,
            1060,
            100.0));

    const auto result =
        environment.step(
            DevAction::HOLD,
            1120,
            101.0);

    // 100000 / 100 = 1000 shares
    // +1 move = +1000

    require(
        approximatelyEqual(
            result.gross_pnl,
            1000.0),
        "LONG gross P&L mismatch.");

    require(
        approximatelyEqual(
            environment.state().
                unrealized_gross_pnl,
            1000.0),
        "LONG unrealized P&L mismatch.");
}


// ============================================================
// SHORT P&L
// ============================================================

void testShortGrossPnl()
{
    std::cout
        << "[TEST] SHORT gross P&L..."
        << std::endl;

    auto config =
        zeroCostConfiguration();

    TradingEnvironment environment(config);

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    static_cast<void>(
        environment.step(
            DevAction::SHORT,
            1060,
            100.0));

    const auto result =
        environment.step(
            DevAction::HOLD,
            1120,
            99.0);

    require(
        approximatelyEqual(
            result.gross_pnl,
            1000.0),
        "SHORT gross P&L mismatch.");
}


// ============================================================
// REALIZED / UNREALIZED
// ============================================================

void testRealizedUnrealized()
{
    std::cout
        << "[TEST] Realized/unrealized P&L..."
        << std::endl;

    auto config =
        zeroCostConfiguration();

    TradingEnvironment environment(config);

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
            102.0));

    require(
        approximatelyEqual(
            environment.state().
                unrealized_gross_pnl,
            2000.0),
        "Intermediate unrealized P&L mismatch.");

    const auto exit =
        environment.step(
            DevAction::EXIT,
            1180,
            105.0);

    require(
        approximatelyEqual(
            exit.gross_pnl,
            3000.0),
        "Final step gross P&L mismatch.");

    require(
        approximatelyEqual(
            environment.state().
                realized_gross_pnl,
            5000.0),
        "Realized gross P&L mismatch.");

    require(
        approximatelyEqual(
            environment.state().
                unrealized_gross_pnl,
            0.0),
        "Unrealized P&L not cleared.");

    require(
        approximatelyEqual(
            environment.state().equity,
            105000.0),
        "Equity double counting detected.");
}


// ============================================================
// LONG -> SHORT
// ============================================================

void testLongToShortReversal()
{
    std::cout
        << "[TEST] LONG -> SHORT reversal..."
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

    const auto result =
        environment.step(
            DevAction::SHORT,
            1120,
            101.0);

    require(
        result.executions.size() == 2,
        "LONG -> SHORT must have two executions.");

    require(
        result.executions[0].side ==
            brokerage::ExecutionSide::SELL,
        "LONG close must be SELL.");

    require(
        result.executions[1].side ==
            brokerage::ExecutionSide::SELL,
        "SHORT open must be SELL.");

    require(
        approximatelyEqual(
            result.turnover,
            2.0),
        "Reversal action turnover mismatch.");

    require(
        result.brokerage_charges > 0.0,
        "Reversal brokerage missing.");

    require(
        result.slippage_cost > 0.0,
        "Reversal slippage missing.");

    require(
        environment.state().position ==
            DEV_POSITION_SHORT,
        "LONG -> SHORT failed.");
}


// ============================================================
// SHORT -> LONG
// ============================================================

void testShortToLongReversal()
{
    std::cout
        << "[TEST] SHORT -> LONG reversal..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    static_cast<void>(
        environment.step(
            DevAction::SHORT,
            1060,
            100.0));

    const auto result =
        environment.step(
            DevAction::LONG,
            1120,
            99.0);

    require(
        result.executions.size() == 2,
        "SHORT -> LONG must have two executions.");

    require(
        result.executions[0].side ==
            brokerage::ExecutionSide::BUY,
        "SHORT close must be BUY.");

    require(
        result.executions[1].side ==
            brokerage::ExecutionSide::BUY,
        "LONG open must be BUY.");

    require(
        environment.state().position ==
            DEV_POSITION_LONG,
        "SHORT -> LONG failed.");
}


// ============================================================
// BROKERAGE INTEGRATION
// ============================================================

void testBrokerageIntegration()
{
    std::cout
        << "[TEST] Shared brokerage integration..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);

    require(
        result.brokerage_charges > 0.0,
        "Shared brokerage engine not applied.");

    require(
        environment.state().
            cumulative_brokerage_charges > 0.0,
        "Cumulative brokerage not updated.");
}


// ============================================================
// SLIPPAGE INTEGRATION
// ============================================================

void testSlippageIntegration()
{
    std::cout
        << "[TEST] Slippage integration..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);

    const auto& execution =
        result.executions.at(0);

    require(
        execution.execution_price >
            execution.market_price,
        "BUY slippage direction incorrect.");

    require(
        execution.slippage_cost > 0.0,
        "Slippage cost not recorded.");
}


// ============================================================
// NET EQUITY
// ============================================================

void testNetEquityAccounting()
{
    std::cout
        << "[TEST] Net equity accounting..."
        << std::endl;

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
        approximatelyEqual(
            entry.net_pnl,
            entry.equity_after -
                entry.equity_before),
        "Step net P&L mismatch.");

    require(
        approximatelyEqual(
            environment.state().net_pnl,
            environment.state().equity -
                environment.state().
                    initial_equity),
        "Account net P&L mismatch.");
}


// ============================================================
// PORTFOLIO STATE
// ============================================================

void testPortfolioState()
{
    std::cout
        << "[TEST] Portfolio-state generation..."
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

    static_cast<void>(
        environment.step(
            DevAction::HOLD,
            1120,
            101.0));

    const auto portfolio =
        environment.portfolioState();

    require(
        portfolio.position ==
            DEV_POSITION_LONG,
        "Portfolio position mismatch.");

    require(
        portfolio.holding_period ==
            1.0,
        "Holding period mismatch.");

    require(
        std::isfinite(
            portfolio.unrealized_return),
        "Non-finite unrealized return.");

    require(
        std::isfinite(
            portfolio.realized_return),
        "Non-finite realized return.");

    require(
        std::isfinite(
            portfolio.equity_return),
        "Non-finite equity return.");

    require(
        std::isfinite(
            portfolio.drawdown),
        "Non-finite drawdown.");
}


// ============================================================
// STRICT TIME
// ============================================================

void testStrictDecisionTime()
{
    std::cout
        << "[TEST] Strict decision-time progression..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    bool threw = false;

    try
    {
        static_cast<void>(
            environment.step(
                DevAction::HOLD,
                1000,
                101.0));
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    require(
        threw,
        "Duplicate decision time accepted.");
}


// ============================================================
// INVALID PRICE
// ============================================================

void testInvalidPrice()
{
    std::cout
        << "[TEST] Invalid price rejection..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    bool threw = false;

    try
    {
        static_cast<void>(
            environment.step(
                DevAction::HOLD,
                1060,
                std::numeric_limits<double>::
                    quiet_NaN()));
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    require(
        threw,
        "NaN price accepted.");
}


// ============================================================
// REWARD INTEGRATION
// ============================================================

void testRewardIntegration()
{
    std::cout
        << "[TEST] RewardEngine integration..."
        << std::endl;

    TradingEnvironment environment;

    environment.reset(
        "RELIANCE",
        1000,
        100.0);

    const auto result =
        environment.step(
            DevAction::LONG,
            1060,
            100.0);

    require(
        std::isfinite(
            result.reward.total),
        "Reward non-finite.");

    require(
        approximatelyEqual(
            result.reward.pnl_component,
            result.net_equity_return),
        "Reward did not receive net equity return.");
}


// ============================================================
// DETERMINISM
// ============================================================

void testDeterminism()
{
    std::cout
        << "[TEST] Deterministic environment..."
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

    const auto a =
        first.step(
            DevAction::LONG,
            1060,
            100.0);

    const auto b =
        second.step(
            DevAction::LONG,
            1060,
            100.0);

    require(
        approximatelyEqual(
            a.equity_after,
            b.equity_after),
        "Environment equity not deterministic.");

    require(
        approximatelyEqual(
            a.reward.total,
            b.reward.total),
        "Environment reward not deterministic.");
}

} // namespace


int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.7 — TRADING ENVIRONMENT VALIDATION\n"
            << "============================================================\n"
            << "Agent                         : Dev\n"
            << "Position Space                : {-1, 0, +1}\n"
            << "Shared Brokerage Engine       : ENABLED\n"
            << "Statutory Charges             : ENABLED\n"
            << "Execution-Price Slippage      : ENABLED\n"
            << "Gross P&L                     : ENABLED\n"
            << "Realized / Unrealized P&L     : ENABLED\n"
            << "Net Equity Accounting         : ENABLED\n"
            << "Direct Reversal               : TWO EXECUTIONS\n"
            << "Portfolio State               : ENABLED\n"
            << "RewardEngine                  : CONNECTED\n"
            << "Same-Step Future Profit       : PROHIBITED\n"
            << "============================================================\n\n";

        testReset();
        testFlatHold();

        testLongEntry();
        testShortEntry();

        testNoSameStepFutureProfit();

        testLongGrossPnl();
        testShortGrossPnl();

        testRealizedUnrealized();

        testLongToShortReversal();
        testShortToLongReversal();

        testBrokerageIntegration();
        testSlippageIntegration();

        testNetEquityAccounting();
        testPortfolioState();

        testStrictDecisionTime();
        testInvalidPrice();

        testRewardIntegration();
        testDeterminism();


        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.7 VALIDATION SUMMARY\n"
            << "============================================================\n"
            << "1.7.1 Position / Execution Model       : PASSED\n"
            << "1.7.2 Gross P&L Accounting             : PASSED\n"
            << "1.7.3 Shared Brokerage Integration     : PASSED\n"
            << "1.7.4 Slippage                         : PASSED\n"
            << "1.7.5 Realized / Unrealized P&L        : PASSED\n"
            << "1.7.6 Net Equity Accounting            : PASSED\n"
            << "1.7.7 Long <-> Short Reversal          : PASSED\n"
            << "1.7.8 Portfolio State Generation       : PASSED\n"
            << "1.7.9 Environment Validation           : PASSED\n"
            << "RewardEngine Integration               : PASSED\n"
            << "Decision-Time Integrity                : PASSED\n"
            << "Same-Step Future Profit                : NONE\n"
            << "Deterministic Execution                : PASSED\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.7 TRADING ENVIRONMENT VALIDATION PASSED\n"
            << "============================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.7 TRADING ENVIRONMENT VALIDATION FAILED\n"
            << "============================================================\n"
            << "Reason: "
            << exception.what()
            << "\n"
            << "============================================================\n";

        return 1;
    }
}