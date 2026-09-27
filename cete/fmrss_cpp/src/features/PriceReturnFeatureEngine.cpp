#include "devai/features/PriceReturnFeatureEngine.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace devai::features
{

namespace
{

double nanValue() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


bool validPositive(double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace


// ============================================================================
// Constructor
// ============================================================================

PriceReturnFeatureEngine::PriceReturnFeatureEngine(
    std::string symbol,
    std::size_t maximum_history)
    :
    symbol_(std::move(symbol)),
    maximum_history_(maximum_history)
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "PriceReturnFeatureEngine symbol cannot be empty."
        );
    }

    // A 30-bar return requires 31 observations:
    //
    //     current close
    //     +
    //     close 30 observations earlier

    if (maximum_history_ < 31)
    {
        throw std::invalid_argument(
            "PriceReturnFeatureEngine maximum_history must be at least 31."
        );
    }
}


// ============================================================================
// Explicit reset
//
// CETE POLICY:
//
// reset() means explicit engine reinitialization.
//
// A new trading session does NOT reset continuous price/return history.
// ============================================================================

void PriceReturnFeatureEngine::reset() noexcept
{
    one_minute_history_.clear();
    five_minute_history_.clear();
    fifteen_minute_history_.clear();

    last_decision_time_.reset();
}


// ============================================================================
// Append candle only if genuinely new
//
// Repeated MarketSnapshots can expose the same completed 5m/15m candle.
// Such candles must be consumed only once.
// ============================================================================

bool PriceReturnFeatureEngine::appendIfNew(
    std::deque<devai::market::Candle>& history,
    const std::optional<devai::market::Candle>& candle,
    std::size_t maximum_history)
{
    // No candle available for this timeframe.

    if (!candle)
    {
        return false;
    }


    // ------------------------------------------------------------------------
    // Candle validity
    // ------------------------------------------------------------------------

    if (!candle->valid())
    {
        throw std::runtime_error(
            "PriceReturnFeatureEngine received invalid candle."
        );
    }


    // ------------------------------------------------------------------------
    // Existing history checks
    // ------------------------------------------------------------------------

    if (!history.empty())
    {
        const devai::market::Candle& previous =
            history.back();


        // Symbol must remain constant within this timeframe history.

        if (candle->symbol != previous.symbol)
        {
            throw std::runtime_error(
                "PriceReturnFeatureEngine candle symbol changed "
                "inside timeframe history."
            );
        }


        // Same timestamp means the same completed candle is being exposed
        // again by a later/repeated snapshot.
        //
        // Do not append it again.

        if (candle->timestamp == previous.timestamp)
        {
            return false;
        }


        // A timeframe stream must never move backwards.

        if (candle->timestamp < previous.timestamp)
        {
            throw std::runtime_error(
                "PriceReturnFeatureEngine candle timestamp moved backwards."
            );
        }
    }


    // ------------------------------------------------------------------------
    // Append new observation
    // ------------------------------------------------------------------------

    history.push_back(
        *candle
    );


    // ------------------------------------------------------------------------
    // Bound memory
    //
    // This is rolling-history trimming, NOT a session reset.
    // ------------------------------------------------------------------------

    while (history.size() > maximum_history)
    {
        history.pop_front();
    }


    return true;
}


// ============================================================================
// Current candle return
//
//     (Close - Open) / Open
// ============================================================================

double PriceReturnFeatureEngine::candleReturn(
    const devai::market::Candle& candle)
{
    if (
        !validPositive(candle.open) ||
        !std::isfinite(candle.close)
    )
    {
        return nanValue();
    }


    return
        (
            candle.close -
            candle.open
        )
        /
        candle.open;
}


// ============================================================================
// Close-to-close return
//
//     current_close / previous_close - 1
//
// History intentionally continues across trading sessions.
// ============================================================================

double PriceReturnFeatureEngine::closeToCloseReturn(
    const std::deque<devai::market::Candle>& history)
{
    if (history.size() < 2)
    {
        return nanValue();
    }


    const double previous_close =
        history[
            history.size() - 2
        ].close;


    const double current_close =
        history.back().close;


    if (
        !validPositive(previous_close) ||
        !std::isfinite(current_close)
    )
    {
        return nanValue();
    }


    return
        (
            current_close /
            previous_close
        )
        -
        1.0;
}


// ============================================================================
// Log return
//
//     ln(current_close / previous_close)
// ============================================================================

double PriceReturnFeatureEngine::logReturn(
    const std::deque<devai::market::Candle>& history)
{
    if (history.size() < 2)
    {
        return nanValue();
    }


    const double previous_close =
        history[
            history.size() - 2
        ].close;


    const double current_close =
        history.back().close;


    if (
        !validPositive(previous_close) ||
        !validPositive(current_close)
    )
    {
        return nanValue();
    }


    return std::log(
        current_close /
        previous_close
    );
}


// ============================================================================
// N-bar return
//
//     current_close / close_N_observations_ago - 1
//
// Examples:
//
//     5-bar return  -> requires 6 observations
//     15-bar return -> requires 16 observations
//     30-bar return -> requires 31 observations
//
// "Bar" means real completed trading observation.
// ============================================================================

double PriceReturnFeatureEngine::nBarReturn(
    const std::deque<devai::market::Candle>& history,
    std::size_t bars)
{
    if (bars == 0)
    {
        return nanValue();
    }


    if (history.size() < bars + 1)
    {
        return nanValue();
    }


    const std::size_t reference_index =
        history.size() -
        bars -
        1;


    const double reference_close =
        history[
            reference_index
        ].close;


    const double current_close =
        history.back().close;


    if (
        !validPositive(reference_close) ||
        !std::isfinite(current_close)
    )
    {
        return nanValue();
    }


    return
        (
            current_close /
            reference_close
        )
        -
        1.0;
}


// ============================================================================
// Candle body percent
//
//     (Close - Open) / Open
// ============================================================================

double PriceReturnFeatureEngine::candleBodyPercent(
    const devai::market::Candle& candle)
{
    if (
        !validPositive(candle.open) ||
        !std::isfinite(candle.close)
    )
    {
        return nanValue();
    }


    return
        (
            candle.close -
            candle.open
        )
        /
        candle.open;
}


// ============================================================================
// Candle range percent
//
//     (High - Low) / Open
// ============================================================================

double PriceReturnFeatureEngine::candleRangePercent(
    const devai::market::Candle& candle)
{
    if (
        !validPositive(candle.open) ||
        !std::isfinite(candle.high) ||
        !std::isfinite(candle.low)
    )
    {
        return nanValue();
    }


    if (candle.high < candle.low)
    {
        return nanValue();
    }


    return
        (
            candle.high -
            candle.low
        )
        /
        candle.open;
}


// ============================================================================
// Close location
//
//             Close - Low
//     ---------------------------
//              High - Low
//
//     0.0 = close at low
//     0.5 = close at midpoint
//     1.0 = close at high
//
// Zero-range candle returns 0.5.
// ============================================================================

double PriceReturnFeatureEngine::closeLocation(
    const devai::market::Candle& candle)
{
    if (
        !std::isfinite(candle.high) ||
        !std::isfinite(candle.low) ||
        !std::isfinite(candle.close)
    )
    {
        return nanValue();
    }


    const double range =
        candle.high -
        candle.low;


    if (range < 0.0)
    {
        return nanValue();
    }


    if (range == 0.0)
    {
        return 0.5;
    }


    return
        (
            candle.close -
            candle.low
        )
        /
        range;
}


// ============================================================================
// Main update
// ============================================================================

PriceReturnFeatures
PriceReturnFeatureEngine::update(
    const devai::market::MarketSnapshot& snapshot)
{
    // ========================================================================
    // 1. Snapshot symbol protection
    // ========================================================================

    if (snapshot.symbol != symbol_)
    {
        throw std::invalid_argument(
            "PriceReturnFeatureEngine snapshot symbol does not match engine."
        );
    }


    // ========================================================================
    // 2. Decision-time protection
    //
    // Same decision time is allowed.
    //
    // A decreasing decision time is forbidden.
    // ========================================================================

    if (
        last_decision_time_ &&
        snapshot.decision_time <
            *last_decision_time_
    )
    {
        throw std::runtime_error(
            "PriceReturnFeatureEngine decision time moved backwards."
        );
    }


    // ========================================================================
    // 3. CETE CROSS-SESSION CONTINUITY
    //
    // IMPORTANT:
    //
    // There is intentionally NO:
    //
    //     if (snapshot.new_session)
    //     {
    //         history.clear();
    //     }
    //
    // Price/return history is continuous state.
    //
    // Example:
    //
    //     Friday final real candle
    //              |
    //              | market closed
    //              | no synthetic candles
    //              v
    //     Monday first real candle
    //
    // Monday's first real candle is simply the next trading observation.
    //
    // Consequently:
    //
    //     close-to-close return
    //     log return
    //     5-bar return
    //     15-bar return
    //     30-bar return
    //
    // can legally span a trading-session boundary.
    //
    // Session-specific information will be handled by separate feature state.
    // ========================================================================


    // ========================================================================
    // 4. Append genuinely new timeframe observations
    // ========================================================================

    appendIfNew(
        one_minute_history_,
        snapshot.one_minute,
        maximum_history_
    );


    appendIfNew(
        five_minute_history_,
        snapshot.five_minute,
        maximum_history_
    );


    appendIfNew(
        fifteen_minute_history_,
        snapshot.fifteen_minute,
        maximum_history_
    );


    // Record decision time only after all timeframe processing succeeds.

    last_decision_time_ =
        snapshot.decision_time;


    // ========================================================================
    // 5. Output object
    // ========================================================================

    PriceReturnFeatures output;


    output.symbol =
        symbol_;


    output.decision_time =
        snapshot.decision_time;


    // ========================================================================
    // 6. Current snapshot availability
    // ========================================================================

    output.has_one_minute =
        snapshot.one_minute.has_value();


    output.has_five_minute =
        snapshot.five_minute.has_value();


    output.has_fifteen_minute =
        snapshot.fifteen_minute.has_value();


    // ========================================================================
    // 7. Current 1-minute candle features
    // ========================================================================

    if (snapshot.one_minute)
    {
        output.one_minute_candle_return =
            candleReturn(
                *snapshot.one_minute
            );


        output.candle_body_percent =
            candleBodyPercent(
                *snapshot.one_minute
            );


        output.candle_range_percent =
            candleRangePercent(
                *snapshot.one_minute
            );


        output.close_location =
            closeLocation(
                *snapshot.one_minute
            );
    }


    // ========================================================================
    // 8. Current 5-minute candle return
    // ========================================================================

    if (snapshot.five_minute)
    {
        output.five_minute_candle_return =
            candleReturn(
                *snapshot.five_minute
            );
    }


    // ========================================================================
    // 9. Current 15-minute candle return
    // ========================================================================

    if (snapshot.fifteen_minute)
    {
        output.fifteen_minute_candle_return =
            candleReturn(
                *snapshot.fifteen_minute
            );
    }


    // ========================================================================
    // 10. Close-to-close returns
    // ========================================================================

    output.one_minute_return =
        closeToCloseReturn(
            one_minute_history_
        );


    output.five_minute_return =
        closeToCloseReturn(
            five_minute_history_
        );


    output.fifteen_minute_return =
        closeToCloseReturn(
            fifteen_minute_history_
        );


    // ========================================================================
    // 11. Log returns
    // ========================================================================

    output.one_minute_log_return =
        logReturn(
            one_minute_history_
        );


    output.five_minute_log_return =
        logReturn(
            five_minute_history_
        );


    output.fifteen_minute_log_return =
        logReturn(
            fifteen_minute_history_
        );


    // ========================================================================
    // 12. Multi-bar returns
    //
    // Current Phase 3.1 design calculates these from 1-minute observations.
    // ========================================================================

    output.return_5_bars =
        nBarReturn(
            one_minute_history_,
            5
        );


    output.return_15_bars =
        nBarReturn(
            one_minute_history_,
            15
        );


    output.return_30_bars =
        nBarReturn(
            one_minute_history_,
            30
        );


    // ========================================================================
    // 13. Rolling-history readiness
    // ========================================================================

    output.has_5_bar_history =
        one_minute_history_.size() >= 6;


    output.has_15_bar_history =
        one_minute_history_.size() >= 16;


    output.has_30_bar_history =
        one_minute_history_.size() >= 31;


    return output;
}

} // namespace devai::features