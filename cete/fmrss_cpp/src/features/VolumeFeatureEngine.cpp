#include "devai/features/VolumeFeatureEngine.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>


namespace devai::features
{

namespace
{

// ============================================================================
// Configuration
// ============================================================================

constexpr std::size_t
    VOLUME_HISTORY_LIMIT = 50;


constexpr std::size_t
    AVERAGE_VOLUME_20_PERIOD = 20;


constexpr std::size_t
    AVERAGE_VOLUME_50_PERIOD = 50;


// ============================================================================
// Local helpers
// ============================================================================

double nanValue() noexcept
{
    return
        std::numeric_limits<double>::quiet_NaN();
}


bool validVolume(
    double volume) noexcept
{
    return
        std::isfinite(volume) &&
        volume >= 0.0;
}

} // namespace


// ============================================================================
// TimeframeState reset
// ============================================================================

void
VolumeFeatureEngine::TimeframeState::reset() noexcept
{
    last_timestamp.reset();

    volume_history.clear();
}


// ============================================================================
// Constructor
// ============================================================================

VolumeFeatureEngine::VolumeFeatureEngine(
    std::string symbol)
    :
    symbol_(std::move(symbol))
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "VolumeFeatureEngine symbol cannot be empty."
        );
    }
}


// ============================================================================
// Explicit complete reset
//
// This is used when the complete engine is deliberately restarted.
//
// It is NOT called at a normal trading-session boundary.
// ============================================================================

void
VolumeFeatureEngine::reset() noexcept
{
    // Continuous rolling state.

    one_minute_state_.reset();

    five_minute_state_.reset();

    fifteen_minute_state_.reset();


    // Session-specific state.

    resetSessionState();

    session_trading_date_.reset();


    // Runtime protection state.

    last_decision_time_.reset();
}


// ============================================================================
// Session-only reset
//
// IMPORTANT:
//
// Rolling 1m / 5m / 15m histories are deliberately NOT cleared here.
// ============================================================================

void
VolumeFeatureEngine::resetSessionState() noexcept
{
    session_cumulative_volume_ =
        0.0;


    session_bar_count_ =
        0;
}


// ============================================================================
// Handle trading-session boundary
//
// TradingDate is the authoritative identity of the current session.
//
// Continuous rolling volume history survives.
//
// Only session-specific state resets.
// ============================================================================

void
VolumeFeatureEngine::handleSessionBoundary(
    const devai::market::MarketSnapshot& snapshot)
{
    // First snapshot ever processed by this engine.
    //
    // Establish the session identity and start with clean
    // session-specific state.

    if (!session_trading_date_)
    {
        session_trading_date_ =
            snapshot.trading_date;


        resetSessionState();


        return;
    }


    // Same trading date:
    //
    // Do nothing, even if the same first-session snapshot is replayed.

    if (
        *session_trading_date_ ==
        snapshot.trading_date
    )
    {
        return;
    }


    // TradingDate changed.
    //
    // This is a genuine session transition.

    resetSessionState();


    session_trading_date_ =
        snapshot.trading_date;
}


// ============================================================================
// Append volume
//
// Only the latest 50 observations are required because the longest
// Phase 3.3 rolling window is AverageVolume50.
// ============================================================================

void
VolumeFeatureEngine::appendVolume(
    TimeframeState& state,
    double volume)
{
    state.volume_history.push_back(
        volume
    );


    while (
        state.volume_history.size() >
        VOLUME_HISTORY_LIMIT
    )
    {
        state.volume_history.pop_front();
    }
}


// ============================================================================
// Rolling average
//
// The current completed candle is included in the rolling window.
//
// Example:
//
//     AverageVolume20
//
// uses the latest 20 legally available completed candles,
// including the current candle.
// ============================================================================

double
VolumeFeatureEngine::rollingAverage(
    const std::deque<double>& history,
    std::size_t period)
{
    if (
        period == 0 ||
        history.size() < period
    )
    {
        return nanValue();
    }


    double sum =
        0.0;


    const std::size_t start =
        history.size() -
        period;


    for (
        std::size_t index = start;
        index < history.size();
        ++index
    )
    {
        sum +=
            history[index];
    }


    return
        sum /
        static_cast<double>(
            period
        );
}


// ============================================================================
// Consume one timeframe
//
// Returns:
//
//     true  -> a genuinely new completed candle was consumed
//     false -> no candle or repeated latest candle
//
// Repeated 5m / 15m candles are therefore never inserted into rolling
// histories more than once.
// ============================================================================

bool
VolumeFeatureEngine::updateTimeframe(
    TimeframeState& state,
    const std::optional<devai::market::Candle>& candle)
{
    if (!candle)
    {
        return false;
    }


    // ------------------------------------------------------------------------
    // Candle integrity
    // ------------------------------------------------------------------------

    if (!candle->valid())
    {
        throw std::runtime_error(
            "VolumeFeatureEngine received invalid candle."
        );
    }


    if (!validVolume(candle->volume))
    {
        throw std::runtime_error(
            "VolumeFeatureEngine received invalid candle volume."
        );
    }


    // ------------------------------------------------------------------------
    // Timestamp protection
    // ------------------------------------------------------------------------

    if (state.last_timestamp)
    {
        // Same completed candle appearing in another runtime snapshot.
        //
        // This is normal for 5m and 15m streams.

        if (
            candle->timestamp ==
            *state.last_timestamp
        )
        {
            return false;
        }


        // A feature engine must never consume older data after newer data.

        if (
            candle->timestamp <
            *state.last_timestamp
        )
        {
            throw std::runtime_error(
                "VolumeFeatureEngine candle timestamp moved backwards."
            );
        }
    }


    // ------------------------------------------------------------------------
    // Consume exactly once
    // ------------------------------------------------------------------------

    appendVolume(
        state,
        candle->volume
    );


    state.last_timestamp =
        candle->timestamp;


    return true;
}


// ============================================================================
// Build features for one timeframe
// ============================================================================

TimeframeVolumeFeatures
VolumeFeatureEngine::makeFeatures(
    const TimeframeState& state,
    const std::optional<devai::market::Candle>& candle)
{
    TimeframeVolumeFeatures output;


    if (!candle)
    {
        return output;
    }


    if (!validVolume(candle->volume))
    {
        return output;
    }


    // ========================================================================
    // Current volume
    // ========================================================================

    output.volume =
        candle->volume;


    output.has_volume =
        true;


    // ========================================================================
    // Average Volume 20
    // ========================================================================

    const double average20 =
        rollingAverage(
            state.volume_history,
            AVERAGE_VOLUME_20_PERIOD
        );


    if (std::isfinite(average20))
    {
        output.average_volume_20 =
            average20;


        output.has_average_volume_20 =
            true;


        // RVOL is undefined if the rolling average is zero.

        if (average20 > 0.0)
        {
            output.relative_volume_20 =
                candle->volume /
                average20;


            output.has_relative_volume_20 =
                true;
        }
    }


    // ========================================================================
    // Average Volume 50
    // ========================================================================

    const double average50 =
        rollingAverage(
            state.volume_history,
            AVERAGE_VOLUME_50_PERIOD
        );


    if (std::isfinite(average50))
    {
        output.average_volume_50 =
            average50;


        output.has_average_volume_50 =
            true;


        // RVOL is undefined if the rolling average is zero.

        if (average50 > 0.0)
        {
            output.relative_volume_50 =
                candle->volume /
                average50;


            output.has_relative_volume_50 =
                true;
        }
    }


    return output;
}


// ============================================================================
// Main update
// ============================================================================

VolumeFeatures
VolumeFeatureEngine::update(
    const devai::market::MarketSnapshot& snapshot)
{
    // ========================================================================
    // Symbol protection
    // ========================================================================

    if (
        snapshot.symbol !=
        symbol_
    )
    {
        throw std::invalid_argument(
            "VolumeFeatureEngine snapshot symbol does not match engine."
        );
    }


    // ========================================================================
    // Decision-time protection
    //
    // Equal decision times are permitted because a runtime snapshot may
    // legitimately be replayed.
    //
    // Moving backwards is forbidden.
    // ========================================================================

    if (
        last_decision_time_ &&
        snapshot.decision_time <
            *last_decision_time_
    )
    {
        throw std::runtime_error(
            "VolumeFeatureEngine decision time moved backwards."
        );
    }


    // ========================================================================
    // Session boundary
    //
    // TradingDate determines session identity.
    //
    // This resets ONLY:
    //
    //     session cumulative volume
    //     session bar count
    //
    // It does NOT reset:
    //
    //     1m rolling volume history
    //     5m rolling volume history
    //     15m rolling volume history
    // ========================================================================

    handleSessionBoundary(
        snapshot
    );


    // ========================================================================
    // Consume legally available completed candles
    // ========================================================================

    const bool new_one_minute =
        updateTimeframe(
            one_minute_state_,
            snapshot.one_minute
        );


    static_cast<void>(
        updateTimeframe(
            five_minute_state_,
            snapshot.five_minute
        )
    );


    static_cast<void>(
        updateTimeframe(
            fifteen_minute_state_,
            snapshot.fifteen_minute
        )
    );


    // ========================================================================
    // Current-session 1-minute volume
    //
    // Session volume advances ONLY when a genuinely new 1-minute candle
    // was consumed.
    //
    // Repeated snapshots therefore cannot double-count volume.
    // ========================================================================

    if (
        new_one_minute &&
        snapshot.one_minute
    )
    {
        session_cumulative_volume_ +=
            snapshot.one_minute->volume;


        ++session_bar_count_;
    }


    // ========================================================================
    // Decision time
    //
    // Record only after successful processing.
    // ========================================================================

    last_decision_time_ =
        snapshot.decision_time;


    // ========================================================================
    // Build output
    // ========================================================================

    VolumeFeatures output;


    output.symbol =
        symbol_;


    output.decision_time =
        snapshot.decision_time;


    // ------------------------------------------------------------------------
    // 1-minute
    // ------------------------------------------------------------------------

    output.one_minute =
        makeFeatures(
            one_minute_state_,
            snapshot.one_minute
        );


    // ------------------------------------------------------------------------
    // 5-minute
    // ------------------------------------------------------------------------

    output.five_minute =
        makeFeatures(
            five_minute_state_,
            snapshot.five_minute
        );


    // ------------------------------------------------------------------------
    // 15-minute
    // ------------------------------------------------------------------------

    output.fifteen_minute =
        makeFeatures(
            fifteen_minute_state_,
            snapshot.fifteen_minute
        );


    // ========================================================================
    // Session features
    // ========================================================================

    output.session_cumulative_volume =
        session_cumulative_volume_;


    output.session_bar_count =
        session_bar_count_;


    if (session_bar_count_ > 0)
    {
        output.session_average_volume_per_bar =
            session_cumulative_volume_ /
            static_cast<double>(
                session_bar_count_
            );


        output.has_session_volume =
            true;
    }


    return output;
}

} // namespace devai::features