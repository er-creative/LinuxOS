#pragma once
#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/features/StockBenchmarkFeatures.hpp"
#include "devai/features/TrendMomentumFeatures.hpp"
#include "devai/features/VolatilityFeatures.hpp"
#include <string>
namespace devai::features {
class StockBenchmarkFeatureEngine {
public:
 StockBenchmarkFeatureEngine(std::string stock_symbol,std::string benchmark_symbol);
 [[nodiscard]] StockBenchmarkFeatures update(const PriceReturnFeatures&,const TrendMomentumFeatures&,const VolatilityFeatures&,const PriceReturnFeatures&,const TrendMomentumFeatures&,const VolatilityFeatures&) const;
 [[nodiscard]] const std::string& stockSymbol() const noexcept { return stock_symbol_; }
 [[nodiscard]] const std::string& benchmarkSymbol() const noexcept { return benchmark_symbol_; }
private:
 std::string stock_symbol_, benchmark_symbol_;
 static double difference(double,double) noexcept; static double ratio(double,double) noexcept; static double normalizedEMASpread(double,double) noexcept;
 static RelativeDifference makeDifference(double,bool,double,bool,double,bool,double,bool,double,bool,double,bool) noexcept;
 static RelativeRatio makeRatio(double,bool,double,bool,double,bool,double,bool,double,bool,double,bool) noexcept;
 static RelativeEMARelationship makeEMARelationship(const TrendMomentumFeatures&,const TrendMomentumFeatures&) noexcept;
 void validateInputs(const PriceReturnFeatures&,const TrendMomentumFeatures&,const VolatilityFeatures&,const PriceReturnFeatures&,const TrendMomentumFeatures&,const VolatilityFeatures&) const;
};
} // namespace devai::features
