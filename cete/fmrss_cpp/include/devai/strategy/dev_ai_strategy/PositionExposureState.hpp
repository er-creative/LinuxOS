#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "devai/strategy/dev_ai_strategy/DevAction.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

namespace devai::strategy
{

// ============================================================================
// PositionExposureSnapshot
//
// Phase 2 episode-level view of the authoritative TradingEnvironment state.
//
// IMPORTANT:
//
// This structure does NOT independently calculate positions, quantity,
// entry price, P&L, brokerage, or slippage.
//
// TradingEnvironment remains the single source of truth.
// ============================================================================

struct PositionExposureSnapshot
{
    std::string symbol;

    std::int64_t decision_time{0};

    double position{0.0};

    double quantity{0.0};

    double entry_price{0.0};

    double exposure_fraction{0.0};

    std::size_t holding_bars{0};

    bool flat{true};

    bool long_position{false};

    bool short_position{false};

    bool initialized{false};
};


// ============================================================================
// PositionExposureState
//
// Phase 2.3 integration layer.
//
// Responsibilities:
//
//   - initialize one episode position state
//   - forward Dev actions to the frozen TradingEnvironment
//   - expose the authoritative position/exposure state
//   - preserve strict decision-time progression
//   - provide clean episode reset semantics
//
// It deliberately does NOT duplicate execution/accounting logic.
// ============================================================================

class PositionExposureState
{
public:

    explicit PositionExposureState(
        TradingEnvironmentConfig config = {});


    // ------------------------------------------------------------------------
    // Episode initialization / reset
    // ------------------------------------------------------------------------

    void reset(
        const std::string& symbol,
        std::int64_t decision_time,
        double market_price);


    // ------------------------------------------------------------------------
    // Apply one Dev action at one decision time
    // ------------------------------------------------------------------------

    TradingStepResult step(
        DevAction action,
        std::int64_t decision_time,
        double market_price);


    // ------------------------------------------------------------------------
    // Authoritative state
    // ------------------------------------------------------------------------

    PositionExposureSnapshot snapshot() const;

    const TradingEnvironmentState& environmentState() const noexcept;

    DevPortfolioState portfolioState() const;


    // ------------------------------------------------------------------------
    // Convenience accessors
    // ------------------------------------------------------------------------

    bool initialized() const noexcept;

    bool isFlat() const noexcept;

    bool isLong() const noexcept;

    bool isShort() const noexcept;

    double position() const noexcept;

    double quantity() const noexcept;

    double entryPrice() const noexcept;

    std::size_t holdingBars() const noexcept;

    const std::string& symbol() const noexcept;


private:

    TradingEnvironmentConfig config_;

    TradingEnvironment environment_;

    bool initialized_{false};


    void validateState() const;
};

} // namespace devai::strategy