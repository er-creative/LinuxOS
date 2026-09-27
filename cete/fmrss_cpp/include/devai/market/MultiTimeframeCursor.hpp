#pragma once

#include "devai/market/MultiTimeframeSynchronizer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace devai::market
{

class MultiTimeframeCursor
{
public:
    MultiTimeframeCursor(
        const std::vector<TimedCandle>& one_minute,
        const std::vector<TimedCandle>& five_minute,
        const std::vector<TimedCandle>& fifteen_minute
    );

    // Move the cursor forward to decision_time.
    //
    // The cursor is intentionally forward-only.
    // Calling advanceTo() with an earlier timestamp is an error.
    void advanceTo(std::int64_t decision_time);

    [[nodiscard]]
    const MultiTimeframeState& state() const noexcept
    {
        return state_;
    }

    [[nodiscard]]
    std::int64_t decisionTime() const noexcept
    {
        return state_.decision_time;
    }

    [[nodiscard]]
    bool fullySynchronized() const noexcept
    {
        return state_.fullySynchronized();
    }

    [[nodiscard]]
    std::size_t oneMinuteIndex() const noexcept
    {
        return one_index_;
    }

    [[nodiscard]]
    std::size_t fiveMinuteIndex() const noexcept
    {
        return five_index_;
    }

    [[nodiscard]]
    std::size_t fifteenMinuteIndex() const noexcept
    {
        return fifteen_index_;
    }

    void reset() noexcept;

private:
    const std::vector<TimedCandle>& one_minute_;
    const std::vector<TimedCandle>& five_minute_;
    const std::vector<TimedCandle>& fifteen_minute_;

    std::size_t one_index_{0};
    std::size_t five_index_{0};
    std::size_t fifteen_index_{0};

    MultiTimeframeState state_{};

    bool initialized_{false};

    static void advanceStream(
        const std::vector<TimedCandle>& stream,
        std::size_t& index,
        std::int64_t decision_time,
        std::optional<TimedCandle>& latest
    );

    static void validateStream(
        const std::vector<TimedCandle>& stream,
        const char* stream_name
    );
};

}