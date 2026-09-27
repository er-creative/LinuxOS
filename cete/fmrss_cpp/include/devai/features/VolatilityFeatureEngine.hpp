#pragma once

#include "devai/features/VolatilityFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>


namespace devai::features
{

class VolatilityFeatureEngine
{
public:

    explicit VolatilityFeatureEngine(
        std::string symbol
    );


    [[nodiscard]]
    VolatilityFeatures update(
        const devai::market::MarketSnapshot& snapshot
    );


    void reset() noexcept;


    [[nodiscard]]
    const std::string& symbol() const noexcept
    {
        return symbol_;
    }


private:

    // ========================================================================
    // Per-timeframe streaming state
    //
    // IMPORTANT:
    //
    // This state is continuous across trading sessions.
    //
    // A new trading session does NOT reset:
    //
    //     previous close
    //     ATR14
    //     rolling simple returns
    //     rolling log returns
    //
    // This allows overnight gaps to participate naturally in True Range
    // and close-to-close volatility.
    // ========================================================================

    struct TimeframeState
    {
        // Last candle consumed by this timeframe.
        //
        // Used for duplicate and backwards-timestamp protection.

        std::optional<std::int64_t>
            last_timestamp;


        // Close of the most recently consumed candle.
        //
        // Used to calculate:
        //
        //     True Range
        //     simple return
        //     log return

        std::optional<double>
            previous_close;


        // --------------------------------------------------------------------
        // Latest True Range
        //
        // These values are deliberately stored in state.
        //
        // Example:
        //
        //     09:20 -> new 5m candle -> calculate TR
        //     09:21 -> same 5m candle -> preserve same TR
        //     09:22 -> same 5m candle -> preserve same TR
        //     ...
        //     09:25 -> new 5m candle -> calculate new TR
        //
        // Without these fields, repeated 5m/15m snapshots would incorrectly
        // return NaN for True Range between completed candles.
        // --------------------------------------------------------------------

        std::optional<double>
            last_true_range;


        std::optional<double>
            last_true_range_percent;


        // --------------------------------------------------------------------
        // ATR14 seed state
        //
        // The first ATR14 is the arithmetic mean of the first 14 True Range
        // observations. After that, Wilder smoothing is used.
        // --------------------------------------------------------------------

        std::deque<double>
            initial_true_ranges;


        std::optional<double>
            atr14;


        // --------------------------------------------------------------------
        // Rolling return histories
        //
        // Maximum required window is 50 observations.
        // --------------------------------------------------------------------

        std::deque<double>
            simple_returns;


        std::deque<double>
            log_returns;


        void reset() noexcept;
    };


    // ========================================================================
    // Internal result of consuming one timeframe
    // ========================================================================

    struct TimeframeUpdateResult
    {
        bool has_candle{false};

        bool new_candle{false};

        double true_range{
            volatilityFeatureNaN()
        };

        double true_range_percent{
            volatilityFeatureNaN()
        };
    };


    // ========================================================================
    // Engine state
    // ========================================================================

    std::string symbol_;


    std::optional<std::int64_t>
        last_decision_time_;


    TimeframeState
        one_minute_state_;


    TimeframeState
        five_minute_state_;


    TimeframeState
        fifteen_minute_state_;


    // ========================================================================
    // Helpers
    // ========================================================================

    static TimeframeUpdateResult updateTimeframe(
        TimeframeState& state,
        const std::optional<devai::market::Candle>& candle
    );


    [[nodiscard]]
    static double calculateTrueRange(
        const devai::market::Candle& candle,
        const std::optional<double>& previous_close
    );


    [[nodiscard]]
    static double calculateStandardDeviation(
        const std::deque<double>& values,
        std::size_t period
    );


    static void appendBounded(
        std::deque<double>& history,
        double value,
        std::size_t maximum_size
    );


    [[nodiscard]]
    static TimeframeVolatilityFeatures makeFeatures(
        const TimeframeState& state,
        const std::optional<devai::market::Candle>& candle,
        const TimeframeUpdateResult& update_result
    );
};

} // namespace devai::features