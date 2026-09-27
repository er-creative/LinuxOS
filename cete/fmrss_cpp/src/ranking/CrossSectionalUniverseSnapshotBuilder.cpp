#include "devai/ranking/CrossSectionalUniverseSnapshotBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>


namespace devai::ranking
{

// ============================================================================
// Constructor
// ============================================================================

CrossSectionalUniverseSnapshotBuilder::
CrossSectionalUniverseSnapshotBuilder(
    std::size_t minimum_universe_size)
    :
    minimum_universe_size_(
        minimum_universe_size)
{
    if (minimum_universe_size_ < 2)
    {
        throw std::invalid_argument(
            "Cross-sectional universe minimum size "
            "must be at least 2.");
    }
}


// ============================================================================
// Configuration
// ============================================================================

std::size_t
CrossSectionalUniverseSnapshotBuilder::
minimumUniverseSize() const noexcept
{
    return
        minimum_universe_size_;
}


// ============================================================================
// Identity validation
// ============================================================================

void
CrossSectionalUniverseSnapshotBuilder::
validateIdentity(
    const std::vector<
        CrossSectionalMathematicalState>&
        states)
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
            "Universe snapshot contains "
            "an empty benchmark symbol.");
    }


    std::unordered_set<std::string>
        symbols;


    symbols.reserve(
        states.size());


    for (const auto& state :
         states)
    {
        if (state.symbol.empty())
        {
            throw std::invalid_argument(
                "Universe snapshot contains "
                "an empty stock symbol.");
        }


        if (state.benchmark_symbol.empty())
        {
            throw std::invalid_argument(
                "Universe snapshot contains "
                "an empty benchmark symbol.");
        }


        if (state.symbol ==
            state.benchmark_symbol)
        {
            throw std::invalid_argument(
                "Benchmark cannot appear in "
                "the ranked stock universe: " +
                state.symbol);
        }


        if (state.decision_time !=
            decision_time)
        {
            throw std::invalid_argument(
                "Universe snapshot contains "
                "different decision times.");
        }


        if (state.benchmark_symbol !=
            benchmark_symbol)
        {
            throw std::invalid_argument(
                "Universe snapshot contains "
                "different benchmark symbols.");
        }


        if (!symbols.insert(
                state.symbol).
                second)
        {
            throw std::invalid_argument(
                "Duplicate stock symbol in "
                "universe snapshot: " +
                state.symbol);
        }
    }
}


// ============================================================================
// Validate one rank
// ============================================================================

bool
CrossSectionalUniverseSnapshotBuilder::
validateMetricRank(
    const CrossSectionalRank& rank,
    const std::string& expected_symbol,
    std::int64_t expected_decision_time,
    std::size_t total_stock_count)
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


    if (rank.universe_size >
        total_stock_count)
    {
        return false;
    }


    if (!rank.available)
    {
        if (std::isfinite(
                rank.rank) ||
            std::isfinite(
                rank.percentile) ||
            std::isfinite(
                rank.centered_percentile))
        {
            return false;
        }


        return true;
    }


    if (!std::isfinite(
            rank.value) ||
        !std::isfinite(
            rank.rank) ||
        !std::isfinite(
            rank.percentile) ||
        !std::isfinite(
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
// Validate one timeframe
// ============================================================================

bool
CrossSectionalUniverseSnapshotBuilder::
validateHorizon(
    const CrossSectionalHorizonState& horizon,
    const std::string& expected_symbol,
    std::int64_t expected_decision_time,
    std::size_t total_stock_count)
{
    const auto& ranks =
        horizon.rankings;


    const CrossSectionalRank*
        all_ranks[] =
    {
        &ranks.return_value,
        &ranks.return_z_score,
        &ranks.return_robust_z_score,

        &ranks.volume_z_score,
        &ranks.rvol20_z_score,

        &ranks.relative_return,

        &ranks.residual_return,
        &ranks.residual_z_score,
        &ranks.residual_robust_z_score,

        &ranks.atr14_percent,
        &ranks.realized_volatility_20,

        &ranks.rolling_beta,
        &ranks.rolling_correlation
    };


    std::size_t available_count =
        0;


    for (const auto* rank :
         all_ranks)
    {
        if (!validateMetricRank(
                *rank,
                expected_symbol,
                expected_decision_time,
                total_stock_count))
        {
            return false;
        }


        if (rank->available)
        {
            ++available_count;
        }
    }


    if (horizon.readiness.
            available_metric_count !=
        available_count)
    {
        return false;
    }


    if (horizon.readiness.
            total_metric_count !=
        13)
    {
        return false;
    }


    if (!horizon.readiness.
            numerical_integrity)
    {
        return false;
    }


    const bool expected_fully_ready =
        available_count == 13;


    if (horizon.readiness.
            fully_ready !=
        expected_fully_ready)
    {
        return false;
    }


    return true;
}


// ============================================================================
// Validate one Phase-5.3 stock state
// ============================================================================

bool
CrossSectionalUniverseSnapshotBuilder::
validateState(
    const CrossSectionalMathematicalState& state,
    std::size_t total_stock_count)
{
    const bool one_minute_valid =
        validateHorizon(
            state.one_minute,
            state.symbol,
            state.decision_time,
            total_stock_count);


    const bool five_minute_valid =
        validateHorizon(
            state.five_minute,
            state.symbol,
            state.decision_time,
            total_stock_count);


    const bool fifteen_minute_valid =
        validateHorizon(
            state.fifteen_minute,
            state.symbol,
            state.decision_time,
            total_stock_count);


    if (!one_minute_valid ||
        !five_minute_valid ||
        !fifteen_minute_valid)
    {
        return false;
    }


    const bool expected_one_minute_ready =
        state.one_minute.
            readiness.
            fully_ready;


    const bool expected_five_minute_ready =
        state.five_minute.
            readiness.
            fully_ready;


    const bool expected_fifteen_minute_ready =
        state.fifteen_minute.
            readiness.
            fully_ready;


    if (state.one_minute_ready !=
        expected_one_minute_ready)
    {
        return false;
    }


    if (state.five_minute_ready !=
        expected_five_minute_ready)
    {
        return false;
    }


    if (state.fifteen_minute_ready !=
        expected_fifteen_minute_ready)
    {
        return false;
    }


    const bool expected_numerical_integrity =
        state.one_minute.
            readiness.
            numerical_integrity &&
        state.five_minute.
            readiness.
            numerical_integrity &&
        state.fifteen_minute.
            readiness.
            numerical_integrity;


    if (state.numerical_integrity !=
        expected_numerical_integrity)
    {
        return false;
    }


    const bool expected_fully_ready =
        expected_numerical_integrity &&
        state.one_minute_ready &&
        state.five_minute_ready &&
        state.fifteen_minute_ready;


    if (state.fully_ready !=
        expected_fully_ready)
    {
        return false;
    }


    return true;
}


// ============================================================================
// Determine one metric's universe size.
//
// Phase 5.1 attaches the SAME metric-specific universe_size to every output
// observation, including unavailable observations.
//
// Therefore every stock must agree on the universe size for a metric.
// ============================================================================

std::size_t
CrossSectionalUniverseSnapshotBuilder::
determineUniverseSize(
    const std::vector<
        CrossSectionalMathematicalState>&
        states,

    const CrossSectionalRank
        CrossSectionalMathematicalHorizon::*
            member,

    const CrossSectionalHorizonState
        CrossSectionalMathematicalState::*
            horizon_member)
{
    if (states.empty())
    {
        return 0;
    }


    bool initialized =
        false;


    std::size_t expected_size =
        0;


    for (const auto& state :
         states)
    {
        const auto& horizon =
            state.*horizon_member;


        const auto& rank =
            horizon.rankings.*member;


        if (!initialized)
        {
            expected_size =
                rank.universe_size;

            initialized =
                true;

            continue;
        }


        if (rank.universe_size !=
            expected_size)
        {
            throw std::runtime_error(
                "Metric-specific universe-size mismatch.");
        }
    }


    return expected_size;
}


// ============================================================================
// Determine all metric-specific universe sizes for one timeframe
// ============================================================================

CrossSectionalMetricUniverseSizes
CrossSectionalUniverseSnapshotBuilder::
determineHorizonUniverseSizes(
    const std::vector<
        CrossSectionalMathematicalState>&
        states,

    const CrossSectionalHorizonState
        CrossSectionalMathematicalState::*
            horizon_member)
{
    CrossSectionalMetricUniverseSizes
        output;


    output.return_value =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                return_value,
            horizon_member);


    output.return_z_score =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                return_z_score,
            horizon_member);


    output.return_robust_z_score =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                return_robust_z_score,
            horizon_member);


    output.volume_z_score =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                volume_z_score,
            horizon_member);


    output.rvol20_z_score =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                rvol20_z_score,
            horizon_member);


    output.relative_return =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                relative_return,
            horizon_member);


    output.residual_return =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                residual_return,
            horizon_member);


    output.residual_z_score =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                residual_z_score,
            horizon_member);


    output.residual_robust_z_score =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                residual_robust_z_score,
            horizon_member);


    output.atr14_percent =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                atr14_percent,
            horizon_member);


    output.realized_volatility_20 =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                realized_volatility_20,
            horizon_member);


    output.rolling_beta =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                rolling_beta,
            horizon_member);


    output.rolling_correlation =
        determineUniverseSize(
            states,
            &CrossSectionalMathematicalHorizon::
                rolling_correlation,
            horizon_member);


    return output;
}


// ============================================================================
// Universe-size validity
// ============================================================================

bool
CrossSectionalUniverseSnapshotBuilder::
validateUniverseSize(
    std::size_t universe_size,
    std::size_t stock_count) const noexcept
{
    if (universe_size >
        stock_count)
    {
        return false;
    }


    // A metric-specific universe smaller than the configured minimum is legal.
    //
    // In that case Phase 5.1 correctly marks the corresponding ranks
    // unavailable.
    //
    // Therefore universe_size = 0 or 1 is not itself an integrity failure.

    return true;
}


// ============================================================================
// Build complete universe snapshot
// ============================================================================

CrossSectionalUniverseSnapshot
CrossSectionalUniverseSnapshotBuilder::
build(
    const std::vector<
        CrossSectionalMathematicalState>&
        states) const
{
    validateIdentity(
        states);


    CrossSectionalUniverseSnapshot
        output;


    if (states.empty())
    {
        // No authoritative universe exists.
        //
        // Return the default unavailable snapshot.

        return output;
    }


    output.decision_time =
        states.front().
            decision_time;


    output.benchmark_symbol =
        states.front().
            benchmark_symbol;


    // ========================================================================
    // Preserve all Phase-5.3 stock state.
    // ========================================================================

    output.stocks =
        states;


    // ========================================================================
    // Deterministic universe ordering
    //
    // Ranking values/ranks are NOT changed.
    // Only the container order is canonicalized.
    // ========================================================================

    std::sort(
        output.stocks.begin(),
        output.stocks.end(),
        [](
            const CrossSectionalMathematicalState& lhs,
            const CrossSectionalMathematicalState& rhs)
        {
            return
                lhs.symbol <
                rhs.symbol;
        });


    output.stock_count =
        output.stocks.size();


    // ========================================================================
    // Validate all Phase-5.3 states
    // ========================================================================

    bool all_states_valid =
        true;


    for (const auto& state :
         output.stocks)
    {
        if (!validateState(
                state,
                output.stock_count))
        {
            all_states_valid =
                false;
        }
    }


    // ========================================================================
    // Metric-specific universe sizes
    // ========================================================================

    output.one_minute_universe_sizes =
        determineHorizonUniverseSizes(
            output.stocks,
            &CrossSectionalMathematicalState::
                one_minute);


    output.five_minute_universe_sizes =
        determineHorizonUniverseSizes(
            output.stocks,
            &CrossSectionalMathematicalState::
                five_minute);


    output.fifteen_minute_universe_sizes =
        determineHorizonUniverseSizes(
            output.stocks,
            &CrossSectionalMathematicalState::
                fifteen_minute);


    // ========================================================================
    // Validate all universe-size values
    // ========================================================================

    const CrossSectionalMetricUniverseSizes*
        universe_groups[] =
    {
        &output.one_minute_universe_sizes,
        &output.five_minute_universe_sizes,
        &output.fifteen_minute_universe_sizes
    };


    bool universe_sizes_valid =
        true;


    for (const auto* group :
         universe_groups)
    {
        const std::size_t sizes[] =
        {
            group->return_value,
            group->return_z_score,
            group->return_robust_z_score,

            group->volume_z_score,
            group->rvol20_z_score,

            group->relative_return,

            group->residual_return,
            group->residual_z_score,
            group->residual_robust_z_score,

            group->atr14_percent,
            group->realized_volatility_20,

            group->rolling_beta,
            group->rolling_correlation
        };


        for (const std::size_t size :
             sizes)
        {
            if (!validateUniverseSize(
                    size,
                    output.stock_count))
            {
                universe_sizes_valid =
                    false;
            }
        }
    }


    // ========================================================================
    // Stock readiness counts
    // ========================================================================

    output.fully_ready_stock_count =
        0;


    for (const auto& state :
         output.stocks)
    {
        if (state.fully_ready)
        {
            ++output.
                fully_ready_stock_count;
        }
    }


    output.partially_ready_stock_count =
        output.stock_count -
        output.fully_ready_stock_count;


    // ========================================================================
    // Universe numerical integrity
    // ========================================================================

    output.numerical_integrity =
        all_states_valid &&
        universe_sizes_valid;


    // ========================================================================
    // Complete universe readiness
    //
    // Full readiness means:
    //
    // - enough stocks exist to form a cross-sectional universe
    // - every stock has complete Phase-5.3 readiness
    // - all structural/numerical integrity checks pass
    //
    // Partial snapshots remain valid and are preserved.
    // ========================================================================

    output.fully_ready =
        output.numerical_integrity &&
        output.stock_count >=
            minimum_universe_size_ &&
        output.fully_ready_stock_count ==
            output.stock_count;


    return output;
}

} // namespace devai::ranking