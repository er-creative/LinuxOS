#pragma once

#include <cstdint>
#include <string>

namespace devai::market
{

struct Candle
{
    // -----------------------------------------------------
    // Identity
    // -----------------------------------------------------

    std::string symbol;

    // Unix timestamp in seconds.
    //
    // For historical data this represents the START
    // of the candle.
    //
    // Example:
    //
    // 09:15 timestamp
    //      1-minute candle = 09:15 -> 09:16
    //
    // The candle becomes usable by DevAI only after it
    // has completed.
    std::int64_t timestamp{0};

    // -----------------------------------------------------
    // OHLCV
    // -----------------------------------------------------

    double open{0.0};
    double high{0.0};
    double low{0.0};
    double close{0.0};

    std::uint64_t volume{0};

    // -----------------------------------------------------
    // Validation
    // -----------------------------------------------------

    [[nodiscard]]
    bool valid() const noexcept
    {
        if (symbol.empty())
            return false;

        if (timestamp <= 0)
            return false;

        if (open <= 0.0 ||
            high <= 0.0 ||
            low <= 0.0 ||
            close <= 0.0)
        {
            return false;
        }

        if (high < low)
            return false;

        if (high < open || high < close)
            return false;

        if (low > open || low > close)
            return false;

        return true;
    }
};

} // namespace devai::market