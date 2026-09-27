#pragma once

#include "devai/features/TrendMomentumFeatures.hpp"

#include "devai/market/Candle.hpp"
#include "devai/market/MarketSnapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace devai::features
{

class TrendMomentumFeatureEngine
{
public:

    explicit TrendMomentumFeatureEngine(
        std::string symbol
    );


    [[nodiscard]]
    TrendMomentumFeatures update(
        const devai::market::MarketSnapshot& snapshot
    );


    // Explicit engine reinitialization only.
    //
    // IMPORTANT:
    //
    // This is NOT called automatically when a new trading session begins.
    //
    // EMA and RSI history must persist across sessions.

    void reset() noexcept;


    [[nodiscard]]
    const std::string& symbol() const noexcept
    {
        return symbol_;
    }


private:

    // ========================================================================
    // Per-timeframe continuous state
    // ========================================================================

    struct TimeframeState
    {
        std::optional<std::int64_t>
            last_timestamp;


        std::size_t candle_count{0};


        // --------------------------------------------------------------------
        // EMA seed state
        // --------------------------------------------------------------------

        std::deque<double>
            ema20_seed_closes;

        std::deque<double>
            ema50_seed_closes;


        std::optional<double>
            ema20;

        std::optional<double>
            ema50;


        // Previous EMA values are required for slope calculation.

        std::optional<double>
            previous_ema20;

        std::optional<double>
            previous_ema50;


        // --------------------------------------------------------------------
        // RSI14 state
        // --------------------------------------------------------------------

        std::optional<double>
            previous_close;


        std::size_t
            rsi_seed_changes{0};


        double
            rsi_seed_gain_sum{0.0};

        double
            rsi_seed_loss_sum{0.0};


        std::optional<double>
            average_gain;

        std::optional<double>
            average_loss;


        std::optional<double>
            rsi14;


        void reset() noexcept;
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
    // Internal processing
    // ========================================================================

    static bool updateTimeframe(
        TimeframeState& state,
        const std::optional<devai::market::Candle>& candle
    );


    static void updateEMA(
        TimeframeState& state,
        double close
    );


    static void updateRSI(
        TimeframeState& state,
        double close
    );


    [[nodiscard]]
    static TimeframeTrendMomentumFeatures makeFeatures(
        const TimeframeState& state,
        const std::optional<devai::market::Candle>& candle
    );


    [[nodiscard]]
    static double simpleAverage(
        const std::deque<double>& values
    );


    [[nodiscard]]
    static double normalizedDistance(
        double value,
        double reference
    );


    [[nodiscard]]
    static double normalizedSlope(
        double current,
        double previous
    );


    [[nodiscard]]
    static double calculateRSI(
        double average_gain,
        double average_loss
    );
};

} // namespace devai::features