#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"

#include <cmath>
#include <stdexcept>


namespace devai::strategy
{

namespace
{

using devai::ranking::CrossSectionalHorizonState;
using devai::ranking::CrossSectionalRank;


// ============================================================================
// Store one rank
// ============================================================================

void storeRank(
    DevAIObservation& observation,
    DevTimeframe timeframe,
    DevMetric metric,
    const CrossSectionalRank& rank)
{
    const std::size_t index =
        marketIndex(
            timeframe,
            metric);


    if (!rank.available)
    {
        observation.market[index] =
            0.0;

        observation.availability[index] =
            0.0;

        return;
    }


    if (!std::isfinite(
            rank.centered_percentile))
    {
        throw std::runtime_error(
            "Available CETE rank contains "
            "non-finite centered percentile.");
    }


    if (rank.centered_percentile < -1.0 ||
        rank.centered_percentile > 1.0)
    {
        throw std::runtime_error(
            "Centered percentile outside [-1, 1].");
    }


    observation.market[index] =
        rank.centered_percentile;

    observation.availability[index] =
        1.0;
}


// ============================================================================
// Store all 13 metrics for one timeframe
// ============================================================================

void storeHorizon(
    DevAIObservation& observation,
    DevTimeframe timeframe,
    const CrossSectionalHorizonState& horizon)
{
    const auto& r =
        horizon.rankings;


    storeRank(
        observation,
        timeframe,
        DevMetric::RETURN_VALUE,
        r.return_value);

    storeRank(
        observation,
        timeframe,
        DevMetric::RETURN_Z_SCORE,
        r.return_z_score);

    storeRank(
        observation,
        timeframe,
        DevMetric::RETURN_ROBUST_Z_SCORE,
        r.return_robust_z_score);

    storeRank(
        observation,
        timeframe,
        DevMetric::VOLUME_Z_SCORE,
        r.volume_z_score);

    storeRank(
        observation,
        timeframe,
        DevMetric::RVOL20_Z_SCORE,
        r.rvol20_z_score);

    storeRank(
        observation,
        timeframe,
        DevMetric::RELATIVE_RETURN,
        r.relative_return);

    storeRank(
        observation,
        timeframe,
        DevMetric::RESIDUAL_RETURN,
        r.residual_return);

    storeRank(
        observation,
        timeframe,
        DevMetric::RESIDUAL_Z_SCORE,
        r.residual_z_score);

    storeRank(
        observation,
        timeframe,
        DevMetric::RESIDUAL_ROBUST_Z_SCORE,
        r.residual_robust_z_score);

    storeRank(
        observation,
        timeframe,
        DevMetric::ATR14_PERCENT,
        r.atr14_percent);

    storeRank(
        observation,
        timeframe,
        DevMetric::REALIZED_VOLATILITY_20,
        r.realized_volatility_20);

    storeRank(
        observation,
        timeframe,
        DevMetric::ROLLING_BETA,
        r.rolling_beta);

    storeRank(
        observation,
        timeframe,
        DevMetric::ROLLING_CORRELATION,
        r.rolling_correlation);
}

} // anonymous namespace


DevAIObservation
ObservationBuilder::build(
    const devai::ranking::CrossSectionalUniverseSnapshot& snapshot,
    const std::string& symbol,
    const DevPortfolioState& portfolio) const
{
    if (symbol.empty())
    {
        throw std::invalid_argument(
            "Dev observation symbol cannot be empty.");
    }


    if (!snapshot.numerical_integrity)
    {
        throw std::invalid_argument(
            "Dev cannot consume a CETE snapshot "
            "with failed numerical integrity.");
    }


    if (snapshot.decision_time <= 0)
    {
        throw std::invalid_argument(
            "CETE snapshot has invalid decision time.");
    }


    if (symbol ==
        snapshot.benchmark_symbol)
    {
        throw std::invalid_argument(
            "Dev cannot trade the benchmark.");
    }


    const devai::ranking::CrossSectionalMathematicalState*
        selected = nullptr;


    for (const auto& stock :
         snapshot.stocks)
    {
        if (stock.symbol == symbol)
        {
            selected =
                &stock;

            break;
        }
    }


    if (selected == nullptr)
    {
        throw std::invalid_argument(
            "Requested symbol is not present "
            "in CETE universe snapshot: " +
            symbol);
    }


    if (selected->decision_time !=
        snapshot.decision_time)
    {
        throw std::runtime_error(
            "Stock state decision time differs "
            "from CETE universe decision time.");
    }


    if (selected->benchmark_symbol !=
        snapshot.benchmark_symbol)
    {
        throw std::runtime_error(
            "Stock benchmark differs from "
            "CETE universe benchmark.");
    }


    if (!selected->numerical_integrity)
    {
        throw std::runtime_error(
            "Stock cross-sectional state failed "
            "numerical integrity.");
    }


    DevAIObservation output;

    output.symbol =
        selected->symbol;

    output.benchmark_symbol =
        selected->benchmark_symbol;

    output.decision_time =
        selected->decision_time;

    output.portfolio =
        portfolio;


    storeHorizon(
        output,
        DevTimeframe::ONE_MINUTE,
        selected->one_minute);

    storeHorizon(
        output,
        DevTimeframe::FIVE_MINUTE,
        selected->five_minute);

    storeHorizon(
        output,
        DevTimeframe::FIFTEEN_MINUTE,
        selected->fifteen_minute);


    output.numerical_integrity =
        true;


    // Partial CETE metric availability is legal.
    //
    // Observation is ready as long as at least one
    // legal market feature is available.

    bool any_available =
        false;


    for (const double available :
         output.availability)
    {
        if (available > 0.5)
        {
            any_available =
                true;

            break;
        }
    }


    output.ready =
        any_available;


    return output;
}

} // namespace devai::strategy