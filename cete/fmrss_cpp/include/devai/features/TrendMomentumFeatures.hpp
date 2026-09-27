#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace devai::features
{

inline double trendMomentumNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}


struct TimeframeTrendMomentumFeatures
{
    double ema20{trendMomentumNaN()};
    double ema50{trendMomentumNaN()};
    double rsi14{trendMomentumNaN()};

    double price_vs_ema20{trendMomentumNaN()};
    double price_vs_ema50{trendMomentumNaN()};
    double ema20_vs_ema50{trendMomentumNaN()};

    double ema20_slope{trendMomentumNaN()};
    double ema50_slope{trendMomentumNaN()};

    bool has_ema20{false};
    bool has_ema50{false};
    bool has_rsi14{false};

    bool has_ema20_slope{false};
    bool has_ema50_slope{false};
};


struct TrendMomentumFeatures
{
    std::string symbol;

    std::int64_t decision_time{0};

    TimeframeTrendMomentumFeatures one_minute;
    TimeframeTrendMomentumFeatures five_minute;
    TimeframeTrendMomentumFeatures fifteen_minute;
};

} // namespace devai::features