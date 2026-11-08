#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using namespace devai::strategy::brokerage;


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
    double tolerance = 1e-10)
{
    return std::abs(lhs - rhs) <= tolerance;
}


// ============================================================
// ZERO TURNOVER
// ============================================================

void testZeroTurnover()
{
    std::cout
        << "[TEST] Zero turnover..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto result =
        engine.calculate(
            ExecutionSide::BUY,
            0.0);

    require(
        approximatelyEqual(
            result.total,
            0.0),
        "Zero turnover produced charges.");
}


// ============================================================
// BUY-SIDE RULES
// ============================================================

void testBuySide()
{
    std::cout
        << "[TEST] BUY-side charges..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto result =
        engine.calculate(
            ExecutionSide::BUY,
            100000.0);

    require(
        result.brokerage > 0.0,
        "BUY brokerage missing.");

    require(
        result.brokerage <= 20.0,
        "Brokerage cap exceeded.");

    require(
        approximatelyEqual(
            result.stt,
            0.0),
        "BUY intraday STT must be zero.");

    require(
        result.exchange_transaction_charge > 0.0,
        "Exchange charge missing.");

    require(
        result.sebi_charge > 0.0,
        "SEBI charge missing.");

    require(
        result.stamp_duty > 0.0,
        "BUY stamp duty missing.");

    require(
        result.gst > 0.0,
        "GST missing.");

    require(
        result.total > 0.0,
        "Total BUY charges invalid.");
}


// ============================================================
// SELL-SIDE RULES
// ============================================================

void testSellSide()
{
    std::cout
        << "[TEST] SELL-side charges..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto result =
        engine.calculate(
            ExecutionSide::SELL,
            100000.0);

    require(
        result.stt > 0.0,
        "SELL STT missing.");

    require(
        approximatelyEqual(
            result.stamp_duty,
            0.0),
        "SELL stamp duty must be zero.");

    require(
        result.total > 0.0,
        "Total SELL charges invalid.");
}


// ============================================================
// BROKERAGE CAP
// ============================================================

void testBrokerageCap()
{
    std::cout
        << "[TEST] Brokerage cap..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto result =
        engine.calculate(
            ExecutionSide::BUY,
            1000000.0);

    require(
        approximatelyEqual(
            result.brokerage,
            20.0),
        "Brokerage cap mismatch.");
}


// ============================================================
// SMALL ORDER
// ============================================================

void testPercentageBrokerage()
{
    std::cout
        << "[TEST] Percentage brokerage..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto result =
        engine.calculate(
            ExecutionSide::BUY,
            10000.0);

    require(
        approximatelyEqual(
            result.brokerage,
            3.0),
        "Percentage brokerage mismatch.");
}


// ============================================================
// TOTAL INTEGRITY
// ============================================================

void testTotalIntegrity()
{
    std::cout
        << "[TEST] Charge component sum..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto result =
        engine.calculate(
            ExecutionSide::SELL,
            123456.0);

    const double expected =
        result.brokerage +
        result.stt +
        result.exchange_transaction_charge +
        result.sebi_charge +
        result.stamp_duty +
        result.gst;

    require(
        approximatelyEqual(
            result.total,
            expected),
        "Charge total mismatch.");
}


// ============================================================
// INVALID INPUT
// ============================================================

void testInvalidInput()
{
    std::cout
        << "[TEST] Invalid input rejection..."
        << std::endl;

    BrokerageChargesEngine engine;

    bool negative_threw = false;

    try
    {
        static_cast<void>(
            engine.calculate(
                ExecutionSide::BUY,
                -1.0));
    }
    catch (const std::invalid_argument&)
    {
        negative_threw = true;
    }

    require(
        negative_threw,
        "Negative turnover accepted.");


    bool nan_threw = false;

    try
    {
        static_cast<void>(
            engine.calculate(
                ExecutionSide::BUY,
                std::numeric_limits<double>::
                    quiet_NaN()));
    }
    catch (const std::invalid_argument&)
    {
        nan_threw = true;
    }

    require(
        nan_threw,
        "NaN turnover accepted.");
}


// ============================================================
// DETERMINISM
// ============================================================

void testDeterminism()
{
    std::cout
        << "[TEST] Deterministic charges..."
        << std::endl;

    BrokerageChargesEngine engine;

    const auto first =
        engine.calculate(
            ExecutionSide::SELL,
            98765.0);

    const auto second =
        engine.calculate(
            ExecutionSide::SELL,
            98765.0);

    require(
        approximatelyEqual(
            first.total,
            second.total),
        "Brokerage calculation is not deterministic.");
}

} // namespace


int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "CETE SHARED BROKERAGE ENGINE VALIDATION\n"
            << "============================================================\n"
            << "Reusable By                 : ALL STRATEGIES\n"
            << "DevAI Integration           : YES\n"
            << "PCA Integration             : AVAILABLE\n"
            << "Backtesting                 : SUPPORTED\n"
            << "Forward Testing             : SUPPORTED\n"
            << "============================================================\n\n";

        testZeroTurnover();
        testBuySide();
        testSellSide();
        testBrokerageCap();
        testPercentageBrokerage();
        testTotalIntegrity();
        testInvalidInput();
        testDeterminism();

        std::cout
            << "\n"
            << "============================================================\n"
            << "SHARED BROKERAGE VALIDATION SUMMARY\n"
            << "============================================================\n"
            << "Zero Turnover                    : PASSED\n"
            << "BUY-Side Charges                 : PASSED\n"
            << "SELL-Side Charges                : PASSED\n"
            << "Brokerage Percentage             : PASSED\n"
            << "Brokerage Cap                    : PASSED\n"
            << "Component-Sum Integrity          : PASSED\n"
            << "Invalid-Input Rejection          : PASSED\n"
            << "Deterministic Calculation        : PASSED\n"
            << "============================================================\n"
            << "CETE SHARED BROKERAGE ENGINE VALIDATION PASSED\n"
            << "============================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nFAILED: "
            << exception.what()
            << '\n';

        return 1;
    }
}