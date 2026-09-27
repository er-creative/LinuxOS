#include "devai/statistics/ReturnStatisticalEngine.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::statistics
{

// ============================================================
// HorizonState
// ============================================================

ReturnStatisticalEngine::HorizonState::HorizonState(
    const std::string& symbol,
    std::size_t window_size)
    : return_statistics(
          symbol,
          window_size),
      absolute_return_statistics(
          symbol,
          window_size)
{
}


// ============================================================
// Constructor
// ============================================================

ReturnStatisticalEngine::ReturnStatisticalEngine(
    std::string symbol,
    std::size_t window_size,
    double extreme_z_threshold)
    : symbol_(std::move(symbol)),
      window_size_(window_size),
      extreme_z_threshold_(extreme_z_threshold),
      one_minute_(symbol_, window_size_),
      five_minute_(symbol_, window_size_),
      fifteen_minute_(symbol_, window_size_)
{
    if (symbol_.empty())
    {
        throw std::invalid_argument(
            "ReturnStatisticalEngine symbol cannot be empty.");
    }

    if (window_size_ == 0)
    {
        throw std::invalid_argument(
            "ReturnStatisticalEngine window size must be greater than zero.");
    }

    if (!std::isfinite(extreme_z_threshold_) ||
        extreme_z_threshold_ <= 0.0)
    {
        throw std::invalid_argument(
            "ReturnStatisticalEngine extreme Z threshold must be positive and finite.");
    }
}


// ============================================================
// Accessors
// ============================================================

const std::string&
ReturnStatisticalEngine::symbol() const noexcept
{
    return symbol_;
}


std::size_t
ReturnStatisticalEngine::windowSize() const noexcept
{
    return window_size_;
}


double
ReturnStatisticalEngine::extremeZThreshold() const noexcept
{
    return extreme_z_threshold_;
}


// ============================================================
// Reset
//
// Explicit reset only.
//
// Trading-session boundaries DO NOT reset the engine.
// ============================================================

void ReturnStatisticalEngine::reset() noexcept
{
    one_minute_.return_statistics.reset();
    one_minute_.absolute_return_statistics.reset();
    one_minute_.last_source_timestamp.reset();
    one_minute_.latest = {};

    five_minute_.return_statistics.reset();
    five_minute_.absolute_return_statistics.reset();
    five_minute_.last_source_timestamp.reset();
    five_minute_.latest = {};

    fifteen_minute_.return_statistics.reset();
    fifteen_minute_.absolute_return_statistics.reset();
    fifteen_minute_.last_source_timestamp.reset();
    fifteen_minute_.latest = {};

    last_decision_time_.reset();
}


// ============================================================
// Extreme classification
//
// This is descriptive statistical state, NOT a trade signal.
//
// |Z| < threshold       -> NORMAL
// Z >= threshold        -> EXTREME_POSITIVE
// Z <= -threshold       -> EXTREME_NEGATIVE
//
// Intermediate POSITIVE/NEGATIVE represents sign context.
// ============================================================

StatisticalExtremeState
ReturnStatisticalEngine::classifyExtreme(
    double return_value,
    double z_score,
    double extreme_threshold) noexcept
{
    if (!std::isfinite(return_value) ||
        !std::isfinite(z_score))
    {
        return StatisticalExtremeState::UNAVAILABLE;
    }

    if (z_score >= extreme_threshold)
    {
        return StatisticalExtremeState::EXTREME_POSITIVE;
    }

    if (z_score <= -extreme_threshold)
    {
        return StatisticalExtremeState::EXTREME_NEGATIVE;
    }

    if (return_value > 0.0)
    {
        return StatisticalExtremeState::POSITIVE;
    }

    if (return_value < 0.0)
    {
        return StatisticalExtremeState::NEGATIVE;
    }

    return StatisticalExtremeState::NORMAL;
}


// ============================================================
// Copy ordinary-return statistics
// ============================================================

void ReturnStatisticalEngine::copyStatistics(
    const RollingStatisticalFeatures& source,
    ReturnHorizonStatistics& destination)
{
    destination.return_value =
        source.value;

    destination.rolling_mean =
        source.rolling_mean;

    destination.rolling_standard_deviation =
        source.rolling_standard_deviation;

    destination.z_score =
        source.z_score;

    destination.rolling_median =
        source.rolling_median;

    destination.rolling_mad =
        source.rolling_mad;

    destination.robust_z_score =
        source.robust_z_score;

    destination.observation_count =
        source.observation_count;

    destination.has_observation =
        source.has_value;

    destination.ready =
        source.ready;
}


// ============================================================
// Copy absolute-return statistics
// ============================================================

void ReturnStatisticalEngine::copyAbsoluteStatistics(
    const RollingStatisticalFeatures& source,
    ReturnHorizonStatistics& destination)
{
    destination.absolute_return =
        source.value;

    destination.absolute_return_mean =
        source.rolling_mean;

    destination.absolute_return_standard_deviation =
        source.rolling_standard_deviation;

    destination.absolute_return_z_score =
        source.z_score;

    destination.absolute_return_median =
        source.rolling_median;

    destination.absolute_return_mad =
        source.rolling_mad;

    destination.absolute_return_robust_z_score =
        source.robust_z_score;
}


// ============================================================
// Update one horizon
// ============================================================

void ReturnStatisticalEngine::updateHorizon(
    HorizonState& state,
    bool has_observation,
    std::optional<std::int64_t> source_timestamp,
    std::int64_t decision_time,
    double return_value)
{
    if (!has_observation ||
        !source_timestamp.has_value() ||
        !std::isfinite(return_value))
    {
        return;
    }

    // --------------------------------------------------------
    // Repeated source candle
    //
    // 5m and 15m snapshots remain visible during subsequent
    // 1m decisions. They must not enter the rolling window
    // repeatedly.
    // --------------------------------------------------------

    if (state.last_source_timestamp.has_value())
    {
        if (*source_timestamp <
            *state.last_source_timestamp)
        {
            throw std::runtime_error(
                "ReturnStatisticalEngine source timestamp moved backwards.");
        }

        if (*source_timestamp ==
            *state.last_source_timestamp)
        {
            return;
        }
    }

    // --------------------------------------------------------
    // Ordinary return statistics
    // --------------------------------------------------------

    const auto return_statistics =
        state.return_statistics.update(
            decision_time,
            return_value);


    // --------------------------------------------------------
    // Absolute-return statistics
    // --------------------------------------------------------

    const double absolute_return =
        std::abs(return_value);

    const auto absolute_statistics =
        state.absolute_return_statistics.update(
            decision_time,
            absolute_return);


    // --------------------------------------------------------
    // Persist latest output
    // --------------------------------------------------------

    copyStatistics(
        return_statistics,
        state.latest);

    copyAbsoluteStatistics(
        absolute_statistics,
        state.latest);

    state.latest.extreme_state =
        classifyExtreme(
            return_value,
            return_statistics.z_score,
            extreme_z_threshold_);

    state.last_source_timestamp =
        source_timestamp;
}


// ============================================================
// Main update
// ============================================================

ReturnStatisticalFeatures
ReturnStatisticalEngine::update(
    const devai::features::PriceReturnFeatures& features,
    const devai::market::MarketSnapshot& snapshot)
{
    // --------------------------------------------------------
    // Symbol integrity
    // --------------------------------------------------------

    if (features.symbol != symbol_)
    {
        throw std::invalid_argument(
            "ReturnStatisticalEngine PriceReturnFeatures symbol mismatch.");
    }

    if (snapshot.symbol != symbol_)
    {
        throw std::invalid_argument(
            "ReturnStatisticalEngine MarketSnapshot symbol mismatch.");
    }


    // --------------------------------------------------------
    // Decision-time integrity
    // --------------------------------------------------------

    if (features.decision_time !=
        snapshot.decision_time)
    {
        throw std::invalid_argument(
            "ReturnStatisticalEngine decision-time mismatch.");
    }

    if (last_decision_time_.has_value() &&
        features.decision_time <
            *last_decision_time_)
    {
        throw std::runtime_error(
            "ReturnStatisticalEngine decision time moved backwards.");
    }


    // --------------------------------------------------------
    // Extract source timestamps
    // --------------------------------------------------------

    std::optional<std::int64_t>
        one_minute_timestamp;

    std::optional<std::int64_t>
        five_minute_timestamp;

    std::optional<std::int64_t>
        fifteen_minute_timestamp;


    if (snapshot.one_minute.has_value())
    {
        one_minute_timestamp =
            snapshot.one_minute->timestamp;
    }

    if (snapshot.five_minute.has_value())
    {
        five_minute_timestamp =
            snapshot.five_minute->timestamp;
    }

    if (snapshot.fifteen_minute.has_value())
    {
        fifteen_minute_timestamp =
            snapshot.fifteen_minute->timestamp;
    }


    // --------------------------------------------------------
    // 1-minute return
    // --------------------------------------------------------

    updateHorizon(
        one_minute_,
        features.has_one_minute &&
            std::isfinite(
                features.one_minute_return),
        one_minute_timestamp,
        features.decision_time,
        features.one_minute_return);


    // --------------------------------------------------------
    // 5-minute return
    // --------------------------------------------------------

    updateHorizon(
        five_minute_,
        features.has_five_minute &&
            std::isfinite(
                features.five_minute_return),
        five_minute_timestamp,
        features.decision_time,
        features.five_minute_return);


    // --------------------------------------------------------
    // 15-minute return
    // --------------------------------------------------------

    updateHorizon(
        fifteen_minute_,
        features.has_fifteen_minute &&
            std::isfinite(
                features.fifteen_minute_return),
        fifteen_minute_timestamp,
        features.decision_time,
        features.fifteen_minute_return);


    // --------------------------------------------------------
    // Build result
    // --------------------------------------------------------

    ReturnStatisticalFeatures result;

    result.symbol =
        symbol_;

    result.decision_time =
        features.decision_time;

    result.one_minute =
        one_minute_.latest;

    result.five_minute =
        five_minute_.latest;

    result.fifteen_minute =
        fifteen_minute_.latest;

    result.one_minute_ready =
        result.one_minute.ready;

    result.five_minute_ready =
        result.five_minute.ready;

    result.fifteen_minute_ready =
        result.fifteen_minute.ready;

    result.fully_ready =
        result.one_minute_ready &&
        result.five_minute_ready &&
        result.fifteen_minute_ready;


    last_decision_time_ =
        features.decision_time;

    return result;
}

} // namespace devai::statistics