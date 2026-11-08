#pragma once

#include <cstdint>

namespace devai::strategy::risk
{

// ============================================================================
// RiskStateInput
//
// Strategy-independent account information available at decision time.
//
// position:
//      -1.0 = SHORT
//       0.0 = FLAT
//      +1.0 = LONG
//
// quantity:
//      Absolute position quantity.
//
// current_price:
//      Current observable market price.
//
// The RiskStateEngine does NOT calculate P&L. Equity must already include
// all economic effects supplied by the execution/accounting layer.
// ============================================================================
struct RiskStateInput
{
    std::int64_t decision_time{0};

    double initial_equity{0.0};
    double current_equity{0.0};

    double position{0.0};
    double quantity{0.0};
    double current_price{0.0};
};


// ============================================================================
// RiskState
//
// Shared CETE account-risk state.
//
// current_drawdown:
//      Drawdown from the highest equity observed in the current episode.
//
// maximum_drawdown:
//      Largest current_drawdown observed in the current episode.
//
// gross_exposure:
//      Absolute market value of the current position.
//
// exposure_ratio:
//      gross_exposure / current_equity.
//
// No strategy policy is represented here.
// ============================================================================
struct RiskState
{
    std::int64_t decision_time{0};

    double initial_equity{0.0};
    double current_equity{0.0};
    double peak_equity{0.0};

    double current_drawdown{0.0};
    double maximum_drawdown{0.0};

    double position{0.0};
    double quantity{0.0};
    double current_price{0.0};

    double gross_exposure{0.0};
    double exposure_ratio{0.0};

    bool initialized{false};
    bool numerical_integrity{false};
};


// ============================================================================
// RiskStateEngine
//
// CETE SHARED COMPONENT.
//
// Responsibilities:
//
//   1. Track peak equity.
//   2. Calculate current drawdown.
//   3. Track maximum drawdown.
//   4. Calculate gross market exposure.
//   5. Calculate exposure ratio.
//   6. Enforce chronological updates.
//   7. Reset state cleanly between episodes.
//   8. Validate numerical integrity.
//
// It deliberately does NOT:
//
//   - calculate P&L
//   - calculate brokerage
//   - calculate slippage
//   - select actions
//   - calculate reward
//   - terminate episodes
//   - apply DevAI-specific risk limits
//   - use future prices or forward returns
// ============================================================================
class RiskStateEngine
{
public:
    RiskStateEngine() = default;

    void reset(
        const RiskStateInput& input);

    const RiskState& update(
        const RiskStateInput& input);

    const RiskState& state() const noexcept;

    bool initialized() const noexcept;

    void validate(
        const RiskState& state) const;

private:
    RiskState state_{};

    static bool finite(
        double value) noexcept;

    static bool finiteNonNegative(
        double value) noexcept;

    static bool approximatelyEqual(
        double left,
        double right) noexcept;

    static void validatePosition(
        double position);

    static void validateInput(
        const RiskStateInput& input);

    static double calculateGrossExposure(
        double position,
        double quantity,
        double current_price);

    static double calculateExposureRatio(
        double gross_exposure,
        double current_equity);

    static double calculateDrawdown(
        double peak_equity,
        double current_equity);
};

} // namespace devai::strategy::risk