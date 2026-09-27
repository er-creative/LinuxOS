#include "devai/features/TrendMomentumFeatureEngine.hpp"

#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace devai::features
{

namespace
{

constexpr std::size_t EMA20_PERIOD = 20;
constexpr std::size_t EMA50_PERIOD = 50;
constexpr std::size_t RSI14_PERIOD = 14;


constexpr double EMA20_ALPHA =
    2.0 /
    (
        static_cast<double>(EMA20_PERIOD) +
        1.0
    );


constexpr double EMA50_ALPHA =
    2.0 /
    (
        static_cast<double>(EMA50_PERIOD) +
        1.0
    );


double nanValue() noexcept
{
    return
        std::numeric_limits<double>::quiet_NaN();
}


bool validPositive(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value > 0.0;
}

} // namespace


// ============================================================================
// TimeframeState reset
// ============================================================================

void
TrendMomentumFeatureEngine::TimeframeState::reset() noexcept
{
    last_timestamp.reset();

    candle_count =
        0;


    ema20_seed_closes.clear();
    ema50_seed_closes.clear();


    ema20.reset();
    ema50.reset();

    previous_ema20.reset();
    previous_ema50.reset();


    previous_close.reset();


    rsi_seed_changes =
        0;


    rsi_seed_gain_sum =
        0.0;

    rsi_seed_loss_sum =
        0.0;


    average_gain.reset();
    average_loss.reset();

    rsi14.reset();
}


// ============================================================================
// Constructor
// ============================================================================

TrendMomentumFeatureEngine::TrendMomentumFeatureEngine(
    std::string symbol)
    :
    symbol_(std::move(symbol))
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "TrendMomentumFeatureEngine symbol cannot be empty."
        );
    }
}


// ============================================================================
// Explicit reset
//
// IMPORTANT CETE RULE:
//
// reset() means explicit engine reinitialization.
//
// It does NOT mean:
//
//     new trading session
//
// EMA20 / EMA50 / RSI14 are continuous mathematical state and therefore
// survive trading-session boundaries.
// ============================================================================

void
TrendMomentumFeatureEngine::reset() noexcept
{
    one_minute_state_.reset();

    five_minute_state_.reset();

    fifteen_minute_state_.reset();


    last_decision_time_.reset();
}


// ============================================================================
// Simple average
// ============================================================================

double
TrendMomentumFeatureEngine::simpleAverage(
    const std::deque<double>& values)
{
    if (values.empty())
    {
        return nanValue();
    }


    const double sum =
        std::accumulate(
            values.begin(),
            values.end(),
            0.0
        );


    return
        sum /
        static_cast<double>(
            values.size()
        );
}


// ============================================================================
// Normalized distance
//
//     value / reference - 1
// ============================================================================

double
TrendMomentumFeatureEngine::normalizedDistance(
    double value,
    double reference)
{
    if (
        !std::isfinite(value) ||
        !validPositive(reference)
    )
    {
        return nanValue();
    }


    return
        (
            value /
            reference
        ) -
        1.0;
}


// ============================================================================
// Normalized slope
//
//     current / previous - 1
// ============================================================================

double
TrendMomentumFeatureEngine::normalizedSlope(
    double current,
    double previous)
{
    if (
        !std::isfinite(current) ||
        !validPositive(previous)
    )
    {
        return nanValue();
    }


    return
        (
            current /
            previous
        ) -
        1.0;
}


// ============================================================================
// RSI
// ============================================================================

double
TrendMomentumFeatureEngine::calculateRSI(
    double average_gain,
    double average_loss)
{
    if (
        !std::isfinite(average_gain) ||
        !std::isfinite(average_loss) ||
        average_gain < 0.0 ||
        average_loss < 0.0
    )
    {
        return nanValue();
    }


    // Completely flat market.

    if (
        average_gain == 0.0 &&
        average_loss == 0.0
    )
    {
        return 50.0;
    }


    // No losses.

    if (average_loss == 0.0)
    {
        return 100.0;
    }


    // No gains.

    if (average_gain == 0.0)
    {
        return 0.0;
    }


    const double relative_strength =
        average_gain /
        average_loss;


    return
        100.0 -
        (
            100.0 /
            (
                1.0 +
                relative_strength
            )
        );
}


// ============================================================================
// EMA update
//
// EMA20:
//     First 20 completed candles -> SMA seed.
//     Subsequent candles         -> recursive EMA.
//
// EMA50:
//     First 50 completed candles -> SMA seed.
//     Subsequent candles         -> recursive EMA.
//
// State persists across trading sessions.
// ============================================================================

void
TrendMomentumFeatureEngine::updateEMA(
    TimeframeState& state,
    double close)
{
    // ========================================================================
    // EMA20
    // ========================================================================

    if (!state.ema20)
    {
        state.ema20_seed_closes.push_back(
            close
        );


        if (
            state.ema20_seed_closes.size() ==
            EMA20_PERIOD
        )
        {
            state.ema20 =
                simpleAverage(
                    state.ema20_seed_closes
                );


            state.ema20_seed_closes.clear();
        }
    }
    else
    {
        state.previous_ema20 =
            state.ema20;


        state.ema20 =
            (
                EMA20_ALPHA *
                close
            ) +
            (
                (
                    1.0 -
                    EMA20_ALPHA
                ) *
                *state.ema20
            );
    }


    // ========================================================================
    // EMA50
    // ========================================================================

    if (!state.ema50)
    {
        state.ema50_seed_closes.push_back(
            close
        );


        if (
            state.ema50_seed_closes.size() ==
            EMA50_PERIOD
        )
        {
            state.ema50 =
                simpleAverage(
                    state.ema50_seed_closes
                );


            state.ema50_seed_closes.clear();
        }
    }
    else
    {
        state.previous_ema50 =
            state.ema50;


        state.ema50 =
            (
                EMA50_ALPHA *
                close
            ) +
            (
                (
                    1.0 -
                    EMA50_ALPHA
                ) *
                *state.ema50
            );
    }
}


// ============================================================================
// RSI14 update
//
// Wilder RSI.
//
// First close:
//
//     establishes previous_close
//
// Next 14 changes:
//
//     seed average gain/loss
//
// Thereafter:
//
//     Wilder smoothing:
//
//     avg_gain = ((previous_avg_gain * 13) + gain) / 14
//
//     avg_loss = ((previous_avg_loss * 13) + loss) / 14
//
// IMPORTANT:
//
// previous_close is intentionally preserved across trading sessions.
//
// Therefore the first real candle of a new session compares against the final
// real completed candle from the previous session.
// ============================================================================

void
TrendMomentumFeatureEngine::updateRSI(
    TimeframeState& state,
    double close)
{
    // First observation.

    if (!state.previous_close)
    {
        state.previous_close =
            close;

        return;
    }


    const double change =
        close -
        *state.previous_close;


    const double gain =
        change > 0.0
            ? change
            : 0.0;


    const double loss =
        change < 0.0
            ? -change
            : 0.0;


    // ========================================================================
    // RSI seed period
    // ========================================================================

    if (
        !state.average_gain ||
        !state.average_loss
    )
    {
        state.rsi_seed_gain_sum +=
            gain;


        state.rsi_seed_loss_sum +=
            loss;


        ++state.rsi_seed_changes;


        if (
            state.rsi_seed_changes ==
            RSI14_PERIOD
        )
        {
            state.average_gain =
                state.rsi_seed_gain_sum /
                static_cast<double>(
                    RSI14_PERIOD
                );


            state.average_loss =
                state.rsi_seed_loss_sum /
                static_cast<double>(
                    RSI14_PERIOD
                );


            state.rsi14 =
                calculateRSI(
                    *state.average_gain,
                    *state.average_loss
                );
        }
    }

    // ========================================================================
    // Wilder recursive update
    // ========================================================================

    else
    {
        state.average_gain =
            (
                (
                    *state.average_gain *
                    static_cast<double>(
                        RSI14_PERIOD - 1
                    )
                ) +
                gain
            )
            /
            static_cast<double>(
                RSI14_PERIOD
            );


        state.average_loss =
            (
                (
                    *state.average_loss *
                    static_cast<double>(
                        RSI14_PERIOD - 1
                    )
                ) +
                loss
            )
            /
            static_cast<double>(
                RSI14_PERIOD
            );


        state.rsi14 =
            calculateRSI(
                *state.average_gain,
                *state.average_loss
            );
    }


    state.previous_close =
        close;
}


// ============================================================================
// Update one timeframe
//
// Returns:
//
//     true  -> genuinely new completed candle consumed
//     false -> no candle or repeated candle
//
// Repeated latest 5m / 15m snapshots must NOT update EMA or RSI twice.
// ============================================================================

bool
TrendMomentumFeatureEngine::updateTimeframe(
    TimeframeState& state,
    const std::optional<devai::market::Candle>& candle)
{
    if (!candle)
    {
        return false;
    }


    if (!candle->valid())
    {
        throw std::runtime_error(
            "TrendMomentumFeatureEngine received invalid candle."
        );
    }


    if (!validPositive(candle->close))
    {
        throw std::runtime_error(
            "TrendMomentumFeatureEngine received invalid candle close."
        );
    }


    // ========================================================================
    // Timestamp protection
    // ========================================================================

    if (state.last_timestamp)
    {
        // Repeated snapshot.

        if (
            candle->timestamp ==
            *state.last_timestamp
        )
        {
            return false;
        }


        // Timeframe stream regression.

        if (
            candle->timestamp <
            *state.last_timestamp
        )
        {
            throw std::runtime_error(
                "TrendMomentumFeatureEngine candle timestamp moved backwards."
            );
        }
    }


    // ========================================================================
    // Consume new candle
    // ========================================================================

    updateEMA(
        state,
        candle->close
    );


    updateRSI(
        state,
        candle->close
    );


    state.last_timestamp =
        candle->timestamp;


    ++state.candle_count;


    return true;
}


// ============================================================================
// Build one timeframe's output
// ============================================================================

TimeframeTrendMomentumFeatures
TrendMomentumFeatureEngine::makeFeatures(
    const TimeframeState& state,
    const std::optional<devai::market::Candle>& candle)
{
    TimeframeTrendMomentumFeatures output;


    // ========================================================================
    // EMA20
    // ========================================================================

    if (state.ema20)
    {
        output.ema20 =
            *state.ema20;


        output.has_ema20 =
            true;


        if (candle)
        {
            output.price_vs_ema20 =
                normalizedDistance(
                    candle->close,
                    *state.ema20
                );
        }


        if (state.previous_ema20)
        {
            output.ema20_slope =
                normalizedSlope(
                    *state.ema20,
                    *state.previous_ema20
                );


            output.has_ema20_slope =
                true;
        }
    }


    // ========================================================================
    // EMA50
    // ========================================================================

    if (state.ema50)
    {
        output.ema50 =
            *state.ema50;


        output.has_ema50 =
            true;


        if (candle)
        {
            output.price_vs_ema50 =
                normalizedDistance(
                    candle->close,
                    *state.ema50
                );
        }


        if (state.previous_ema50)
        {
            output.ema50_slope =
                normalizedSlope(
                    *state.ema50,
                    *state.previous_ema50
                );


            output.has_ema50_slope =
                true;
        }
    }


    // ========================================================================
    // EMA20 / EMA50 relationship
    // ========================================================================

    if (
        state.ema20 &&
        state.ema50
    )
    {
        output.ema20_vs_ema50 =
            normalizedDistance(
                *state.ema20,
                *state.ema50
            );
    }


    // ========================================================================
    // RSI14
    // ========================================================================

    if (state.rsi14)
    {
        output.rsi14 =
            *state.rsi14;


        output.has_rsi14 =
            true;
    }


    return output;
}


// ============================================================================
// Main update
// ============================================================================

TrendMomentumFeatures
TrendMomentumFeatureEngine::update(
    const devai::market::MarketSnapshot& snapshot)
{
    // ========================================================================
    // 1. Snapshot symbol protection
    // ========================================================================

    if (
        snapshot.symbol !=
        symbol_
    )
    {
        throw std::invalid_argument(
            "TrendMomentumFeatureEngine snapshot symbol does not match engine."
        );
    }


    // ========================================================================
    // 2. Decision-time protection
    //
    // Same decision time is legal.
    //
    // Decreasing decision time is forbidden.
    // ========================================================================

    if (
        last_decision_time_ &&
        snapshot.decision_time <
            *last_decision_time_
    )
    {
        throw std::runtime_error(
            "TrendMomentumFeatureEngine decision time moved backwards."
        );
    }


    // ========================================================================
    // 3. CETE CROSS-SESSION POLICY
    //
    // There is intentionally NO:
    //
    //     if (snapshot.new_session)
    //     {
    //         reset();
    //     }
    //
    // EMA20, EMA50 and RSI14 are continuous rolling mathematical state.
    //
    // They MUST survive trading-session boundaries.
    //
    // No synthetic overnight candles are inserted.
    // The first real candle of the new session is simply the next observation.
    // ========================================================================


    // ========================================================================
    // 4. Consume genuinely new completed candles
    // ========================================================================

    updateTimeframe(
        one_minute_state_,
        snapshot.one_minute
    );


    updateTimeframe(
        five_minute_state_,
        snapshot.five_minute
    );


    updateTimeframe(
        fifteen_minute_state_,
        snapshot.fifteen_minute
    );


    // Record decision time only after successful processing.

    last_decision_time_ =
        snapshot.decision_time;


    // ========================================================================
    // 5. Build output
    // ========================================================================

    TrendMomentumFeatures output;


    output.symbol =
        symbol_;


    output.decision_time =
        snapshot.decision_time;


    output.one_minute =
        makeFeatures(
            one_minute_state_,
            snapshot.one_minute
        );


    output.five_minute =
        makeFeatures(
            five_minute_state_,
            snapshot.five_minute
        );


    output.fifteen_minute =
        makeFeatures(
            fifteen_minute_state_,
            snapshot.fifteen_minute
        );


    return output;
}

} // namespace devai::features