#pragma once

#include <cstddef>
#include <cstdint>

namespace devai::strategy
{

// ============================================================================
// TerminalReason
//
// Describes WHY a DevAI episode terminated.
//
// NONE means that the episode may continue.
// ============================================================================
enum class TerminalReason
{
    NONE = 0,

    END_OF_DATA,
    MAXIMUM_STEPS,
    MAXIMUM_DRAWDOWN,
    MINIMUM_EQUITY
};


// ============================================================================
// TerminalConditionConfig
//
// DEVAI POLICY.
//
// maximum_episode_steps:
//
//     Maximum number of completed episode steps allowed.
//
//     0 disables this policy.
//
// maximum_drawdown:
//
//     Maximum permitted current account drawdown.
//
//     Example:
//
//         0.10 = 10%
//
//     Must satisfy:
//
//         0 <= maximum_drawdown < 1
//
//     A value of 0 disables this policy.
//
// minimum_equity_fraction:
//
//     Minimum permitted current equity as a fraction of initial equity.
//
//     Example:
//
//         0.50 = terminate when equity <= 50% of initial equity.
//
//     Must satisfy:
//
//         0 <= minimum_equity_fraction <= 1
//
//     A value of 0 disables this policy.
//
// IMPORTANT:
//
// These values are policy configuration, not CETE market/account facts.
// ============================================================================
struct TerminalConditionConfig
{
    std::size_t maximum_episode_steps{0};

    double maximum_drawdown{0.0};

    double minimum_equity_fraction{0.0};
};


// ============================================================================
// TerminalConditionInput
//
// Contains only information already available at the CURRENT decision point.
//
// No future price.
// No forward return.
// No future episode state.
// ============================================================================
struct TerminalConditionInput
{
    std::int64_t decision_time{0};

    std::size_t completed_steps{0};

    bool has_next_state{true};

    double initial_equity{0.0};

    double current_equity{0.0};

    double current_drawdown{0.0};

    double maximum_drawdown{0.0};
};


// ============================================================================
// TerminalConditionResult
// ============================================================================
struct TerminalConditionResult
{
    bool terminal{false};

    TerminalReason reason{TerminalReason::NONE};

    std::int64_t decision_time{0};

    std::size_t completed_steps{0};

    double current_equity{0.0};

    double current_drawdown{0.0};

    bool numerical_integrity{false};
};


// ============================================================================
// TerminalConditionEngine
//
// DevAI episode-policy component.
//
// Responsibilities:
//
//     - detect end of historical data
//     - enforce optional maximum episode length
//     - enforce optional maximum drawdown policy
//     - enforce optional minimum-equity policy
//     - provide deterministic termination reason
//
// It deliberately does NOT:
//
//     - calculate drawdown
//     - calculate P&L
//     - calculate brokerage
//     - calculate slippage
//     - calculate reward
//     - execute EXIT
//     - modify positions
//     - inspect future candles
//     - inspect ForwardReturn_N
//
// Priority when multiple conditions are simultaneously true:
//
//     1. END_OF_DATA
//     2. MAXIMUM_DRAWDOWN
//     3. MINIMUM_EQUITY
//     4. MAXIMUM_STEPS
//
// The priority exists only so that termination reporting is deterministic.
// ============================================================================
class TerminalConditionEngine
{
public:
    explicit TerminalConditionEngine(
        TerminalConditionConfig config = {});

    [[nodiscard]]
    TerminalConditionResult evaluate(
        const TerminalConditionInput& input) const;

    [[nodiscard]]
    const TerminalConditionConfig&
    config() const noexcept;

    [[nodiscard]]
    static const char*
    reasonName(
        TerminalReason reason) noexcept;

private:
    TerminalConditionConfig config_;

    static bool finite(
        double value) noexcept;

    static void validateConfig(
        const TerminalConditionConfig& config);

    static void validateInput(
        const TerminalConditionInput& input);

    static TerminalConditionResult makeResult(
        const TerminalConditionInput& input,
        bool terminal,
        TerminalReason reason);
};

} // namespace devai::strategy