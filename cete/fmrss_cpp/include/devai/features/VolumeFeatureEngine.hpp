#pragma once

#include "devai/features/VolumeFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"
#include "devai/market/SessionState.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>


namespace devai::features
{

class VolumeFeatureEngine
{
public:

    // ========================================================================
    // Constructor
    // ========================================================================

    explicit VolumeFeatureEngine(
        std::string symbol
    );


    // ========================================================================
    // Update
    //
    // Consumes the latest legally available MarketSnapshot.
    //
    // Continuous rolling volume history:
    //
    //     1m / 5m / 15m
    //     AverageVolume20
    //     AverageVolume50
    //     RVOL20
    //     RVOL50
    //
    // survives trading-session boundaries.
    //
    // Session-specific state:
    //
    //     session cumulative volume
    //     session bar count
    //     session average volume per bar
    //
    // resets whenever TradingDate changes.
    // ========================================================================

    [[nodiscard]]
    VolumeFeatures update(
        const devai::market::MarketSnapshot& snapshot
    );


    // ========================================================================
    // Explicit complete reset
    //
    // This is NOT a normal daily/session reset.
    //
    // It clears:
    //
    //     1m rolling volume history
    //     5m rolling volume history
    //     15m rolling volume history
    //
    //     session cumulative volume
    //     session bar count
    //     current TradingDate
    //
    //     decision-time state
    //
    // Normal session transitions do NOT call this.
    // ========================================================================

    void reset() noexcept;


    // ========================================================================
    // Engine symbol
    // ========================================================================

    [[nodiscard]]
    const std::string& symbol() const noexcept
    {
        return symbol_;
    }


private:

    // ========================================================================
    // Per-timeframe continuous state
    //
    // Each timeframe remembers:
    //
    //     last consumed candle timestamp
    //     latest rolling volume history
    //
    // The longest Phase 3.3 window is 50 candles.
    //
    // These states survive trading-session boundaries.
    // ========================================================================

    struct TimeframeState
    {
        std::optional<std::int64_t>
            last_timestamp;


        std::deque<double>
            volume_history;


        void reset() noexcept;
    };


    // ========================================================================
    // Engine identity
    // ========================================================================

    std::string
        symbol_;


    // ========================================================================
    // Decision-time protection
    //
    // Used to prevent runtime state from moving backwards.
    //
    // Equal decision times are permitted because the same snapshot may
    // legitimately be replayed.
    // ========================================================================

    std::optional<std::int64_t>
        last_decision_time_;


    // ========================================================================
    // Continuous rolling volume states
    //
    // IMPORTANT:
    //
    // These DO NOT reset when TradingDate changes.
    // ========================================================================

    TimeframeState
        one_minute_state_;


    TimeframeState
        five_minute_state_;


    TimeframeState
        fifteen_minute_state_;


    // ========================================================================
    // Session identity
    //
    // TradingDate is used as the authoritative session identity.
    //
    // Example:
    //
    //     2026-09-25
    //
    //         session state accumulates
    //
    //     2026-09-28
    //
    //         TradingDate changes
    //                ↓
    //         session state resets
    //
    // Continuous rolling histories remain intact.
    // ========================================================================

    std::optional<devai::market::TradingDate>
        session_trading_date_;


    // ========================================================================
    // Session-specific volume state
    //
    // Only genuinely new 1-minute candles contribute to these values.
    //
    // Repeated snapshots therefore cannot double-count session volume.
    // ========================================================================

    double
        session_cumulative_volume_{0.0};


    std::size_t
        session_bar_count_{0};


    // ========================================================================
    // Timeframe update
    //
    // Returns true only when a genuinely new completed candle was consumed.
    //
    // Returns false when:
    //
    //     no candle is available
    //
    // or:
    //
    //     the same latest candle appears again in another snapshot.
    //
    // Throws if the candle stream moves backwards.
    // ========================================================================

    static bool updateTimeframe(
        TimeframeState& state,
        const std::optional<devai::market::Candle>& candle
    );


    // ========================================================================
    // Append one volume observation
    //
    // Maintains the bounded rolling history.
    // ========================================================================

    static void appendVolume(
        TimeframeState& state,
        double volume
    );


    // ========================================================================
    // Rolling average
    //
    // Uses the latest `period` observations.
    //
    // The current completed candle is included.
    // ========================================================================

    [[nodiscard]]
    static double rollingAverage(
        const std::deque<double>& history,
        std::size_t period
    );


    // ========================================================================
    // Build output features for one timeframe
    // ========================================================================

    [[nodiscard]]
    static TimeframeVolumeFeatures makeFeatures(
        const TimeframeState& state,
        const std::optional<devai::market::Candle>& candle
    );


    // ========================================================================
    // Reset session-only state
    //
    // Does NOT clear 1m / 5m / 15m rolling histories.
    // ========================================================================

    void resetSessionState() noexcept;


    // ========================================================================
    // Detect session transition
    //
    // TradingDate is authoritative.
    //
    // Same TradingDate:
    //
    //     keep current session state
    //
    // Different TradingDate:
    //
    //     reset session cumulative volume
    //     reset session bar count
    //     preserve rolling timeframe histories
    // ========================================================================

    void handleSessionBoundary(
        const devai::market::MarketSnapshot& snapshot
    );
};

} // namespace devai::features