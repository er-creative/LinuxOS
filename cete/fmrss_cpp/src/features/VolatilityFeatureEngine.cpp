#include "devai/features/VolatilityFeatureEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>


namespace devai::features
{

namespace
{

constexpr std::size_t ATR_PERIOD =
    14;


constexpr std::size_t STDDEV_20_PERIOD =
    20;


constexpr std::size_t STDDEV_50_PERIOD =
    50;


constexpr std::size_t RETURN_HISTORY_LIMIT =
    50;


// ============================================================================
// NaN helper
// ============================================================================

double nanValue() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


// ============================================================================
// Price validation
// ============================================================================

bool validPrice(
    double value) noexcept
{
    return
        std::isfinite(value) &&
        value > 0.0;
}

} // namespace


// ============================================================================
// TimeframeState reset
//
// This is used only by an EXPLICIT engine reset.
//
// It is NOT automatically called when the trading session changes.
// ============================================================================

void VolatilityFeatureEngine::TimeframeState::reset() noexcept
{
    last_timestamp.reset();

    previous_close.reset();

    last_true_range.reset();

    last_true_range_percent.reset();

    initial_true_ranges.clear();

    atr14.reset();

    simple_returns.clear();

    log_returns.clear();
}


// ============================================================================
// Constructor
// ============================================================================

VolatilityFeatureEngine::VolatilityFeatureEngine(
    std::string symbol)
    :
    symbol_(
        std::move(symbol)
    )
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "VolatilityFeatureEngine symbol cannot be empty."
        );
    }
}


// ============================================================================
// Explicit engine reset
//
// IMPORTANT:
//
// Trading-session boundaries DO NOT call this function.
//
// ATR and rolling volatility histories are continuous across sessions.
// ============================================================================

void VolatilityFeatureEngine::reset() noexcept
{
    one_minute_state_.reset();

    five_minute_state_.reset();

    fifteen_minute_state_.reset();

    last_decision_time_.reset();
}


// ============================================================================
// Append bounded history
// ============================================================================

void VolatilityFeatureEngine::appendBounded(
    std::deque<double>& history,
    double value,
    std::size_t maximum_size)
{
    history.push_back(
        value
    );


    while (
        history.size() >
        maximum_size
    )
    {
        history.pop_front();
    }
}


// ============================================================================
// True Range
//
// TR = max(
//          High - Low,
//          |High - PreviousClose|,
//          |Low  - PreviousClose|
//      )
//
// For the first observation there is no previous close, therefore:
//
//     TR = High - Low
// ============================================================================

double VolatilityFeatureEngine::calculateTrueRange(
    const devai::market::Candle& candle,
    const std::optional<double>& previous_close)
{
    const double high_low =
        candle.high -
        candle.low;


    if (!previous_close)
    {
        return high_low;
    }


    const double high_previous_close =
        std::fabs(
            candle.high -
            *previous_close
        );


    const double low_previous_close =
        std::fabs(
            candle.low -
            *previous_close
        );


    return std::max(
        {
            high_low,
            high_previous_close,
            low_previous_close
        }
    );
}


// ============================================================================
// Rolling population standard deviation
//
// variance = sum((x - mean)^2) / N
//
// stddev = sqrt(variance)
//
// CETE Phase 3.4 uses population standard deviation for the realized rolling
// observations.
// ============================================================================

double VolatilityFeatureEngine::calculateStandardDeviation(
    const std::deque<double>& values,
    std::size_t period)
{
    if (
        period == 0 ||
        values.size() < period
    )
    {
        return nanValue();
    }


    const std::size_t start =
        values.size() -
        period;


    double sum =
        0.0;


    for (
        std::size_t index = start;
        index < values.size();
        ++index
    )
    {
        sum +=
            values[index];
    }


    const double mean =
        sum /
        static_cast<double>(
            period
        );


    double squared_sum =
        0.0;


    for (
        std::size_t index = start;
        index < values.size();
        ++index
    )
    {
        const double difference =
            values[index] -
            mean;


        squared_sum +=
            difference *
            difference;
    }


    double variance =
        squared_sum /
        static_cast<double>(
            period
        );


    // Protect against tiny negative values caused only by floating-point
    // rounding.

    if (
        variance < 0.0 &&
        variance > -1e-15
    )
    {
        variance =
            0.0;
    }


    if (
        variance < 0.0 ||
        !std::isfinite(variance)
    )
    {
        return nanValue();
    }


    return std::sqrt(
        variance
    );
}


// ============================================================================
// Update one timeframe
//
// A timeframe is updated ONLY when a new candle timestamp arrives.
//
// Repeated snapshots do not:
//
//     update ATR
//     append returns
//     append log returns
//     change previous close
//
// They simply preserve the latest state.
// ============================================================================

VolatilityFeatureEngine::TimeframeUpdateResult
VolatilityFeatureEngine::updateTimeframe(
    TimeframeState& state,
    const std::optional<devai::market::Candle>& candle)
{
    TimeframeUpdateResult result;


    if (!candle)
    {
        return result;
    }


    result.has_candle =
        true;


    // ========================================================================
    // Candle validation
    // ========================================================================

    if (!candle->valid())
    {
        throw std::runtime_error(
            "VolatilityFeatureEngine received invalid candle."
        );
    }


    if (
        !validPrice(candle->open) ||
        !validPrice(candle->high) ||
        !validPrice(candle->low) ||
        !validPrice(candle->close)
    )
    {
        throw std::runtime_error(
            "VolatilityFeatureEngine received invalid candle price."
        );
    }


    // ========================================================================
    // Duplicate / backwards protection
    // ========================================================================

    if (state.last_timestamp)
    {
        // Same candle already consumed.
        //
        // Do NOT update any rolling state.

        if (
            candle->timestamp ==
            *state.last_timestamp
        )
        {
            return result;
        }


        // A previously consumed timeframe may never move backwards.

        if (
            candle->timestamp <
            *state.last_timestamp
        )
        {
            throw std::runtime_error(
                "VolatilityFeatureEngine candle timestamp moved backwards."
            );
        }
    }


    result.new_candle =
        true;


    // ========================================================================
    // True Range
    //
    // previous_close intentionally survives session boundaries.
    //
    // Therefore an overnight price gap participates naturally in True Range.
    // ========================================================================

    const double true_range =
        calculateTrueRange(
            *candle,
            state.previous_close
        );


    if (
        !std::isfinite(true_range) ||
        true_range < 0.0
    )
    {
        throw std::runtime_error(
            "VolatilityFeatureEngine calculated invalid True Range."
        );
    }


    result.true_range =
        true_range;


    result.true_range_percent =
        true_range /
        candle->close;


    if (
        !std::isfinite(
            result.true_range_percent
        ) ||
        result.true_range_percent < 0.0
    )
    {
        throw std::runtime_error(
            "VolatilityFeatureEngine calculated invalid True Range percent."
        );
    }


    // ========================================================================
    // Store latest TR permanently in timeframe state.
    //
    // This is what allows repeated 5m / 15m snapshots to keep exposing the
    // latest completed candle's True Range.
    // ========================================================================

    state.last_true_range =
        result.true_range;


    state.last_true_range_percent =
        result.true_range_percent;


    // ========================================================================
    // ATR14
    //
    // Initial seed:
    //
    //     SMA(first 14 True Range observations)
    //
    // Thereafter Wilder:
    //
    //     ATR(t) =
    //
    //         ATR(t-1) * 13 + TR(t)
    //         ---------------------
    //                  14
    // ========================================================================

    if (!state.atr14)
    {
        state.initial_true_ranges.push_back(
            true_range
        );


        if (
            state.initial_true_ranges.size() ==
            ATR_PERIOD
        )
        {
            double sum =
                0.0;


            for (
                const double value :
                state.initial_true_ranges
            )
            {
                sum +=
                    value;
            }


            state.atr14 =
                sum /
                static_cast<double>(
                    ATR_PERIOD
                );


            // Wilder ATR is now seeded.
            //
            // We no longer need the original seed observations.

            state.initial_true_ranges.clear();
        }
    }
    else
    {
        state.atr14 =
            (
                (
                    *state.atr14 *
                    static_cast<double>(
                        ATR_PERIOD - 1
                    )
                ) +
                true_range
            ) /
            static_cast<double>(
                ATR_PERIOD
            );
    }


    // ========================================================================
    // Close-to-close returns
    //
    // The first candle has no previous close, therefore no return observation.
    //
    // previous_close survives trading-session boundaries, so the first return
    // of a new session naturally includes the overnight gap.
    // ========================================================================

    if (state.previous_close)
    {
        if (
            !validPrice(
                *state.previous_close
            )
        )
        {
            throw std::runtime_error(
                "VolatilityFeatureEngine previous close is invalid."
            );
        }


        const double simple_return =
            (
                candle->close /
                *state.previous_close
            ) -
            1.0;


        const double log_return =
            std::log(
                candle->close /
                *state.previous_close
            );


        if (
            !std::isfinite(simple_return) ||
            !std::isfinite(log_return)
        )
        {
            throw std::runtime_error(
                "VolatilityFeatureEngine calculated invalid return."
            );
        }


        appendBounded(
            state.simple_returns,
            simple_return,
            RETURN_HISTORY_LIMIT
        );


        appendBounded(
            state.log_returns,
            log_return,
            RETURN_HISTORY_LIMIT
        );
    }


    // ========================================================================
    // Commit current candle as previous candle for next update
    // ========================================================================

    state.previous_close =
        candle->close;


    state.last_timestamp =
        candle->timestamp;


    return result;
}


// ============================================================================
// Create output for one timeframe
// ============================================================================

TimeframeVolatilityFeatures
VolatilityFeatureEngine::makeFeatures(
    const TimeframeState& state,
    const std::optional<devai::market::Candle>& candle,
    const TimeframeUpdateResult& update_result)
{
    TimeframeVolatilityFeatures output;


    if (!candle)
    {
        return output;
    }


    // The update result is intentionally accepted because it records whether
    // the current snapshot introduced a genuinely new timeframe candle.
    //
    // Feature output itself comes from the persistent timeframe state so that
    // repeated snapshots expose the same latest completed values.

    static_cast<void>(
        update_result
    );


    // ========================================================================
    // True Range
    //
    // Stored state means repeated snapshots preserve the latest TR.
    // ========================================================================

    if (
        state.last_true_range &&
        std::isfinite(
            *state.last_true_range
        )
    )
    {
        output.true_range =
            *state.last_true_range;


        output.has_true_range =
            true;
    }


    if (
        state.last_true_range_percent &&
        std::isfinite(
            *state.last_true_range_percent
        )
    )
    {
        output.true_range_percent =
            *state.last_true_range_percent;


        output.has_true_range_percent =
            true;
    }


    // ========================================================================
    // ATR14
    // ========================================================================

    if (
        state.atr14 &&
        std::isfinite(
            *state.atr14
        )
    )
    {
        output.atr14 =
            *state.atr14;


        output.has_atr14 =
            true;


        if (
            validPrice(
                candle->close
            )
        )
        {
            output.atr14_percent =
                *state.atr14 /
                candle->close;


            if (
                std::isfinite(
                    output.atr14_percent
                ) &&
                output.atr14_percent >= 0.0
            )
            {
                output.has_atr14_percent =
                    true;
            }
        }
    }


    // ========================================================================
    // Return standard deviation 20
    // ========================================================================

    const double stddev20 =
        calculateStandardDeviation(
            state.simple_returns,
            STDDEV_20_PERIOD
        );


    if (
        std::isfinite(
            stddev20
        )
    )
    {
        output.return_stddev_20 =
            stddev20;


        output.has_return_stddev_20 =
            true;
    }


    // ========================================================================
    // Return standard deviation 50
    // ========================================================================

    const double stddev50 =
        calculateStandardDeviation(
            state.simple_returns,
            STDDEV_50_PERIOD
        );


    if (
        std::isfinite(
            stddev50
        )
    )
    {
        output.return_stddev_50 =
            stddev50;


        output.has_return_stddev_50 =
            true;
    }


    // ========================================================================
    // Realized Volatility 20
    //
    // Population standard deviation of the latest 20 log returns.
    //
    // Not annualized.
    // ========================================================================

    const double realized20 =
        calculateStandardDeviation(
            state.log_returns,
            STDDEV_20_PERIOD
        );


    if (
        std::isfinite(
            realized20
        )
    )
    {
        output.realized_volatility_20 =
            realized20;


        output.has_realized_volatility_20 =
            true;
    }


    // ========================================================================
    // Realized Volatility 50
    // ========================================================================

    const double realized50 =
        calculateStandardDeviation(
            state.log_returns,
            STDDEV_50_PERIOD
        );


    if (
        std::isfinite(
            realized50
        )
    )
    {
        output.realized_volatility_50 =
            realized50;


        output.has_realized_volatility_50 =
            true;
    }


    return output;
}


// ============================================================================
// Update complete MarketSnapshot
// ============================================================================

VolatilityFeatures VolatilityFeatureEngine::update(
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
            "VolatilityFeatureEngine snapshot symbol does not match engine."
        );
    }


    // ========================================================================
    // Decision-time protection
    //
    // Equal decision time is allowed because tests may intentionally feed the
    // exact same snapshot twice.
    // ========================================================================

    if (
        last_decision_time_ &&
        snapshot.decision_time <
            *last_decision_time_
    )
    {
        throw std::runtime_error(
            "VolatilityFeatureEngine decision time moved backwards."
        );
    }


    // ========================================================================
    // IMPORTANT CETE POLICY
    //
    // DO NOT:
    //
    //     if (snapshot.new_session)
    //         reset();
    //
    // Volatility history is continuous across sessions.
    // ========================================================================

    const TimeframeUpdateResult one_update =
        updateTimeframe(
            one_minute_state_,
            snapshot.one_minute
        );


    const TimeframeUpdateResult five_update =
        updateTimeframe(
            five_minute_state_,
            snapshot.five_minute
        );


    const TimeframeUpdateResult fifteen_update =
        updateTimeframe(
            fifteen_minute_state_,
            snapshot.fifteen_minute
        );


    last_decision_time_ =
        snapshot.decision_time;


    // ========================================================================
    // Build output
    // ========================================================================

    VolatilityFeatures output;


    output.symbol =
        symbol_;


    output.decision_time =
        snapshot.decision_time;


    output.one_minute =
        makeFeatures(
            one_minute_state_,
            snapshot.one_minute,
            one_update
        );


    output.five_minute =
        makeFeatures(
            five_minute_state_,
            snapshot.five_minute,
            five_update
        );


    output.fifteen_minute =
        makeFeatures(
            fifteen_minute_state_,
            snapshot.fifteen_minute,
            fifteen_update
        );


    return output;
}

} // namespace devai::features