#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include "devai/strategy/dev_ai_strategy/ObservationNormalizer.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using devai::strategy::DEV_AVAILABILITY_COUNT;
using devai::strategy::DEV_MARKET_FEATURE_COUNT;
using devai::strategy::DEV_OBSERVATION_SIZE;
using devai::strategy::DEV_PORTFOLIO_FEATURE_COUNT;

using devai::strategy::DevAIObservation;
using devai::strategy::ObservationNormalizer;


// ============================================================
// OBSERVATION INDEXES
// ============================================================

constexpr std::size_t MARKET_BEGIN = 0;

constexpr std::size_t AVAILABILITY_BEGIN =
    DEV_MARKET_FEATURE_COUNT;

constexpr std::size_t PORTFOLIO_BEGIN =
    DEV_MARKET_FEATURE_COUNT +
    DEV_AVAILABILITY_COUNT;

constexpr std::size_t POSITION_INDEX =
    PORTFOLIO_BEGIN + 0;

constexpr std::size_t UNREALIZED_RETURN_INDEX =
    PORTFOLIO_BEGIN + 1;

constexpr std::size_t REALIZED_RETURN_INDEX =
    PORTFOLIO_BEGIN + 2;

constexpr std::size_t EQUITY_RETURN_INDEX =
    PORTFOLIO_BEGIN + 3;

constexpr std::size_t DRAWDOWN_INDEX =
    PORTFOLIO_BEGIN + 4;

constexpr std::size_t HOLDING_PERIOD_INDEX =
    PORTFOLIO_BEGIN + 5;


// ============================================================
// COMPILE-TIME CONTRACT
// ============================================================

static_assert(
    DEV_MARKET_FEATURE_COUNT == 39,
    "DevAI market feature count must remain 39.");

static_assert(
    DEV_AVAILABILITY_COUNT == 39,
    "DevAI availability count must remain 39.");

static_assert(
    DEV_PORTFOLIO_FEATURE_COUNT == 6,
    "DevAI portfolio feature count must remain 6.");

static_assert(
    DEV_OBSERVATION_SIZE == 84,
    "DevAI observation size must remain 84.");

static_assert(
    AVAILABILITY_BEGIN == 39,
    "Availability section must begin at index 39.");

static_assert(
    PORTFOLIO_BEGIN == 78,
    "Portfolio section must begin at index 78.");

static_assert(
    HOLDING_PERIOD_INDEX == 83,
    "Final observation index must be 83.");


// ============================================================
// TEST UTILITIES
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
    double tolerance = 1e-12)
{
    return std::abs(lhs - rhs) <= tolerance;
}


void requireFinite(
    double value,
    const std::string& message)
{
    require(
        std::isfinite(value),
        message + " is non-finite.");
}


void requireRange(
    double value,
    double minimum,
    double maximum,
    const std::string& message)
{
    requireFinite(
        value,
        message);

    require(
        value >= minimum &&
        value <= maximum,
        message + " is outside expected range.");
}


// ============================================================
// BASE OBSERVATION
// ============================================================

DevAIObservation makeObservation()
{
    DevAIObservation observation;

    observation.agent_name =
        "Dev";

    observation.symbol =
        "RELIANCE";

    observation.benchmark_symbol =
        "NIFTY%2050";

    observation.decision_time =
        1;

    observation.market.fill(
        0.0);

    observation.availability.fill(
        1.0);

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

    observation.numerical_integrity =
        true;

    observation.ready =
        true;

    return observation;
}


// ============================================================
// TEST 1
// OBSERVATION CONTRACT
// ============================================================

void testObservationContract()
{
    std::cout
        << "[TEST] Observation contract..."
        << std::endl;

    require(
        DEV_MARKET_FEATURE_COUNT == 39,
        "Market feature count mismatch.");

    require(
        DEV_AVAILABILITY_COUNT == 39,
        "Availability count mismatch.");

    require(
        DEV_PORTFOLIO_FEATURE_COUNT == 6,
        "Portfolio feature count mismatch.");

    require(
        DEV_OBSERVATION_SIZE == 84,
        "Observation size mismatch.");

    require(
        MARKET_BEGIN == 0,
        "Market section must begin at index 0.");

    require(
        AVAILABILITY_BEGIN == 39,
        "Availability section must begin at index 39.");

    require(
        PORTFOLIO_BEGIN == 78,
        "Portfolio section must begin at index 78.");

    require(
        HOLDING_PERIOD_INDEX == 83,
        "Holding-period index must be 83.");
}


// ============================================================
// TEST 2
// ZERO OBSERVATION
// ============================================================

void testZeroObservation()
{
    std::cout
        << "[TEST] Zero observation..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    observation.availability.fill(
        0.0);

    const auto normalized =
        normalizer.normalize(
            observation);

    for (const double value : normalized)
    {
        require(
            approximatelyEqual(
                value,
                0.0),
            "Zero observation did not normalize to zero.");
    }
}


// ============================================================
// TEST 3
// MARKET VALUES PRESERVED
// ============================================================

void testMarketValuesPreserved()
{
    std::cout
        << "[TEST] Market centered-percentile preservation..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        const double fraction =
            static_cast<double>(i) /
            static_cast<double>(
                DEV_MARKET_FEATURE_COUNT - 1);

        observation.market[i] =
            -1.0 +
            (2.0 * fraction);
    }

    const auto normalized =
        normalizer.normalize(
            observation);

    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        require(
            approximatelyEqual(
                normalized[i],
                observation.market[i]),
            "Valid market centered percentile was changed.");
    }
}


// ============================================================
// TEST 4
// MARKET BOUNDARIES
// ============================================================

void testMarketBoundaryValues()
{
    std::cout
        << "[TEST] Market boundaries..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    observation.market[0] = -1.0;
    observation.market[1] = 0.0;
    observation.market[2] = 1.0;

    const auto normalized =
        normalizer.normalize(
            observation);

    require(
        approximatelyEqual(
            normalized[0],
            -1.0),
        "Market -1 boundary failed.");

    require(
        approximatelyEqual(
            normalized[1],
            0.0),
        "Market zero boundary failed.");

    require(
        approximatelyEqual(
            normalized[2],
            1.0),
        "Market +1 boundary failed.");
}


// ============================================================
// TEST 5
// MARKET CLAMPING
// ============================================================

void testMarketClamping()
{
    std::cout
        << "[TEST] Market clamping..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    observation.market[0] =
        -5.0;

    observation.market[1] =
        5.0;

    const auto normalized =
        normalizer.normalize(
            observation);

    require(
        approximatelyEqual(
            normalized[0],
            -1.0),
        "Market lower clamp failed.");

    require(
        approximatelyEqual(
            normalized[1],
            1.0),
        "Market upper clamp failed.");
}


// ============================================================
// TEST 6
// AVAILABILITY MASKS
// ============================================================

void testAvailabilityMasks()
{
    std::cout
        << "[TEST] Availability masks..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        observation.availability[i] =
            (i % 2 == 0)
                ? 0.0
                : 1.0;
    }

    const auto normalized =
        normalizer.normalize(
            observation);

    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        const std::size_t index =
            AVAILABILITY_BEGIN + i;

        require(
            approximatelyEqual(
                normalized[index],
                observation.availability[i]),
            "Availability mask mapping failed.");

        require(
            normalized[index] == 0.0 ||
            normalized[index] == 1.0,
            "Availability mask is not binary.");
    }
}


// ============================================================
// TEST 7
// POSITION NORMALIZATION
// ============================================================

void testPositionNormalization()
{
    std::cout
        << "[TEST] Position normalization..."
        << std::endl;

    ObservationNormalizer normalizer;

    // Short
    {
        auto observation =
            makeObservation();

        observation.portfolio.position =
            -1.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[POSITION_INDEX],
                -1.0),
            "Short position normalization failed.");
    }

    // Flat
    {
        auto observation =
            makeObservation();

        observation.portfolio.position =
            0.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[POSITION_INDEX],
                0.0),
            "Flat position normalization failed.");
    }

    // Long
    {
        auto observation =
            makeObservation();

        observation.portfolio.position =
            1.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[POSITION_INDEX],
                1.0),
            "Long position normalization failed.");
    }
}


// ============================================================
// TEST 8
// POSITION CLAMPING
// ============================================================

void testPositionClamping()
{
    std::cout
        << "[TEST] Position clamping..."
        << std::endl;

    ObservationNormalizer normalizer;

    {
        auto observation =
            makeObservation();

        observation.portfolio.position =
            -10.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[POSITION_INDEX],
                -1.0),
            "Position lower clamp failed.");
    }

    {
        auto observation =
            makeObservation();

        observation.portfolio.position =
            10.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[POSITION_INDEX],
                1.0),
            "Position upper clamp failed.");
    }
}


// ============================================================
// TEST 9
// RETURN NORMALIZATION
// ============================================================

void testReturnNormalization()
{
    std::cout
        << "[TEST] Portfolio return normalization..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    observation.portfolio.unrealized_return =
        0.10;

    observation.portfolio.realized_return =
        -0.10;

    observation.portfolio.equity_return =
        0.05;

    const auto normalized =
        normalizer.normalize(
            observation);

    require(
        approximatelyEqual(
            normalized[UNREALIZED_RETURN_INDEX],
            std::tanh(1.0)),
        "Unrealized-return normalization failed.");

    require(
        approximatelyEqual(
            normalized[REALIZED_RETURN_INDEX],
            std::tanh(-1.0)),
        "Realized-return normalization failed.");

    require(
        approximatelyEqual(
            normalized[EQUITY_RETURN_INDEX],
            std::tanh(0.5)),
        "Equity-return normalization failed.");
}


// ============================================================
// TEST 10
// RETURN SIGN SYMMETRY
// ============================================================

void testReturnSignSymmetry()
{
    std::cout
        << "[TEST] Return sign symmetry..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto positive =
        makeObservation();

    auto negative =
        makeObservation();

    positive.portfolio.unrealized_return =
        0.025;

    negative.portfolio.unrealized_return =
        -0.025;

    const auto positive_normalized =
        normalizer.normalize(
            positive);

    const auto negative_normalized =
        normalizer.normalize(
            negative);

    require(
        approximatelyEqual(
            positive_normalized[
                UNREALIZED_RETURN_INDEX],
            -negative_normalized[
                UNREALIZED_RETURN_INDEX]),
        "Return normalization is not sign symmetric.");
}


// ============================================================
// TEST 11
// LARGE RETURN BOUNDING
// ============================================================

void testLargeReturnBounding()
{
    std::cout
        << "[TEST] Large-return bounding..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    observation.portfolio.unrealized_return =
        1000.0;

    observation.portfolio.realized_return =
        -1000.0;

    observation.portfolio.equity_return =
        1000.0;

    const auto normalized =
        normalizer.normalize(
            observation);

    requireRange(
        normalized[UNREALIZED_RETURN_INDEX],
        -1.0,
        1.0,
        "Unrealized return");

    requireRange(
        normalized[REALIZED_RETURN_INDEX],
        -1.0,
        1.0,
        "Realized return");

    requireRange(
        normalized[EQUITY_RETURN_INDEX],
        -1.0,
        1.0,
        "Equity return");

    require(
        normalized[UNREALIZED_RETURN_INDEX] >
            0.999,
        "Large positive unrealized return was not bounded near +1.");

    require(
        normalized[REALIZED_RETURN_INDEX] <
            -0.999,
        "Large negative realized return was not bounded near -1.");
}


// ============================================================
// TEST 12
// DRAWDOWN NORMALIZATION
// ============================================================

void testDrawdownNormalization()
{
    std::cout
        << "[TEST] Drawdown normalization..."
        << std::endl;

    ObservationNormalizer normalizer;

    // Zero drawdown
    {
        auto observation =
            makeObservation();

        observation.portfolio.drawdown =
            0.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[DRAWDOWN_INDEX],
                0.0),
            "Zero drawdown normalization failed.");
    }

    // 25% drawdown
    {
        auto observation =
            makeObservation();

        observation.portfolio.drawdown =
            0.25;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[DRAWDOWN_INDEX],
                0.25),
            "Drawdown preservation failed.");
    }

    // Above maximum
    {
        auto observation =
            makeObservation();

        observation.portfolio.drawdown =
            2.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[DRAWDOWN_INDEX],
                1.0),
            "Drawdown upper clamp failed.");
    }

    // Below minimum
    {
        auto observation =
            makeObservation();

        observation.portfolio.drawdown =
            -1.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[DRAWDOWN_INDEX],
                0.0),
            "Drawdown lower clamp failed.");
    }
}


// ============================================================
// TEST 13
// HOLDING PERIOD NORMALIZATION
// ============================================================

void testHoldingPeriodNormalization()
{
    std::cout
        << "[TEST] Holding-period normalization..."
        << std::endl;

    constexpr double maximum_holding_bars =
        390.0;

    ObservationNormalizer normalizer(
        maximum_holding_bars);

    // Zero
    {
        auto observation =
            makeObservation();

        observation.portfolio.holding_period =
            0.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[HOLDING_PERIOD_INDEX],
                0.0),
            "Zero holding-period normalization failed.");
    }

    // Half maximum
    {
        auto observation =
            makeObservation();

        observation.portfolio.holding_period =
            195.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[HOLDING_PERIOD_INDEX],
                0.5),
            "Half holding-period normalization failed.");
    }

    // Maximum
    {
        auto observation =
            makeObservation();

        observation.portfolio.holding_period =
            390.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[HOLDING_PERIOD_INDEX],
                1.0),
            "Maximum holding-period normalization failed.");
    }

    // Above maximum
    {
        auto observation =
            makeObservation();

        observation.portfolio.holding_period =
            1000.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[HOLDING_PERIOD_INDEX],
                1.0),
            "Holding-period upper clamp failed.");
    }

    // Negative
    {
        auto observation =
            makeObservation();

        observation.portfolio.holding_period =
            -100.0;

        const auto normalized =
            normalizer.normalize(
                observation);

        require(
            approximatelyEqual(
                normalized[HOLDING_PERIOD_INDEX],
                0.0),
            "Holding-period lower clamp failed.");
    }
}


// ============================================================
// TEST 14
// MAXIMUM HOLDING BARS CONFIGURATION
// ============================================================

void testMaximumHoldingBarsConfiguration()
{
    std::cout
        << "[TEST] Maximum-holding-bars configuration..."
        << std::endl;

    ObservationNormalizer normalizer(
        100.0);

    require(
        approximatelyEqual(
            normalizer.maximumHoldingBars(),
            100.0),
        "maximumHoldingBars() returned wrong value.");

    auto observation =
        makeObservation();

    observation.portfolio.holding_period =
        25.0;

    const auto normalized =
        normalizer.normalize(
            observation);

    require(
        approximatelyEqual(
            normalized[HOLDING_PERIOD_INDEX],
            0.25),
        "Custom maximum holding-period normalization failed.");
}


// ============================================================
// TEST 15
// COMPLETE OUTPUT RANGE
// ============================================================

void testCompleteOutputRange()
{
    std::cout
        << "[TEST] Complete normalized-vector range..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        observation.market[i] =
            (i % 2 == 0)
                ? -1.0
                : 1.0;
    }

    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        observation.availability[i] =
            (i % 2 == 0)
                ? 0.0
                : 1.0;
    }

    observation.portfolio.position =
        1.0;

    observation.portfolio.unrealized_return =
        0.05;

    observation.portfolio.realized_return =
        -0.02;

    observation.portfolio.equity_return =
        0.03;

    observation.portfolio.drawdown =
        0.10;

    observation.portfolio.holding_period =
        100.0;

    const auto normalized =
        normalizer.normalize(
            observation);

    // Market
    for (std::size_t i = 0;
         i < DEV_MARKET_FEATURE_COUNT;
         ++i)
    {
        requireRange(
            normalized[i],
            -1.0,
            1.0,
            "Normalized market value");
    }

    // Availability
    for (std::size_t i = 0;
         i < DEV_AVAILABILITY_COUNT;
         ++i)
    {
        const std::size_t index =
            AVAILABILITY_BEGIN + i;

        requireFinite(
            normalized[index],
            "Normalized availability value");

        require(
            normalized[index] == 0.0 ||
            normalized[index] == 1.0,
            "Normalized availability value is not binary.");
    }

    // Portfolio
    requireRange(
        normalized[POSITION_INDEX],
        -1.0,
        1.0,
        "Normalized position");

    requireRange(
        normalized[UNREALIZED_RETURN_INDEX],
        -1.0,
        1.0,
        "Normalized unrealized return");

    requireRange(
        normalized[REALIZED_RETURN_INDEX],
        -1.0,
        1.0,
        "Normalized realized return");

    requireRange(
        normalized[EQUITY_RETURN_INDEX],
        -1.0,
        1.0,
        "Normalized equity return");

    requireRange(
        normalized[DRAWDOWN_INDEX],
        0.0,
        1.0,
        "Normalized drawdown");

    requireRange(
        normalized[HOLDING_PERIOD_INDEX],
        0.0,
        1.0,
        "Normalized holding period");
}


// ============================================================
// TEST 16
// DETERMINISM
// ============================================================

void testDeterminism()
{
    std::cout
        << "[TEST] Deterministic normalization..."
        << std::endl;

    ObservationNormalizer normalizer;

    auto observation =
        makeObservation();

    observation.market[0] =
        0.777777777;

    observation.market[10] =
        -0.333333333;

    observation.portfolio.position =
        -1.0;

    observation.portfolio.unrealized_return =
        0.0125;

    observation.portfolio.realized_return =
        -0.0075;

    observation.portfolio.equity_return =
        0.004;

    observation.portfolio.drawdown =
        0.08;

    observation.portfolio.holding_period =
        125.0;

    const auto first =
        normalizer.normalize(
            observation);

    const auto second =
        normalizer.normalize(
            observation);

    for (std::size_t i = 0;
         i < DEV_OBSERVATION_SIZE;
         ++i)
    {
        require(
            approximatelyEqual(
                first[i],
                second[i]),
            "Observation normalization is not deterministic.");
    }
}


// ============================================================
// END ANONYMOUS NAMESPACE
// ============================================================

} // namespace


// ============================================================
// MAIN
// ============================================================

int main()
{
    try
    {
        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.4 — OBSERVATION NORMALIZATION VALIDATION\n"
            << "============================================================\n"
            << "Agent                    : Dev\n"
            << "Market features          : "
            << DEV_MARKET_FEATURE_COUNT
            << "\n"
            << "Availability masks       : "
            << DEV_AVAILABILITY_COUNT
            << "\n"
            << "Portfolio features       : "
            << DEV_PORTFOLIO_FEATURE_COUNT
            << "\n"
            << "Total observation size   : "
            << DEV_OBSERVATION_SIZE
            << "\n"
            << "============================================================\n\n";

        testObservationContract();
        testZeroObservation();
        testMarketValuesPreserved();
        testMarketBoundaryValues();
        testMarketClamping();
        testAvailabilityMasks();
        testPositionNormalization();
        testPositionClamping();
        testReturnNormalization();
        testReturnSignSymmetry();
        testLargeReturnBounding();
        testDrawdownNormalization();
        testHoldingPeriodNormalization();
        testMaximumHoldingBarsConfiguration();
        testCompleteOutputRange();
        testDeterminism();

        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.4 VALIDATION SUMMARY\n"
            << "============================================================\n"
            << "Observation Contract             : PASSED\n"
            << "39 Market Values                 : PASSED\n"
            << "39 Availability Masks            : PASSED\n"
            << "Position Normalization           : PASSED\n"
            << "Return Normalization             : PASSED\n"
            << "Drawdown Normalization           : PASSED\n"
            << "Holding-Period Normalization     : PASSED\n"
            << "Output Range Protection          : PASSED\n"
            << "Deterministic Normalization      : PASSED\n"
            << "Observation Size (84)            : PASSED\n"
            << "Fitted/Future Statistics Needed : NO\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.4 OBSERVATION NORMALIZATION PASSED\n"
            << "============================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.4 OBSERVATION NORMALIZATION FAILED\n"
            << "============================================================\n"
            << "Reason: "
            << exception.what()
            << "\n"
            << "============================================================\n";

        return 1;
    }
}