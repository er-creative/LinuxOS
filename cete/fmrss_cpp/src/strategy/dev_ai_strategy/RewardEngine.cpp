#include "devai/strategy/dev_ai_strategy/RewardEngine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace devai::strategy
{

namespace
{

// ============================================================
// FINITE-VALUE VALIDATION
// ============================================================

void requireFinite(
    double value,
    const char* name)
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument(
            std::string(
                "RewardEngine received non-finite value: ") +
            name);
    }
}

} // namespace


// ============================================================
// CONSTRUCTOR
// ============================================================

RewardEngine::RewardEngine(
    RewardConfig config)
    :
    config_(config)
{
    requireFinite(
        config_.turnover_penalty,
        "turnover_penalty");

    requireFinite(
        config_.drawdown_penalty,
        "drawdown_penalty");


    if (config_.turnover_penalty < 0.0)
    {
        throw std::invalid_argument(
            "RewardEngine turnover_penalty must be >= 0.");
    }


    if (config_.drawdown_penalty < 0.0)
    {
        throw std::invalid_argument(
            "RewardEngine drawdown_penalty must be >= 0.");
    }
}


// ============================================================
// CALCULATE REWARD
// ============================================================

RewardResult RewardEngine::calculate(
    const RewardInput& input) const
{
    // --------------------------------------------------------
    // Validate input
    // --------------------------------------------------------

    requireFinite(
        input.net_equity_return,
        "net_equity_return");

    requireFinite(
        input.turnover,
        "turnover");

    requireFinite(
        input.previous_drawdown,
        "previous_drawdown");

    requireFinite(
        input.current_drawdown,
        "current_drawdown");


    // --------------------------------------------------------
    // Domain validation
    // --------------------------------------------------------

    if (input.turnover < 0.0)
    {
        throw std::invalid_argument(
            "RewardEngine turnover must be >= 0.");
    }


    if (input.previous_drawdown < 0.0)
    {
        throw std::invalid_argument(
            "RewardEngine previous_drawdown must be >= 0.");
    }


    if (input.current_drawdown < 0.0)
    {
        throw std::invalid_argument(
            "RewardEngine current_drawdown must be >= 0.");
    }


    // --------------------------------------------------------
    // Drawdown increase
    // --------------------------------------------------------
    //
    // Recovery from drawdown is NOT penalized.
    //
    // Example:
    //
    // previous = 0.10
    // current  = 0.06
    //
    // increase = 0
    //
    // --------------------------------------------------------

    const double drawdown_increase =
        std::max(
            0.0,
            input.current_drawdown -
            input.previous_drawdown);


    // --------------------------------------------------------
    // Components
    // --------------------------------------------------------

    RewardResult result;

    result.pnl_component =
        input.net_equity_return;


    result.turnover_component =
        -config_.turnover_penalty *
        input.turnover;


    result.drawdown_component =
        -config_.drawdown_penalty *
        drawdown_increase;


    // --------------------------------------------------------
    // Final reward
    // --------------------------------------------------------

    result.total =
        result.pnl_component +
        result.turnover_component +
        result.drawdown_component;


    // --------------------------------------------------------
    // Final numerical-integrity check
    // --------------------------------------------------------

    if (!std::isfinite(
            result.pnl_component) ||
        !std::isfinite(
            result.turnover_component) ||
        !std::isfinite(
            result.drawdown_component) ||
        !std::isfinite(
            result.total))
    {
        throw std::runtime_error(
            "RewardEngine produced non-finite reward.");
    }


    return result;
}


// ============================================================
// CONFIGURATION ACCESS
// ============================================================

const RewardConfig&
RewardEngine::config() const noexcept
{
    return config_;
}

} // namespace devai::strategy