#include "devai/ranking/CrossSectionalMathematicalRanker.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>


namespace devai::ranking
{

using devai::statistics::MathematicalHorizonState;
using devai::statistics::MathematicalStateFeatures;


// ============================================================================
// Constructor
// ============================================================================

CrossSectionalMathematicalRanker::
CrossSectionalMathematicalRanker(
    std::size_t minimum_universe_size)
    :
    ranker_(
        minimum_universe_size)
{
}


// ============================================================================
// Configuration
// ============================================================================

std::size_t
CrossSectionalMathematicalRanker::
minimumUniverseSize() const noexcept
{
    return
        ranker_.minimumUniverseSize();
}


// ============================================================================
// Validate cross-sectional Phase-4 input
//
// Phase 5.2 requires:
//
// 1. Unique stock symbols.
// 2. Non-empty stock symbols.
// 3. Non-empty benchmark symbols.
// 4. Stock cannot equal benchmark.
// 5. Every stock must use the same benchmark.
// 6. Every state must belong to exactly the same decision time.
//
// Individual metrics do NOT need to be globally ready.
// Metric-specific readiness is handled later.
// ============================================================================

void
CrossSectionalMathematicalRanker::
validateStates(
    const std::vector<
        MathematicalStateFeatures>& states)
{
    if (states.empty())
    {
        return;
    }


    const std::int64_t decision_time =
        states.front().
            decision_time;


    const std::string benchmark_symbol =
        states.front().
            benchmark_symbol;


    if (benchmark_symbol.empty())
    {
        throw std::invalid_argument(
            "Cross-sectional mathematical input "
            "contains an empty benchmark symbol.");
    }


    std::unordered_set<std::string>
        symbols;


    symbols.reserve(
        states.size());


    for (const auto& state :
         states)
    {
        if (state.stock_symbol.empty())
        {
            throw std::invalid_argument(
                "Cross-sectional mathematical input "
                "contains an empty stock symbol.");
        }


        if (state.benchmark_symbol.empty())
        {
            throw std::invalid_argument(
                "Cross-sectional mathematical input "
                "contains an empty benchmark symbol.");
        }


        if (state.stock_symbol ==
            state.benchmark_symbol)
        {
            throw std::invalid_argument(
                "Benchmark cannot be ranked as its own stock: " +
                state.stock_symbol);
        }


        if (state.benchmark_symbol !=
            benchmark_symbol)
        {
            throw std::invalid_argument(
                "Cross-sectional mathematical states "
                "contain different benchmark symbols.");
        }


        if (state.decision_time !=
            decision_time)
        {
            throw std::invalid_argument(
                "Cross-sectional mathematical states "
                "contain different decision times.");
        }


        if (!symbols.insert(
                state.stock_symbol).
                second)
        {
            throw std::invalid_argument(
                "Duplicate stock symbol in cross-sectional "
                "mathematical input: " +
                state.stock_symbol);
        }
    }
}


// ============================================================================
// Extract one mathematical horizon
// ============================================================================

namespace
{

const MathematicalHorizonState&
selectHorizon(
    const MathematicalStateFeatures& state,
    CrossSectionalMathematicalRanker::Horizon horizon)
{
    switch (horizon)
    {
        case CrossSectionalMathematicalRanker::
            Horizon::ONE_MINUTE:

            return
                state.one_minute;


        case CrossSectionalMathematicalRanker::
            Horizon::FIVE_MINUTE:

            return
                state.five_minute;


        case CrossSectionalMathematicalRanker::
            Horizon::FIFTEEN_MINUTE:

            return
                state.fifteen_minute;
    }


    throw std::logic_error(
        "Unknown mathematical horizon.");
}

} // namespace


// ============================================================================
// Extract one metric
//
// IMPORTANT:
//
// Availability is metric-specific.
//
// A stock does NOT need the complete Phase-4 state to be fully_ready in order
// to participate in a metric whose own underlying observation is valid.
//
// This prevents one missing metric from unnecessarily removing a stock from
// every cross-sectional universe.
// ============================================================================

CrossSectionalMathematicalRanker::MetricValue
CrossSectionalMathematicalRanker::
extractMetric(
    const MathematicalStateFeatures& state,
    Horizon horizon,
    Metric metric)
{
    const auto& source =
        selectHorizon(
            state,
            horizon);


    MetricValue output;


    switch (metric)
    {
        // ====================================================================
        // Return
        // ====================================================================

        case Metric::RETURN_VALUE:
        {
            const auto& value =
                source.return_statistics;


            output.available =
                value.has_observation &&
                std::isfinite(
                    value.return_value);


            output.value =
                value.return_value;

            break;
        }


        case Metric::RETURN_Z_SCORE:
        {
            const auto& value =
                source.return_statistics;


            output.available =
                value.ready &&
                value.has_observation &&
                std::isfinite(
                    value.z_score);


            output.value =
                value.z_score;

            break;
        }


        case Metric::RETURN_ROBUST_Z_SCORE:
        {
            const auto& value =
                source.return_statistics;


            output.available =
                value.ready &&
                value.has_observation &&
                std::isfinite(
                    value.robust_z_score);


            output.value =
                value.robust_z_score;

            break;
        }


        // ====================================================================
        // Volume
        // ====================================================================

        case Metric::VOLUME_Z_SCORE:
        {
            const auto& value =
                source.volume_statistics.
                    volume;


            output.available =
                value.ready &&
                value.has_observation &&
                std::isfinite(
                    value.z_score);


            output.value =
                value.z_score;

            break;
        }


        case Metric::RVOL20_Z_SCORE:
        {
            const auto& value =
                source.volume_statistics.
                    rvol20;


            output.available =
                value.ready &&
                value.has_observation &&
                std::isfinite(
                    value.z_score);


            output.value =
                value.z_score;

            break;
        }


        // ====================================================================
        // Stock / NIFTY relationship
        // ====================================================================

        case Metric::RELATIVE_RETURN:
        {
            const auto& value =
                source.stock_benchmark_statistics;


            output.available =
                value.has_observation &&
                std::isfinite(
                    value.relative_return);


            output.value =
                value.relative_return;

            break;
        }


        case Metric::RESIDUAL_RETURN:
        {
            const auto& value =
                source.stock_benchmark_statistics;


            output.available =
                value.regression_ready &&
                std::isfinite(
                    value.residual_return);


            output.value =
                value.residual_return;

            break;
        }


        case Metric::RESIDUAL_Z_SCORE:
        {
            const auto& value =
                source.stock_benchmark_statistics;


            output.available =
                value.residual_statistics_ready &&
                std::isfinite(
                    value.residual_z_score);


            output.value =
                value.residual_z_score;

            break;
        }


        case Metric::RESIDUAL_ROBUST_Z_SCORE:
        {
            const auto& value =
                source.stock_benchmark_statistics;


            output.available =
                value.residual_statistics_ready &&
                std::isfinite(
                    value.residual_robust_z_score);


            output.value =
                value.residual_robust_z_score;

            break;
        }


        case Metric::ROLLING_BETA:
        {
            const auto& value =
                source.stock_benchmark_statistics;


            output.available =
                value.regression_ready &&
                std::isfinite(
                    value.rolling_beta);


            output.value =
                value.rolling_beta;

            break;
        }


        case Metric::ROLLING_CORRELATION:
        {
            const auto& value =
                source.stock_benchmark_statistics;


            output.available =
                value.regression_ready &&
                std::isfinite(
                    value.rolling_correlation);


            output.value =
                value.rolling_correlation;

            break;
        }


        // ====================================================================
        // Volatility
        //
        // These numerical values are preserved by Phase 4.5 MarketRegimeHorizon.
        // ====================================================================

        case Metric::ATR14_PERCENT:
        {
            const auto& value =
                source.market_regime;


            output.available =
                value.volatility_ready &&
                std::isfinite(
                    value.atr14_percent);


            output.value =
                value.atr14_percent;

            break;
        }


        case Metric::REALIZED_VOLATILITY_20:
        {
            const auto& value =
                source.market_regime;


            output.available =
                value.volatility_ready &&
                std::isfinite(
                    value.realized_volatility_20);


            output.value =
                value.realized_volatility_20;

            break;
        }
    }


    if (!output.available)
    {
        output.value =
            crossSectionalNaN();
    }


    return output;
}


// ============================================================================
// Rank one metric across the stock universe
//
// Ranking direction is DESCENDING:
//
//     highest numerical value -> rank 1
//
// Percentile semantics remain those defined by Phase 5.1:
//
//     lowest numerical value  -> 0
//     highest numerical value -> 1
//
// Rank 1 therefore corresponds to percentile 1 for ordinary distinct values.
// ============================================================================

std::vector<CrossSectionalRank>
CrossSectionalMathematicalRanker::
rankMetric(
    const std::vector<
        MathematicalStateFeatures>& states,
    Horizon horizon,
    Metric metric) const
{
    std::vector<CrossSectionalObservation>
        observations;


    observations.reserve(
        states.size());


    for (const auto& state :
         states)
    {
        const MetricValue metric_value =
            extractMetric(
                state,
                horizon,
                metric);


        observations.push_back(
            CrossSectionalObservation{
                state.stock_symbol,
                state.decision_time,
                metric_value.value,
                metric_value.available
            });
    }


    return
        ranker_.rank(
            observations,
            RankingDirection::DESCENDING);
}


// ============================================================================
// Assign one metric rank to output horizon
// ============================================================================

void
CrossSectionalMathematicalRanker::
assignMetric(
    CrossSectionalMathematicalHorizon& output,
    Metric metric,
    const CrossSectionalRank& rank)
{
    switch (metric)
    {
        case Metric::RETURN_VALUE:

            output.return_value =
                rank;

            break;


        case Metric::RETURN_Z_SCORE:

            output.return_z_score =
                rank;

            break;


        case Metric::RETURN_ROBUST_Z_SCORE:

            output.return_robust_z_score =
                rank;

            break;


        case Metric::VOLUME_Z_SCORE:

            output.volume_z_score =
                rank;

            break;


        case Metric::RVOL20_Z_SCORE:

            output.rvol20_z_score =
                rank;

            break;


        case Metric::RELATIVE_RETURN:

            output.relative_return =
                rank;

            break;


        case Metric::RESIDUAL_RETURN:

            output.residual_return =
                rank;

            break;


        case Metric::RESIDUAL_Z_SCORE:

            output.residual_z_score =
                rank;

            break;


        case Metric::RESIDUAL_ROBUST_Z_SCORE:

            output.residual_robust_z_score =
                rank;

            break;


        case Metric::ATR14_PERCENT:

            output.atr14_percent =
                rank;

            break;


        case Metric::REALIZED_VOLATILITY_20:

            output.realized_volatility_20 =
                rank;

            break;


        case Metric::ROLLING_BETA:

            output.rolling_beta =
                rank;

            break;


        case Metric::ROLLING_CORRELATION:

            output.rolling_correlation =
                rank;

            break;
    }
}


// ============================================================================
// Complete Phase 5.2 ranking
// ============================================================================

std::vector<CrossSectionalMathematicalFeatures>
CrossSectionalMathematicalRanker::
rank(
    const std::vector<
        MathematicalStateFeatures>& states) const
{
    validateStates(
        states);


    std::vector<CrossSectionalMathematicalFeatures>
        output;


    output.reserve(
        states.size());


    if (states.empty())
    {
        return output;
    }


    // ========================================================================
    // Preserve caller stock ordering.
    // ========================================================================

    for (const auto& state :
         states)
    {
        CrossSectionalMathematicalFeatures
            result;


        result.symbol =
            state.stock_symbol;


        result.benchmark_symbol =
            state.benchmark_symbol;


        result.decision_time =
            state.decision_time;


        output.push_back(
            std::move(result));
    }


    // ========================================================================
    // Metrics included in Phase 5.2.
    // ========================================================================

    constexpr Metric metrics[] =
    {
        Metric::RETURN_VALUE,
        Metric::RETURN_Z_SCORE,
        Metric::RETURN_ROBUST_Z_SCORE,

        Metric::VOLUME_Z_SCORE,
        Metric::RVOL20_Z_SCORE,

        Metric::RELATIVE_RETURN,

        Metric::RESIDUAL_RETURN,
        Metric::RESIDUAL_Z_SCORE,
        Metric::RESIDUAL_ROBUST_Z_SCORE,

        Metric::ATR14_PERCENT,
        Metric::REALIZED_VOLATILITY_20,

        Metric::ROLLING_BETA,
        Metric::ROLLING_CORRELATION
    };


    constexpr Horizon horizons[] =
    {
        Horizon::ONE_MINUTE,
        Horizon::FIVE_MINUTE,
        Horizon::FIFTEEN_MINUTE
    };


    // ========================================================================
    // Rank every metric independently for every timeframe.
    // ========================================================================

    for (const Horizon horizon :
         horizons)
    {
        for (const Metric metric :
             metrics)
        {
            const auto ranks =
                rankMetric(
                    states,
                    horizon,
                    metric);


            if (ranks.size() !=
                output.size())
            {
                throw std::logic_error(
                    "Cross-sectional mathematical rank "
                    "output-size mismatch.");
            }


            for (std::size_t index = 0;
                 index < output.size();
                 ++index)
            {
                if (ranks[index].symbol !=
                    output[index].symbol)
                {
                    throw std::logic_error(
                        "Cross-sectional mathematical "
                        "symbol-order mismatch.");
                }


                switch (horizon)
                {
                    case Horizon::ONE_MINUTE:

                        assignMetric(
                            output[index].
                                one_minute,
                            metric,
                            ranks[index]);

                        break;


                    case Horizon::FIVE_MINUTE:

                        assignMetric(
                            output[index].
                                five_minute,
                            metric,
                            ranks[index]);

                        break;


                    case Horizon::FIFTEEN_MINUTE:

                        assignMetric(
                            output[index].
                                fifteen_minute,
                            metric,
                            ranks[index]);

                        break;
                }
            }
        }
    }


    return output;
}

} // namespace devai::ranking