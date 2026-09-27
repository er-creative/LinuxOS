#pragma once

#include "devai/features/MultiTimeframeFeatures.hpp"
#include "devai/features/PriceReturnFeatures.hpp"
#include "devai/features/TrendMomentumFeatures.hpp"
#include "devai/features/VolumeFeatures.hpp"
#include "devai/features/VolatilityFeatures.hpp"

#include <cstdint>
#include <string>


namespace devai::features
{

class MultiTimeframeFeatureEngine
{
public:

    explicit MultiTimeframeFeatureEngine(
        std::string symbol
    );


    [[nodiscard]]
    MultiTimeframeFeatures update(
        const PriceReturnFeatures& price_return,
        const TrendMomentumFeatures& trend_momentum,
        const VolumeFeatures& volume,
        const VolatilityFeatures& volatility
    ) const;


    [[nodiscard]]
    const std::string& symbol() const noexcept
    {
        return symbol_;
    }


private:

    std::string symbol_;


    // ========================================================================
    // Direction helpers
    // ========================================================================

    [[nodiscard]]
    static FeatureDirection directionFromValue(
        double value
    ) noexcept;


    [[nodiscard]]
    static FeatureDirection directionFromPair(
        double first,
        double second
    ) noexcept;


    [[nodiscard]]
    static DirectionAlignment makeAlignment(
        FeatureDirection one,
        bool has_one,
        FeatureDirection five,
        bool has_five,
        FeatureDirection fifteen,
        bool has_fifteen
    ) noexcept;


    // ========================================================================
    // Ratio helpers
    // ========================================================================

    [[nodiscard]]
    static CrossTimeframeRatios makeRatios(
        double one,
        bool has_one,
        double five,
        bool has_five,
        double fifteen,
        bool has_fifteen
    ) noexcept;


    // ========================================================================
    // RSI helper
    // ========================================================================

    [[nodiscard]]
    static CrossTimeframeRSI makeRSIRelationship(
        double one,
        bool has_one,
        double five,
        bool has_five,
        double fifteen,
        bool has_fifteen
    ) noexcept;


    // ========================================================================
    // Input integrity
    // ========================================================================

    void validateInputs(
        const PriceReturnFeatures& price_return,
        const TrendMomentumFeatures& trend_momentum,
        const VolumeFeatures& volume,
        const VolatilityFeatures& volatility
    ) const;
};

} // namespace devai::features