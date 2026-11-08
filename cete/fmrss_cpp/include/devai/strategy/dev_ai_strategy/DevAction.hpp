#pragma once

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace devai::strategy
{

// ============================================================
// DEVAI ACTION SPACE
// ============================================================
//
// Agent: Dev
//
// Actions represent TARGET POSITION INTENT.
//
// They are NOT:
//   - broker order types
//   - exchange instructions
//   - quantities
//   - limit/market order specifications
//
// Position convention:
//
//   -1.0 = fully short
//    0.0 = flat
//   +1.0 = fully long
//
// HOLD is state-dependent:
//
//   HOLD keeps the current position unchanged.
//
// LONG, SHORT and EXIT are absolute target-position actions.
//
// Examples:
//
//   current  0, HOLD  ->  0
//   current  0, LONG  -> +1
//   current +1, HOLD  -> +1
//   current +1, SHORT -> -1
//   current -1, LONG  -> +1
//   current -1, EXIT  ->  0
//
// ============================================================

enum class DevAction : std::size_t
{
    HOLD  = 0,
    LONG  = 1,
    SHORT = 2,
    EXIT  = 3
};


// ============================================================
// ACTION-SPACE SIZE
// ============================================================

inline constexpr std::size_t DEV_ACTION_COUNT = 4;


// ============================================================
// VALID POSITION STATES
// ============================================================

inline constexpr double DEV_POSITION_SHORT = -1.0;
inline constexpr double DEV_POSITION_FLAT  =  0.0;
inline constexpr double DEV_POSITION_LONG  =  1.0;


// ============================================================
// ACTION VALIDATION
// ============================================================

[[nodiscard]]
constexpr bool isValidAction(
    DevAction action) noexcept
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


// ============================================================
// ACTION NAME
// ============================================================

[[nodiscard]]
constexpr std::string_view actionName(
    DevAction action) noexcept
{
    switch (action)
    {
        case DevAction::HOLD:
            return "HOLD";

        case DevAction::LONG:
            return "LONG";

        case DevAction::SHORT:
            return "SHORT";

        case DevAction::EXIT:
            return "EXIT";
    }

    return "INVALID";
}


// ============================================================
// POSITION VALIDATION
// ============================================================
//
// Phase 1.5 uses a discrete position model.
//
// Only:
//
//   -1.0
//    0.0
//   +1.0
//
// are legal.
//
// Fractional sizing belongs to a future position-sizing/risk layer.
// ============================================================

[[nodiscard]]
constexpr bool isValidDevPosition(
    double position) noexcept
{
    return
        position == DEV_POSITION_SHORT ||
        position == DEV_POSITION_FLAT  ||
        position == DEV_POSITION_LONG;
}


// ============================================================
// TARGET POSITION
// ============================================================
//
// HOLD:
//     preserve current position.
//
// LONG:
//     target +1.
//
// SHORT:
//     target -1.
//
// EXIT:
//     target 0.
//
// The function intentionally does NOT calculate:
//   - turnover
//   - transaction costs
//   - slippage
//   - P&L
//   - reward
//
// Those belong to TradingEnvironment / RewardEngine.
// ============================================================

[[nodiscard]]
inline double targetPosition(
    DevAction action,
    double current_position)
{
    if (!isValidDevPosition(current_position))
    {
        throw std::invalid_argument(
            "Dev targetPosition received invalid current position.");
    }

    switch (action)
    {
        case DevAction::HOLD:
            return current_position;

        case DevAction::LONG:
            return DEV_POSITION_LONG;

        case DevAction::SHORT:
            return DEV_POSITION_SHORT;

        case DevAction::EXIT:
            return DEV_POSITION_FLAT;
    }

    throw std::invalid_argument(
        "Dev targetPosition received invalid action.");
}


// ============================================================
// TURNOVER
// ============================================================
//
// Turnover represents absolute position change.
//
// Examples:
//
//   flat  -> long   = 1
//   long  -> flat   = 1
//   long  -> short  = 2
//   short -> long   = 2
//   long  -> long   = 0
//
// This is useful later for execution costs and reward penalties.
// ============================================================

[[nodiscard]]
constexpr double actionTurnover(
    double current_position,
    double target_position) noexcept
{
    const double difference =
        target_position - current_position;

    return
        difference < 0.0
            ? -difference
            : difference;
}


// ============================================================
// POSITION CHANGE
// ============================================================

[[nodiscard]]
constexpr bool changesPosition(
    double current_position,
    double target_position) noexcept
{
    return
        current_position != target_position;
}


// ============================================================
// REVERSAL
// ============================================================
//
// A reversal occurs only:
//
//   LONG  -> SHORT
//   SHORT -> LONG
//
// Flat transitions are not reversals.
// ============================================================

[[nodiscard]]
constexpr bool isPositionReversal(
    double current_position,
    double target_position) noexcept
{
    return
        (current_position == DEV_POSITION_LONG &&
         target_position == DEV_POSITION_SHORT)
        ||
        (current_position == DEV_POSITION_SHORT &&
         target_position == DEV_POSITION_LONG);
}

} // namespace devai::strategy