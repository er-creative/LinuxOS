#pragma once
#include <cstdint>
#include <limits>
#include <string>
namespace devai::features {
inline double stockBenchmarkFeatureNaN() noexcept { return std::numeric_limits<double>::quiet_NaN(); }
struct RelativeDifference { double one_minute{stockBenchmarkFeatureNaN()}, five_minute{stockBenchmarkFeatureNaN()}, fifteen_minute{stockBenchmarkFeatureNaN()}; bool has_one_minute{false}, has_five_minute{false}, has_fifteen_minute{false}; };
struct RelativeRatio { double one_minute{stockBenchmarkFeatureNaN()}, five_minute{stockBenchmarkFeatureNaN()}, fifteen_minute{stockBenchmarkFeatureNaN()}; bool has_one_minute{false}, has_five_minute{false}, has_fifteen_minute{false}; };
struct RelativeEMARelationship { double one_minute{stockBenchmarkFeatureNaN()}, five_minute{stockBenchmarkFeatureNaN()}, fifteen_minute{stockBenchmarkFeatureNaN()}; bool has_one_minute{false}, has_five_minute{false}, has_fifteen_minute{false}; };
struct StockBenchmarkFeatures {
 std::string stock_symbol, benchmark_symbol; std::int64_t decision_time{0};
 RelativeDifference relative_return, relative_rsi14; RelativeEMARelationship relative_ema_relationship; RelativeDifference relative_ema20_slope, relative_ema50_slope;
 // Stock RVOL20 remains in Phase 3.3. No Stock/NIFTY RVOL20 ratio.
 RelativeRatio relative_atr14_percent, relative_realized_volatility_20; bool core_features_ready{false};
};
} // namespace devai::features
