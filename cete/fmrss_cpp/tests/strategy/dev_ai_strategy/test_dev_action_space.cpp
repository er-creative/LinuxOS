#include "devai/strategy/dev_ai_strategy/DevAction.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using devai::strategy::DEV_ACTION_COUNT;

using devai::strategy::DEV_POSITION_FLAT;
using devai::strategy::DEV_POSITION_LONG;
using devai::strategy::DEV_POSITION_SHORT;

using devai::strategy::DevAction;

using devai::strategy::actionName;
using devai::strategy::actionTurnover;
using devai::strategy::changesPosition;
using devai::strategy::isPositionReversal;
using devai::strategy::isValidAction;
using devai::strategy::isValidDevPosition;
using devai::strategy::targetPosition;


// ============================================================
// COMPILE-TIME CONTRACT
// ============================================================

static_assert(
    DEV_ACTION_COUNT == 4,
    "Dev action space must contain exactly four actions.");

static_assert(
    static_cast<std::size_t>(
        DevAction::HOLD) == 0,
    "HOLD action index must be 0.");

static_assert(
    static_cast<std::size_t>(
        DevAction::LONG) == 1,
    "LONG action index must be 1.");

static_assert(
    static_cast<std::size_t>(
        DevAction::SHORT) == 2,
    "SHORT action index must be 2.");

static_assert(
    static_cast<std::size_t>(
        DevAction::EXIT) == 3,
    "EXIT action index must be 3.");

static_assert(
    DEV_POSITION_SHORT == -1.0);

static_assert(
    DEV_POSITION_FLAT == 0.0);

static_assert(
    DEV_POSITION_LONG == 1.0);


// ============================================================
// TEST UTILITIES
// ============================================================

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
    double tolerance = 1e-12)
{
    return
        std::abs(lhs - rhs) <= tolerance;
}


void requirePosition(
    double actual,
    double expected,
    const std::string& message)
{
    require(
        approximatelyEqual(
            actual,
            expected),
        message);
}


// ============================================================
// TEST 1
// ACTION COUNT
// ============================================================

void testActionCount()
{
    std::cout
        << "[TEST] Action count..."
        << std::endl;

    require(
        DEV_ACTION_COUNT == 4,
        "Dev action count must equal 4.");
}


// ============================================================
// TEST 2
// ACTION INDEXES
// ============================================================

void testActionIndexes()
{
    std::cout
        << "[TEST] Action indexes..."
        << std::endl;

    require(
        static_cast<std::size_t>(
            DevAction::HOLD) == 0,
        "HOLD index mismatch.");

    require(
        static_cast<std::size_t>(
            DevAction::LONG) == 1,
        "LONG index mismatch.");

    require(
        static_cast<std::size_t>(
            DevAction::SHORT) == 2,
        "SHORT index mismatch.");

    require(
        static_cast<std::size_t>(
            DevAction::EXIT) == 3,
        "EXIT index mismatch.");
}


// ============================================================
// TEST 3
// ACTION NAMES
// ============================================================

void testActionNames()
{
    std::cout
        << "[TEST] Action names..."
        << std::endl;

    require(
        actionName(
            DevAction::HOLD) == "HOLD",
        "HOLD action name mismatch.");

    require(
        actionName(
            DevAction::LONG) == "LONG",
        "LONG action name mismatch.");

    require(
        actionName(
            DevAction::SHORT) == "SHORT",
        "SHORT action name mismatch.");

    require(
        actionName(
            DevAction::EXIT) == "EXIT",
        "EXIT action name mismatch.");
}


// ============================================================
// TEST 4
// ACTION VALIDATION
// ============================================================

void testActionValidation()
{
    std::cout
        << "[TEST] Action validation..."
        << std::endl;

    require(
        isValidAction(
            DevAction::HOLD),
        "HOLD must be valid.");

    require(
        isValidAction(
            DevAction::LONG),
        "LONG must be valid.");

    require(
        isValidAction(
            DevAction::SHORT),
        "SHORT must be valid.");

    require(
        isValidAction(
            DevAction::EXIT),
        "EXIT must be valid.");

    const auto invalid =
        static_cast<DevAction>(999);

    require(
        !isValidAction(invalid),
        "Invalid action was accepted.");

    require(
        actionName(invalid) == "INVALID",
        "Invalid action name must be INVALID.");
}


// ============================================================
// TEST 5
// POSITION VALIDATION
// ============================================================

void testPositionValidation()
{
    std::cout
        << "[TEST] Position validation..."
        << std::endl;

    require(
        isValidDevPosition(
            DEV_POSITION_SHORT),
        "Short position must be valid.");

    require(
        isValidDevPosition(
            DEV_POSITION_FLAT),
        "Flat position must be valid.");

    require(
        isValidDevPosition(
            DEV_POSITION_LONG),
        "Long position must be valid.");

    require(
        !isValidDevPosition(
            -0.5),
        "Fractional short position must not be valid.");

    require(
        !isValidDevPosition(
            0.5),
        "Fractional long position must not be valid.");

    require(
        !isValidDevPosition(
            2.0),
        "Out-of-range position must not be valid.");
}


// ============================================================
// TEST 6
// FLAT TRANSITIONS
// ============================================================

void testFlatTransitions()
{
    std::cout
        << "[TEST] Flat-state transitions..."
        << std::endl;

    requirePosition(
        targetPosition(
            DevAction::HOLD,
            DEV_POSITION_FLAT),
        DEV_POSITION_FLAT,
        "FLAT + HOLD must remain FLAT.");

    requirePosition(
        targetPosition(
            DevAction::LONG,
            DEV_POSITION_FLAT),
        DEV_POSITION_LONG,
        "FLAT + LONG must become LONG.");

    requirePosition(
        targetPosition(
            DevAction::SHORT,
            DEV_POSITION_FLAT),
        DEV_POSITION_SHORT,
        "FLAT + SHORT must become SHORT.");

    requirePosition(
        targetPosition(
            DevAction::EXIT,
            DEV_POSITION_FLAT),
        DEV_POSITION_FLAT,
        "FLAT + EXIT must remain FLAT.");
}


// ============================================================
// TEST 7
// LONG TRANSITIONS
// ============================================================

void testLongTransitions()
{
    std::cout
        << "[TEST] Long-state transitions..."
        << std::endl;

    requirePosition(
        targetPosition(
            DevAction::HOLD,
            DEV_POSITION_LONG),
        DEV_POSITION_LONG,
        "LONG + HOLD must remain LONG.");

    requirePosition(
        targetPosition(
            DevAction::LONG,
            DEV_POSITION_LONG),
        DEV_POSITION_LONG,
        "LONG + LONG must remain LONG.");

    requirePosition(
        targetPosition(
            DevAction::SHORT,
            DEV_POSITION_LONG),
        DEV_POSITION_SHORT,
        "LONG + SHORT must reverse to SHORT.");

    requirePosition(
        targetPosition(
            DevAction::EXIT,
            DEV_POSITION_LONG),
        DEV_POSITION_FLAT,
        "LONG + EXIT must become FLAT.");
}


// ============================================================
// TEST 8
// SHORT TRANSITIONS
// ============================================================

void testShortTransitions()
{
    std::cout
        << "[TEST] Short-state transitions..."
        << std::endl;

    requirePosition(
        targetPosition(
            DevAction::HOLD,
            DEV_POSITION_SHORT),
        DEV_POSITION_SHORT,
        "SHORT + HOLD must remain SHORT.");

    requirePosition(
        targetPosition(
            DevAction::LONG,
            DEV_POSITION_SHORT),
        DEV_POSITION_LONG,
        "SHORT + LONG must reverse to LONG.");

    requirePosition(
        targetPosition(
            DevAction::SHORT,
            DEV_POSITION_SHORT),
        DEV_POSITION_SHORT,
        "SHORT + SHORT must remain SHORT.");

    requirePosition(
        targetPosition(
            DevAction::EXIT,
            DEV_POSITION_SHORT),
        DEV_POSITION_FLAT,
        "SHORT + EXIT must become FLAT.");
}


// ============================================================
// TEST 9
// HOLD SEMANTICS
// ============================================================

void testHoldSemantics()
{
    std::cout
        << "[TEST] HOLD semantics..."
        << std::endl;

    requirePosition(
        targetPosition(
            DevAction::HOLD,
            DEV_POSITION_SHORT),
        DEV_POSITION_SHORT,
        "HOLD changed SHORT position.");

    requirePosition(
        targetPosition(
            DevAction::HOLD,
            DEV_POSITION_FLAT),
        DEV_POSITION_FLAT,
        "HOLD changed FLAT position.");

    requirePosition(
        targetPosition(
            DevAction::HOLD,
            DEV_POSITION_LONG),
        DEV_POSITION_LONG,
        "HOLD changed LONG position.");
}


// ============================================================
// TEST 10
// EXIT SEMANTICS
// ============================================================

void testExitSemantics()
{
    std::cout
        << "[TEST] EXIT semantics..."
        << std::endl;

    requirePosition(
        targetPosition(
            DevAction::EXIT,
            DEV_POSITION_SHORT),
        DEV_POSITION_FLAT,
        "EXIT failed from SHORT.");

    requirePosition(
        targetPosition(
            DevAction::EXIT,
            DEV_POSITION_FLAT),
        DEV_POSITION_FLAT,
        "EXIT failed from FLAT.");

    requirePosition(
        targetPosition(
            DevAction::EXIT,
            DEV_POSITION_LONG),
        DEV_POSITION_FLAT,
        "EXIT failed from LONG.");
}


// ============================================================
// TEST 11
// REVERSALS
// ============================================================

void testReversals()
{
    std::cout
        << "[TEST] Position reversals..."
        << std::endl;

    require(
        isPositionReversal(
            DEV_POSITION_LONG,
            DEV_POSITION_SHORT),
        "LONG -> SHORT must be a reversal.");

    require(
        isPositionReversal(
            DEV_POSITION_SHORT,
            DEV_POSITION_LONG),
        "SHORT -> LONG must be a reversal.");

    require(
        !isPositionReversal(
            DEV_POSITION_LONG,
            DEV_POSITION_FLAT),
        "LONG -> FLAT must not be a reversal.");

    require(
        !isPositionReversal(
            DEV_POSITION_SHORT,
            DEV_POSITION_FLAT),
        "SHORT -> FLAT must not be a reversal.");

    require(
        !isPositionReversal(
            DEV_POSITION_FLAT,
            DEV_POSITION_LONG),
        "FLAT -> LONG must not be a reversal.");

    require(
        !isPositionReversal(
            DEV_POSITION_FLAT,
            DEV_POSITION_SHORT),
        "FLAT -> SHORT must not be a reversal.");
}


// ============================================================
// TEST 12
// POSITION-CHANGE DETECTION
// ============================================================

void testPositionChangeDetection()
{
    std::cout
        << "[TEST] Position-change detection..."
        << std::endl;

    require(
        !changesPosition(
            DEV_POSITION_FLAT,
            DEV_POSITION_FLAT),
        "FLAT -> FLAT must not be a change.");

    require(
        !changesPosition(
            DEV_POSITION_LONG,
            DEV_POSITION_LONG),
        "LONG -> LONG must not be a change.");

    require(
        !changesPosition(
            DEV_POSITION_SHORT,
            DEV_POSITION_SHORT),
        "SHORT -> SHORT must not be a change.");

    require(
        changesPosition(
            DEV_POSITION_FLAT,
            DEV_POSITION_LONG),
        "FLAT -> LONG must be a change.");

    require(
        changesPosition(
            DEV_POSITION_LONG,
            DEV_POSITION_FLAT),
        "LONG -> FLAT must be a change.");

    require(
        changesPosition(
            DEV_POSITION_LONG,
            DEV_POSITION_SHORT),
        "LONG -> SHORT must be a change.");
}


// ============================================================
// TEST 13
// TURNOVER
// ============================================================

void testTurnover()
{
    std::cout
        << "[TEST] Action turnover..."
        << std::endl;

    requirePosition(
        actionTurnover(
            DEV_POSITION_FLAT,
            DEV_POSITION_FLAT),
        0.0,
        "FLAT -> FLAT turnover must be 0.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_LONG,
            DEV_POSITION_LONG),
        0.0,
        "LONG -> LONG turnover must be 0.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_SHORT,
            DEV_POSITION_SHORT),
        0.0,
        "SHORT -> SHORT turnover must be 0.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_FLAT,
            DEV_POSITION_LONG),
        1.0,
        "FLAT -> LONG turnover must be 1.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_FLAT,
            DEV_POSITION_SHORT),
        1.0,
        "FLAT -> SHORT turnover must be 1.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_LONG,
            DEV_POSITION_FLAT),
        1.0,
        "LONG -> FLAT turnover must be 1.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_SHORT,
            DEV_POSITION_FLAT),
        1.0,
        "SHORT -> FLAT turnover must be 1.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_LONG,
            DEV_POSITION_SHORT),
        2.0,
        "LONG -> SHORT turnover must be 2.");

    requirePosition(
        actionTurnover(
            DEV_POSITION_SHORT,
            DEV_POSITION_LONG),
        2.0,
        "SHORT -> LONG turnover must be 2.");
}


// ============================================================
// TEST 14
// INVALID CURRENT POSITION
// ============================================================

void testInvalidCurrentPosition()
{
    std::cout
        << "[TEST] Invalid current-position rejection..."
        << std::endl;

    bool threw =
        false;

    try
    {
        static_cast<void>(
            targetPosition(
                DevAction::LONG,
                0.5));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }

    require(
        threw,
        "targetPosition must reject invalid current position.");
}


// ============================================================
// TEST 15
// INVALID ACTION
// ============================================================

void testInvalidAction()
{
    std::cout
        << "[TEST] Invalid action rejection..."
        << std::endl;

    const auto invalid =
        static_cast<DevAction>(999);

    bool threw =
        false;

    try
    {
        static_cast<void>(
            targetPosition(
                invalid,
                DEV_POSITION_FLAT));
    }
    catch (const std::invalid_argument&)
    {
        threw =
            true;
    }

    require(
        threw,
        "targetPosition must reject invalid action.");
}


// ============================================================
// TEST 16
// COMPLETE TRANSITION MATRIX
// ============================================================

void testCompleteTransitionMatrix()
{
    std::cout
        << "[TEST] Complete transition matrix..."
        << std::endl;

    constexpr double positions[] =
    {
        DEV_POSITION_FLAT,
        DEV_POSITION_LONG,
        DEV_POSITION_SHORT
    };

    constexpr DevAction actions[] =
    {
        DevAction::HOLD,
        DevAction::LONG,
        DevAction::SHORT,
        DevAction::EXIT
    };

    for (const double current_position : positions)
    {
        for (const DevAction action : actions)
        {
            const double target =
                targetPosition(
                    action,
                    current_position);

            require(
                isValidDevPosition(target),
                "Action generated an invalid target position.");
        }
    }
}


// ============================================================
// TEST 17
// DETERMINISM
// ============================================================

void testDeterminism()
{
    std::cout
        << "[TEST] Deterministic action mapping..."
        << std::endl;

    constexpr DevAction actions[] =
    {
        DevAction::HOLD,
        DevAction::LONG,
        DevAction::SHORT,
        DevAction::EXIT
    };

    constexpr double positions[] =
    {
        DEV_POSITION_SHORT,
        DEV_POSITION_FLAT,
        DEV_POSITION_LONG
    };

    for (const double current_position : positions)
    {
        for (const DevAction action : actions)
        {
            const double first =
                targetPosition(
                    action,
                    current_position);

            const double second =
                targetPosition(
                    action,
                    current_position);

            requirePosition(
                first,
                second,
                "Action mapping is not deterministic.");
        }
    }
}

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
            << "DEVAI PHASE 1.5 — ACTION-SPACE VALIDATION\n"
            << "============================================================\n"
            << "Agent                  : Dev\n"
            << "Action Count           : "
            << DEV_ACTION_COUNT
            << "\n"
            << "HOLD Index             : "
            << static_cast<std::size_t>(
                   DevAction::HOLD)
            << "\n"
            << "LONG Index             : "
            << static_cast<std::size_t>(
                   DevAction::LONG)
            << "\n"
            << "SHORT Index            : "
            << static_cast<std::size_t>(
                   DevAction::SHORT)
            << "\n"
            << "EXIT Index             : "
            << static_cast<std::size_t>(
                   DevAction::EXIT)
            << "\n"
            << "Position Space         : {-1, 0, +1}\n"
            << "Direct Reversal        : ALLOWED\n"
            << "============================================================\n\n";

        testActionCount();
        testActionIndexes();
        testActionNames();
        testActionValidation();
        testPositionValidation();

        testFlatTransitions();
        testLongTransitions();
        testShortTransitions();

        testHoldSemantics();
        testExitSemantics();

        testReversals();
        testPositionChangeDetection();
        testTurnover();

        testInvalidCurrentPosition();
        testInvalidAction();

        testCompleteTransitionMatrix();
        testDeterminism();

        std::cout
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.5 VALIDATION SUMMARY\n"
            << "============================================================\n"
            << "Action Count (4)                : PASSED\n"
            << "Action Index Contract           : PASSED\n"
            << "Action Names                    : PASSED\n"
            << "Action Validation               : PASSED\n"
            << "Discrete Position Contract      : PASSED\n"
            << "HOLD Semantics                  : PASSED\n"
            << "LONG Semantics                  : PASSED\n"
            << "SHORT Semantics                 : PASSED\n"
            << "EXIT Semantics                  : PASSED\n"
            << "Direct Reversal Semantics       : PASSED\n"
            << "Position-Change Detection       : PASSED\n"
            << "Turnover Semantics              : PASSED\n"
            << "Invalid Position Rejection      : PASSED\n"
            << "Invalid Action Rejection        : PASSED\n"
            << "Complete Transition Matrix      : PASSED\n"
            << "Deterministic Action Mapping    : PASSED\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.5 ACTION-SPACE VALIDATION PASSED\n"
            << "============================================================\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\n"
            << "============================================================\n"
            << "DEVAI PHASE 1.5 ACTION-SPACE VALIDATION FAILED\n"
            << "============================================================\n"
            << "Reason: "
            << exception.what()
            << "\n"
            << "============================================================\n";

        return 1;
    }
}