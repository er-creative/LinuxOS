#include "devai/features/MultiTimeframeFeatureEngine.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>


namespace devai::features
{

// ============================================================================
// Constructor
// ============================================================================

MultiTimeframeFeatureEngine::MultiTimeframeFeatureEngine(
    std::string symbol)
    :
    symbol_(
        std::move(symbol)
    )
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "MultiTimeframeFeatureEngine symbol cannot be empty."
        );
    }
}


// ============================================================================
// Direction from signed value
// ============================================================================

FeatureDirection
MultiTimeframeFeatureEngine::directionFromValue(
    double value) noexcept
{
    if (!std::isfinite(value))
    {
        return FeatureDirection::NEUTRAL;
    }


    if (value > 0.0)
    {
        return FeatureDirection::POSITIVE;
    }


    if (value < 0.0)
    {
        return FeatureDirection::NEGATIVE;
    }


    return FeatureDirection::NEUTRAL;
}


// ============================================================================
// Direction from relationship between two values
// ============================================================================

FeatureDirection
MultiTimeframeFeatureEngine::directionFromPair(
    double first,
    double second) noexcept
{
    if (
        !std::isfinite(first) ||
        !std::isfinite(second)
    )
    {
        return FeatureDirection::NEUTRAL;
    }


    if (first > second)
    {
        return FeatureDirection::POSITIVE;
    }


    if (first < second)
    {
        return FeatureDirection::NEGATIVE;
    }


    return FeatureDirection::NEUTRAL;
}


// ============================================================================
// Alignment
// ============================================================================

DirectionAlignment
MultiTimeframeFeatureEngine::makeAlignment(
    FeatureDirection one,
    bool has_one,
    FeatureDirection five,
    bool has_five,
    FeatureDirection fifteen,
    bool has_fifteen) noexcept
{
    DirectionAlignment output;


    output.one_minute =
        has_one
            ? one
            : FeatureDirection::NEUTRAL;


    output.five_minute =
        has_five
            ? five
            : FeatureDirection::NEUTRAL;


    output.fifteen_minute =
        has_fifteen
            ? fifteen
            : FeatureDirection::NEUTRAL;


    output.fully_available =
        has_one &&
        has_five &&
        has_fifteen;


    const auto count_direction =
        [&output](
            FeatureDirection direction,
            bool available)
        {
            if (!available)
            {
                return;
            }


            switch (direction)
            {
                case FeatureDirection::POSITIVE:
                {
                    ++output.positive_count;
                    break;
                }

                case FeatureDirection::NEGATIVE:
                {
                    ++output.negative_count;
                    break;
                }

                case FeatureDirection::NEUTRAL:
                {
                    ++output.neutral_count;
                    break;
                }
            }
        };


    count_direction(
        output.one_minute,
        has_one
    );


    count_direction(
        output.five_minute,
        has_five
    );


    count_direction(
        output.fifteen_minute,
        has_fifteen
    );


    output.all_positive =
        output.fully_available &&
        output.positive_count == 3;


    output.all_negative =
        output.fully_available &&
        output.negative_count == 3;


    // Mixed means there is at least one positive AND at least one negative
    // available timeframe.
    //
    // Neutral + positive alone is not classified as directional conflict.

    output.mixed =
        output.positive_count > 0 &&
        output.negative_count > 0;


    return output;
}


// ============================================================================
// Cross-timeframe ratios
// ============================================================================

CrossTimeframeRatios
MultiTimeframeFeatureEngine::makeRatios(
    double one,
    bool has_one,
    double five,
    bool has_five,
    double fifteen,
    bool has_fifteen) noexcept
{
    CrossTimeframeRatios output;


    if (
        has_one &&
        has_five &&
        std::isfinite(one) &&
        std::isfinite(five) &&
        one > 0.0
    )
    {
        output.five_to_one =
            five / one;


        output.has_five_to_one =
            std::isfinite(
                output.five_to_one
            );
    }


    if (
        has_five &&
        has_fifteen &&
        std::isfinite(five) &&
        std::isfinite(fifteen) &&
        five > 0.0
    )
    {
        output.fifteen_to_five =
            fifteen / five;


        output.has_fifteen_to_five =
            std::isfinite(
                output.fifteen_to_five
            );
    }


    if (
        has_one &&
        has_fifteen &&
        std::isfinite(one) &&
        std::isfinite(fifteen) &&
        one > 0.0
    )
    {
        output.fifteen_to_one =
            fifteen / one;


        output.has_fifteen_to_one =
            std::isfinite(
                output.fifteen_to_one
            );
    }


    return output;
}


// ============================================================================
// RSI relationships
// ============================================================================

CrossTimeframeRSI
MultiTimeframeFeatureEngine::makeRSIRelationship(
    double one,
    bool has_one,
    double five,
    bool has_five,
    double fifteen,
    bool has_fifteen) noexcept
{
    CrossTimeframeRSI output;


    if (
        has_one &&
        std::isfinite(one)
    )
    {
        output.one_minute =
            one;

        output.has_one_minute =
            true;
    }


    if (
        has_five &&
        std::isfinite(five)
    )
    {
        output.five_minute =
            five;

        output.has_five_minute =
            true;
    }


    if (
        has_fifteen &&
        std::isfinite(fifteen)
    )
    {
        output.fifteen_minute =
            fifteen;

        output.has_fifteen_minute =
            true;
    }


    if (
        output.has_one_minute &&
        output.has_five_minute
    )
    {
        output.five_minus_one =
            output.five_minute -
            output.one_minute;


        output.has_five_minus_one =
            true;
    }


    if (
        output.has_five_minute &&
        output.has_fifteen_minute
    )
    {
        output.fifteen_minus_five =
            output.fifteen_minute -
            output.five_minute;


        output.has_fifteen_minus_five =
            true;
    }


    if (
        output.has_one_minute &&
        output.has_fifteen_minute
    )
    {
        output.fifteen_minus_one =
            output.fifteen_minute -
            output.one_minute;


        output.has_fifteen_minus_one =
            true;
    }


    return output;
}


// ============================================================================
// Input validation
//
// All Phase 3 feature packages used in one composition must describe the
// same symbol and exact same decision point.
// ============================================================================

void MultiTimeframeFeatureEngine::validateInputs(
    const PriceReturnFeatures& price_return,
    const TrendMomentumFeatures& trend_momentum,
    const VolumeFeatures& volume,
    const VolatilityFeatures& volatility) const
{
    if (
        price_return.symbol != symbol_ ||
        trend_momentum.symbol != symbol_ ||
        volume.symbol != symbol_ ||
        volatility.symbol != symbol_
    )
    {
        throw std::invalid_argument(
            "MultiTimeframeFeatureEngine feature symbol mismatch."
        );
    }


    if (
        price_return.decision_time !=
            trend_momentum.decision_time ||
        price_return.decision_time !=
            volume.decision_time ||
        price_return.decision_time !=
            volatility.decision_time
    )
    {
        throw std::invalid_argument(
            "MultiTimeframeFeatureEngine decision-time mismatch."
        );
    }
}


// ============================================================================
// Update
//
// This engine is intentionally STATELESS.
//
// It does not:
//     maintain candle history
//     calculate EMA
//     calculate RSI
//     calculate RVOL
//     calculate ATR
//     calculate returns
//
// It only composes already-valid Phase 3.1-3.4 observations.
// ============================================================================

MultiTimeframeFeatures
MultiTimeframeFeatureEngine::update(
    const PriceReturnFeatures& price_return,
    const TrendMomentumFeatures& trend_momentum,
    const VolumeFeatures& volume,
    const VolatilityFeatures& volatility) const
{
    validateInputs(
        price_return,
        trend_momentum,
        volume,
        volatility
    );


    MultiTimeframeFeatures output;


    output.symbol =
        symbol_;


    output.decision_time =
        price_return.decision_time;


    // ========================================================================
    // 1. Return alignment
    // ========================================================================

    output.return_alignment =
        makeAlignment(
            directionFromValue(
                price_return.one_minute_return
            ),
            price_return.has_one_minute &&
                std::isfinite(
                    price_return.one_minute_return
                ),

            directionFromValue(
                price_return.five_minute_return
            ),
            price_return.has_five_minute &&
                std::isfinite(
                    price_return.five_minute_return
                ),

            directionFromValue(
                price_return.fifteen_minute_return
            ),
            price_return.has_fifteen_minute &&
                std::isfinite(
                    price_return.fifteen_minute_return
                )
        );

    // ========================================================================
    // 2. EMA20 / EMA50 alignment
    // ========================================================================

    output.ema_alignment =
        makeAlignment(
            directionFromPair(
                trend_momentum.one_minute.ema20,
                trend_momentum.one_minute.ema50
            ),
            trend_momentum.one_minute.has_ema20 &&
                trend_momentum.one_minute.has_ema50,

            directionFromPair(
                trend_momentum.five_minute.ema20,
                trend_momentum.five_minute.ema50
            ),
            trend_momentum.five_minute.has_ema20 &&
                trend_momentum.five_minute.has_ema50,

            directionFromPair(
                trend_momentum.fifteen_minute.ema20,
                trend_momentum.fifteen_minute.ema50
            ),
            trend_momentum.fifteen_minute.has_ema20 &&
                trend_momentum.fifteen_minute.has_ema50
        );


    // ========================================================================
    // 3. EMA20 slope alignment
    // ========================================================================

    output.ema20_slope_alignment =
        makeAlignment(
            directionFromValue(
                trend_momentum.one_minute.ema20_slope
            ),
            trend_momentum.one_minute.has_ema20_slope,

            directionFromValue(
                trend_momentum.five_minute.ema20_slope
            ),
            trend_momentum.five_minute.has_ema20_slope,

            directionFromValue(
                trend_momentum.fifteen_minute.ema20_slope
            ),
            trend_momentum.fifteen_minute.has_ema20_slope
        );


    // ========================================================================
    // 4. RSI alignment around mathematical midpoint 50
    //
    // RSI-50 gives:
    //
    //     positive -> RSI > 50
    //     negative -> RSI < 50
    //     neutral  -> RSI = 50
    // ========================================================================

    output.rsi_alignment =
        makeAlignment(
            directionFromValue(
                trend_momentum.one_minute.rsi14 -
                50.0
            ),
            trend_momentum.one_minute.has_rsi14,

            directionFromValue(
                trend_momentum.five_minute.rsi14 -
                50.0
            ),
            trend_momentum.five_minute.has_rsi14,

            directionFromValue(
                trend_momentum.fifteen_minute.rsi14 -
                50.0
            ),
            trend_momentum.fifteen_minute.has_rsi14
        );


    // ========================================================================
    // 5. RVOL20 relationships
    // ========================================================================

    output.rvol20_ratios =
        makeRatios(
            volume.one_minute.relative_volume_20,
            volume.one_minute.has_relative_volume_20,

            volume.five_minute.relative_volume_20,
            volume.five_minute.has_relative_volume_20,

            volume.fifteen_minute.relative_volume_20,
            volume.fifteen_minute.has_relative_volume_20
        );


    // ========================================================================
    // 6. ATR14% relationships
    // ========================================================================

    output.atr14_percent_ratios =
        makeRatios(
            volatility.one_minute.atr14_percent,
            volatility.one_minute.has_atr14_percent,

            volatility.five_minute.atr14_percent,
            volatility.five_minute.has_atr14_percent,

            volatility.fifteen_minute.atr14_percent,
            volatility.fifteen_minute.has_atr14_percent
        );


    // ========================================================================
    // 7. Realized Volatility 20 relationships
    // ========================================================================

    output.realized_volatility_20_ratios =
        makeRatios(
            volatility.one_minute.realized_volatility_20,
            volatility.one_minute.has_realized_volatility_20,

            volatility.five_minute.realized_volatility_20,
            volatility.five_minute.has_realized_volatility_20,

            volatility.fifteen_minute.realized_volatility_20,
            volatility.fifteen_minute.has_realized_volatility_20
        );


    // ========================================================================
    // 8. RSI cross-timeframe differences
    // ========================================================================

    output.rsi_relationship =
        makeRSIRelationship(
            trend_momentum.one_minute.rsi14,
            trend_momentum.one_minute.has_rsi14,

            trend_momentum.five_minute.rsi14,
            trend_momentum.five_minute.has_rsi14,

            trend_momentum.fifteen_minute.rsi14,
            trend_momentum.fifteen_minute.has_rsi14
        );


    // ========================================================================
    // Core readiness
    // ========================================================================

    output.core_features_ready =
        output.return_alignment.fully_available &&
        output.ema_alignment.fully_available &&
        output.rsi_alignment.fully_available;


    return output;
}

} // namespace devai::features