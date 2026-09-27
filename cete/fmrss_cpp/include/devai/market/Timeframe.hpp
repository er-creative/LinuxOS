#pragma once

#include <cstdint>

namespace devai::market
{

enum class Timeframe : std::int32_t
{
    ONE_MINUTE     = 1,
    FIVE_MINUTES   = 5,
    FIFTEEN_MINUTES = 15
};

[[nodiscard]]
constexpr std::int32_t minutes(Timeframe timeframe) noexcept
{
    return static_cast<std::int32_t>(timeframe);
}

[[nodiscard]]
constexpr std::int64_t seconds(Timeframe timeframe) noexcept
{
    return static_cast<std::int64_t>(minutes(timeframe)) * 60;
}

} // namespace devai::market