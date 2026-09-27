#pragma once

#include "devai/market/Candle.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace devai::market
{

// =========================================================
// Availability Mode
// =========================================================

enum class AvailabilityMode
{
    ZERO_LATENCY,
    SIMULATED_LATENCY,
    LIVE_RECEIPT
};


// =========================================================
// Timed Candle
//
// market timestamp:
//     candle.timestamp
//
// completed_at:
//     theoretical candle completion time
//
// received_at:
//     actual/simulated time the candle became available
// =========================================================

struct TimedCandle
{
    Candle candle;

    std::int64_t completed_at{0};
    std::int64_t received_at{0};

    [[nodiscard]]
    bool availableAt(
        std::int64_t decision_time) const noexcept
    {
        return decision_time >= received_at;
    }
};


// =========================================================
// State Visible To Strategy At One Decision Time
// =========================================================

struct MultiTimeframeState
{
    std::int64_t decision_time{0};

    std::optional<TimedCandle> one_minute;
    std::optional<TimedCandle> five_minute;
    std::optional<TimedCandle> fifteen_minute;

    [[nodiscard]]
    bool hasOneMinute() const noexcept
    {
        return one_minute.has_value();
    }

    [[nodiscard]]
    bool hasFiveMinute() const noexcept
    {
        return five_minute.has_value();
    }

    [[nodiscard]]
    bool hasFifteenMinute() const noexcept
    {
        return fifteen_minute.has_value();
    }

    [[nodiscard]]
    bool fullySynchronized() const noexcept
    {
        return
            one_minute.has_value() &&
            five_minute.has_value() &&
            fifteen_minute.has_value();
    }
};


// =========================================================
// Synchronizer Configuration
// =========================================================

struct MultiTimeframeSynchronizerConfig
{
    AvailabilityMode mode{
        AvailabilityMode::ZERO_LATENCY
    };

    // Simulated latency in seconds.
    //
    // These values are used only in SIMULATED_LATENCY mode.

    std::int64_t one_minute_latency_seconds{0};
    std::int64_t five_minute_latency_seconds{0};
    std::int64_t fifteen_minute_latency_seconds{0};
};


// =========================================================
// MultiTimeframeSynchronizer
// =========================================================

class MultiTimeframeSynchronizer
{
public:

    explicit MultiTimeframeSynchronizer(
        MultiTimeframeSynchronizerConfig config = {}
    );


    // -----------------------------------------------------
    // Convert ordinary historical candles into TimedCandles.
    //
    // Availability is calculated according to:
    //
    // ZERO_LATENCY:
    //   timestamp + timeframe duration
    //
    // SIMULATED_LATENCY:
    //   timestamp + timeframe duration + latency
    //
    // LIVE_RECEIPT should NOT use these functions because
    // actual receipt timestamps must be supplied.
    // -----------------------------------------------------

    [[nodiscard]]
    std::vector<TimedCandle>
    prepareOneMinute(
        const std::vector<Candle>& candles
    ) const;

    [[nodiscard]]
    std::vector<TimedCandle>
    prepareFiveMinute(
        const std::vector<Candle>& candles
    ) const;

    [[nodiscard]]
    std::vector<TimedCandle>
    prepareFifteenMinute(
        const std::vector<Candle>& candles
    ) const;


    // -----------------------------------------------------
    // Create live TimedCandle using actual receipt time.
    // -----------------------------------------------------

    [[nodiscard]]
    TimedCandle makeLiveTimedCandle(
        const Candle& candle,
        std::int64_t timeframe_seconds,
        std::int64_t received_at
    ) const;


    // -----------------------------------------------------
    // Return the latest candle from each timeframe that was
    // genuinely available at decision_time.
    // -----------------------------------------------------

    [[nodiscard]]
    MultiTimeframeState stateAt(
        std::int64_t decision_time,
        const std::vector<TimedCandle>& one_minute,
        const std::vector<TimedCandle>& five_minute,
        const std::vector<TimedCandle>& fifteen_minute
    ) const;


    [[nodiscard]]
    const MultiTimeframeSynchronizerConfig&
    config() const noexcept
    {
        return config_;
    }


private:

    MultiTimeframeSynchronizerConfig config_;


    [[nodiscard]]
    std::vector<TimedCandle>
    prepareHistorical(
        const std::vector<Candle>& candles,
        std::int64_t timeframe_seconds,
        std::int64_t latency_seconds
    ) const;


    [[nodiscard]]
    static std::optional<TimedCandle>
    latestAvailable(
        const std::vector<TimedCandle>& candles,
        std::int64_t decision_time
    );
};

} // namespace devai::market