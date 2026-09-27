#include "devai/ranking/CrossSectionalMathematicalStateComposer.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>


namespace devai::ranking
{

// ============================================================================
// Validate complete Phase-5.2 input
//
// Phase 5.3 is a multi-stock composer, so all supplied stock ranking objects
// must describe the same legal cross-sectional decision point.
//
// Phase 5.4 will add the formal universe-snapshot abstraction later.
// ============================================================================

void
CrossSectionalMathematicalStateComposer::
validateInput(
    const std::vector<
        CrossSectionalMathematicalFeatures>&
        features)
{
    if (features.empty())
    {
        return;
    }


    const std::int64_t decision_time =
        features.front().
            decision_time;


    const std::string benchmark_symbol =
        features.front().
            benchmark_symbol;


    if (benchmark_symbol.empty())
    {
        throw std::invalid_argument(
            "Cross-sectional state input contains "
            "an empty benchmark symbol.");
    }


    std::unordered_set<std::string>
        symbols;


    symbols.reserve(
        features.size());


    for (const auto& feature :
         features)
    {
        if (feature.symbol.empty())
        {
            throw std::invalid_argument(
                "Cross-sectional state input contains "
                "an empty stock symbol.");
        }


        if (feature.benchmark_symbol.empty())
        {
            throw std::invalid_argument(
                "Cross-sectional state input contains "
                "an empty benchmark symbol.");
        }


        if (feature.symbol ==
            feature.benchmark_symbol)
        {
            throw std::invalid_argument(
                "Benchmark cannot appear as its own "
                "ranked stock: " +
                feature.symbol);
        }


        if (feature.decision_time !=
            decision_time)
        {
            throw std::invalid_argument(
                "Cross-sectional state input contains "
                "different decision times.");
        }


        if (feature.benchmark_symbol !=
            benchmark_symbol)
        {
            throw std::invalid_argument(
                "Cross-sectional state input contains "
                "different benchmark symbols.");
        }


        if (!symbols.insert(
                feature.symbol).
                second)
        {
            throw std::invalid_argument(
                "Duplicate stock symbol in "
                "cross-sectional state input: " +
                feature.symbol);
        }
    }
}


// ============================================================================
// Validate one Phase-5.1 rank
//
// AVAILABLE RANK:
//
// - symbol must match
// - decision time must match
// - value finite
// - rank finite
// - universe >= 2
// - rank in [1, universe_size]
// - percentile finite and in [0, 1]
// - centered percentile finite and in [-1, 1]
//
// UNAVAILABLE RANK:
//
// Phase 5.1 deliberately preserves universe_size even when the universe is
// below the configured minimum. Therefore an unavailable rank may legitimately
// contain universe_size = 0 or 1.
//
// Its numerical ranking fields must remain NaN.
// ============================================================================

bool
CrossSectionalMathematicalStateComposer::
validateRank(
    const CrossSectionalRank& rank,
    const std::string& expected_symbol,
    std::int64_t expected_decision_time)
{
    if (rank.symbol !=
        expected_symbol)
    {
        return false;
    }


    if (rank.decision_time !=
        expected_decision_time)
    {
        return false;
    }


    if (!rank.available)
    {
        // Phase 5.1 must not fabricate numerical ranking values
        // for an unavailable observation.

        if (std::isfinite(
                rank.rank))
        {
            return false;
        }


        if (std::isfinite(
                rank.percentile))
        {
            return false;
        }


        if (std::isfinite(
                rank.centered_percentile))
        {
            return false;
        }


        return true;
    }


    // ------------------------------------------------------------------------
    // Available rank
    // ------------------------------------------------------------------------

    if (!std::isfinite(
            rank.value))
    {
        return false;
    }


    if (!std::isfinite(
            rank.rank))
    {
        return false;
    }


    if (!std::isfinite(
            rank.percentile))
    {
        return false;
    }


    if (!std::isfinite(
            rank.centered_percentile))
    {
        return false;
    }


    if (rank.universe_size < 2)
    {
        return false;
    }


    const double universe_size =
        static_cast<double>(
            rank.universe_size);


    if (rank.rank < 1.0 ||
        rank.rank > universe_size)
    {
        return false;
    }


    constexpr double epsilon =
        1.0e-12;


    if (rank.percentile <
            -epsilon ||
        rank.percentile >
            1.0 + epsilon)
    {
        return false;
    }


    if (rank.centered_percentile <
            -1.0 - epsilon ||
        rank.centered_percentile >
            1.0 + epsilon)
    {
        return false;
    }


    // ------------------------------------------------------------------------
    // Preserve Phase-5.1 percentile relationship:
    //
    // centered = 2 * percentile - 1
    // ------------------------------------------------------------------------

    const double expected_centered =
        2.0 *
        rank.percentile -
        1.0;


    if (std::fabs(
            rank.centered_percentile -
            expected_centered) >
        epsilon)
    {
        return false;
    }


    return true;
}


// ============================================================================
// Metric readiness
// ============================================================================

CrossSectionalMetricReadiness
CrossSectionalMathematicalStateComposer::
composeMetricReadiness(
    const CrossSectionalRank& rank)
{
    CrossSectionalMetricReadiness
        output;


    output.available =
        rank.available;


    output.universe_size =
        rank.universe_size;


    return output;
}


// ============================================================================
// Compose one timeframe
// ============================================================================

CrossSectionalHorizonState
CrossSectionalMathematicalStateComposer::
composeHorizon(
    const CrossSectionalMathematicalHorizon&
        rankings,
    const std::string& expected_symbol,
    std::int64_t expected_decision_time)
{
    CrossSectionalHorizonState
        output;


    // ========================================================================
    // Lossless preservation of Phase-5.2 ranking state
    // ========================================================================

    output.rankings =
        rankings;


    // ========================================================================
    // Metric readiness
    // ========================================================================

    output.readiness.return_value =
        composeMetricReadiness(
            rankings.return_value);


    output.readiness.return_z_score =
        composeMetricReadiness(
            rankings.return_z_score);


    output.readiness.return_robust_z_score =
        composeMetricReadiness(
            rankings.return_robust_z_score);


    output.readiness.volume_z_score =
        composeMetricReadiness(
            rankings.volume_z_score);


    output.readiness.rvol20_z_score =
        composeMetricReadiness(
            rankings.rvol20_z_score);


    output.readiness.relative_return =
        composeMetricReadiness(
            rankings.relative_return);


    output.readiness.residual_return =
        composeMetricReadiness(
            rankings.residual_return);


    output.readiness.residual_z_score =
        composeMetricReadiness(
            rankings.residual_z_score);


    output.readiness.residual_robust_z_score =
        composeMetricReadiness(
            rankings.residual_robust_z_score);


    output.readiness.atr14_percent =
        composeMetricReadiness(
            rankings.atr14_percent);


    output.readiness.realized_volatility_20 =
        composeMetricReadiness(
            rankings.realized_volatility_20);


    output.readiness.rolling_beta =
        composeMetricReadiness(
            rankings.rolling_beta);


    output.readiness.rolling_correlation =
        composeMetricReadiness(
            rankings.rolling_correlation);


    // ========================================================================
    // Numerical integrity
    // ========================================================================

    const bool return_value_valid =
        validateRank(
            rankings.return_value,
            expected_symbol,
            expected_decision_time);


    const bool return_z_valid =
        validateRank(
            rankings.return_z_score,
            expected_symbol,
            expected_decision_time);


    const bool return_robust_z_valid =
        validateRank(
            rankings.return_robust_z_score,
            expected_symbol,
            expected_decision_time);


    const bool volume_z_valid =
        validateRank(
            rankings.volume_z_score,
            expected_symbol,
            expected_decision_time);


    const bool rvol20_z_valid =
        validateRank(
            rankings.rvol20_z_score,
            expected_symbol,
            expected_decision_time);


    const bool relative_return_valid =
        validateRank(
            rankings.relative_return,
            expected_symbol,
            expected_decision_time);


    const bool residual_return_valid =
        validateRank(
            rankings.residual_return,
            expected_symbol,
            expected_decision_time);


    const bool residual_z_valid =
        validateRank(
            rankings.residual_z_score,
            expected_symbol,
            expected_decision_time);


    const bool residual_robust_z_valid =
        validateRank(
            rankings.residual_robust_z_score,
            expected_symbol,
            expected_decision_time);


    const bool atr14_percent_valid =
        validateRank(
            rankings.atr14_percent,
            expected_symbol,
            expected_decision_time);


    const bool realized_volatility_valid =
        validateRank(
            rankings.realized_volatility_20,
            expected_symbol,
            expected_decision_time);


    const bool rolling_beta_valid =
        validateRank(
            rankings.rolling_beta,
            expected_symbol,
            expected_decision_time);


    const bool rolling_correlation_valid =
        validateRank(
            rankings.rolling_correlation,
            expected_symbol,
            expected_decision_time);


    output.readiness.numerical_integrity =
        return_value_valid &&
        return_z_valid &&
        return_robust_z_valid &&
        volume_z_valid &&
        rvol20_z_valid &&
        relative_return_valid &&
        residual_return_valid &&
        residual_z_valid &&
        residual_robust_z_valid &&
        atr14_percent_valid &&
        realized_volatility_valid &&
        rolling_beta_valid &&
        rolling_correlation_valid;


    // ========================================================================
    // Available metric count
    // ========================================================================

    output.readiness.available_metric_count =
        0;


    const CrossSectionalMetricReadiness*
        metric_readiness[] =
    {
        &output.readiness.return_value,
        &output.readiness.return_z_score,
        &output.readiness.return_robust_z_score,

        &output.readiness.volume_z_score,
        &output.readiness.rvol20_z_score,

        &output.readiness.relative_return,

        &output.readiness.residual_return,
        &output.readiness.residual_z_score,
        &output.readiness.residual_robust_z_score,

        &output.readiness.atr14_percent,
        &output.readiness.realized_volatility_20,

        &output.readiness.rolling_beta,
        &output.readiness.rolling_correlation
    };


    for (const auto* metric :
         metric_readiness)
    {
        if (metric->available)
        {
            ++output.readiness.
                available_metric_count;
        }
    }


    output.readiness.total_metric_count =
        METRIC_COUNT;


    // ========================================================================
    // Full timeframe readiness
    //
    // "fully_ready" deliberately means that all 13 Phase-5.2 metrics for
    // this stock/timeframe have valid ranks.
    //
    // Partial state is still preserved and remains useful.
    // ========================================================================

    output.readiness.fully_ready =
        output.readiness.
            numerical_integrity &&
        output.readiness.
            available_metric_count ==
            METRIC_COUNT;


    return output;
}


// ============================================================================
// Compose complete multi-timeframe state
// ============================================================================

std::vector<CrossSectionalMathematicalState>
CrossSectionalMathematicalStateComposer::
compose(
    const std::vector<
        CrossSectionalMathematicalFeatures>&
        features) const
{
    validateInput(
        features);


    std::vector<
        CrossSectionalMathematicalState>
        output;


    output.reserve(
        features.size());


    for (const auto& feature :
         features)
    {
        CrossSectionalMathematicalState
            state;


        // --------------------------------------------------------------------
        // Identity
        // --------------------------------------------------------------------

        state.symbol =
            feature.symbol;


        state.benchmark_symbol =
            feature.benchmark_symbol;


        state.decision_time =
            feature.decision_time;


        // --------------------------------------------------------------------
        // 1-minute
        // --------------------------------------------------------------------

        state.one_minute =
            composeHorizon(
                feature.one_minute,
                feature.symbol,
                feature.decision_time);


        // --------------------------------------------------------------------
        // 5-minute
        // --------------------------------------------------------------------

        state.five_minute =
            composeHorizon(
                feature.five_minute,
                feature.symbol,
                feature.decision_time);


        // --------------------------------------------------------------------
        // 15-minute
        // --------------------------------------------------------------------

        state.fifteen_minute =
            composeHorizon(
                feature.fifteen_minute,
                feature.symbol,
                feature.decision_time);


        // --------------------------------------------------------------------
        // Timeframe readiness
        // --------------------------------------------------------------------

        state.one_minute_ready =
            state.one_minute.
                readiness.
                fully_ready;


        state.five_minute_ready =
            state.five_minute.
                readiness.
                fully_ready;


        state.fifteen_minute_ready =
            state.fifteen_minute.
                readiness.
                fully_ready;


        // --------------------------------------------------------------------
        // Global numerical integrity
        // --------------------------------------------------------------------

        state.numerical_integrity =
            state.one_minute.
                readiness.
                numerical_integrity &&
            state.five_minute.
                readiness.
                numerical_integrity &&
            state.fifteen_minute.
                readiness.
                numerical_integrity;


        // --------------------------------------------------------------------
        // Complete multi-timeframe readiness
        // --------------------------------------------------------------------

        state.fully_ready =
            state.numerical_integrity &&
            state.one_minute_ready &&
            state.five_minute_ready &&
            state.fifteen_minute_ready;


        output.push_back(
            std::move(state));
    }


    return output;
}

} // namespace devai::ranking