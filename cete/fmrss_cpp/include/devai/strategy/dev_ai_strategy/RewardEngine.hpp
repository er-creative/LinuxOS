#pragma once

namespace devai::strategy
{

// ============================================================
// REWARD CONFIGURATION
// ============================================================
//
// Reward:
//
//   total =
//       net_equity_return
//       - turnover_penalty * turnover
//       - drawdown_penalty * drawdown_increase
//
// IMPORTANT:
//
// net_equity_return is expected to represent the actual
// environment-level equity return.
//
// In the completed TradingEnvironment this should already
// reflect:
//
//   - trading P&L
//   - brokerage
//   - statutory charges
//   - slippage
//
// Therefore RewardEngine must NOT calculate brokerage,
// exchange charges, STT, GST, stamp duty, etc.
//
// turnover_penalty is an additional behavioral penalty used
// to discourage unnecessary position changes.
// ============================================================

struct RewardConfig
{
    double turnover_penalty{0.00005};

    double drawdown_penalty{0.10};
};


// ============================================================
// REWARD INPUT
// ============================================================

struct RewardInput
{
    // Net change in equity for this environment step,
    // expressed as a decimal return.
    //
    // Example:
    //
    //   +0.001 = +0.10%
    //   -0.001 = -0.10%
    //
    double net_equity_return{0.0};


    // Absolute exposure change.
    //
    // Examples:
    //
    //   FLAT -> LONG   = 1
    //   LONG -> FLAT   = 1
    //   LONG -> SHORT  = 2
    //   LONG -> LONG   = 0
    //
    double turnover{0.0};


    // Drawdown before this step.
    //
    // Decimal representation:
    //
    //   0.05 = 5% drawdown
    //
    double previous_drawdown{0.0};


    // Drawdown after this step.
    double current_drawdown{0.0};
};


// ============================================================
// REWARD RESULT
// ============================================================

struct RewardResult
{
    // Net equity return itself.
    double pnl_component{0.0};


    // Always <= 0.
    double turnover_component{0.0};


    // Always <= 0.
    //
    // Applied only when drawdown increases.
    double drawdown_component{0.0};


    // Sum of all components.
    double total{0.0};
};


// ============================================================
// REWARD ENGINE
// ============================================================

class RewardEngine
{
public:

    explicit RewardEngine(
        RewardConfig config = {});


    [[nodiscard]]
    RewardResult calculate(
        const RewardInput& input) const;


    [[nodiscard]]
    const RewardConfig& config() const noexcept;


private:

    RewardConfig config_;
};

} // namespace devai::strategy