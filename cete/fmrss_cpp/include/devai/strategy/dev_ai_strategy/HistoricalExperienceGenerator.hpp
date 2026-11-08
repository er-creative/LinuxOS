#pragma once

#include "devai/strategy/dev_ai_strategy/DevAgentEpisodeRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"
#include "devai/strategy/dev_ai_strategy/DevPolicy.hpp"
#include "devai/strategy/dev_ai_strategy/DevReplayBuffer.hpp"
#include "devai/strategy/dev_ai_strategy/TradingEnvironment.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>


namespace devai::strategy
{

// ============================================================================
// HistoricalMarketPrice
//
// Causal market price visible at one historical decision time.
//
// IMPORTANT:
//
// This is NOT a future price.
//
// For CETE 1-minute historical data:
//
//     decision_time = completed candle timestamp + 60 seconds
//
// Therefore:
//
//     market_price = close of candle timestamped decision_time - 60
//
// The price belongs to the current state S_t.
// ============================================================================

struct HistoricalMarketPrice
{
    std::int64_t decision_time{0};

    double market_price{0.0};
};


// ============================================================================
// HistoricalExperienceGenerationResult
//
// Summary of one generated historical episode.
//
// No learning occurs here.
// No neural-network update occurs here.
// No future-return target is calculated here.
// ============================================================================

struct HistoricalExperienceGenerationResult
{
    std::string symbol;

    std::int64_t episode_start_time{0};

    std::int64_t episode_end_time{0};

    std::size_t episode_state_count{0};

    std::size_t transitions_generated{0};

    std::size_t replay_size_before{0};

    std::size_t replay_size_after{0};

    std::size_t transitions_stored{0};

    bool terminal_reached{false};

    bool numerical_integrity{false};
};


// ============================================================================
// HistoricalExperienceGenerator
//
// Phase 3.6 responsibility:
//
//     historical CETE states
//              +
//     causal historical prices
//              ↓
//     DevAgentEpisodeRunner
//              ↓
//     DevTransition
//              ↓
//     DevReplayBuffer
//
// This class deliberately DOES NOT:
//
//   - calculate CETE features
//   - calculate cross-sectional rankings
//   - calculate observations independently
//   - calculate reward independently
//   - calculate brokerage independently
//   - calculate slippage independently
//   - calculate P&L independently
//   - calculate ForwardReturn_N
//   - train Dev
//   - update neural-network weights
//
// Those responsibilities already belong elsewhere.
// ============================================================================

class HistoricalExperienceGenerator
{
public:
    HistoricalExperienceGenerator(
        std::string symbol,
        DevEpisode& episode,
        TradingEnvironment& environment,
        DevPolicy& policy,
        DevReplayBuffer& replay_buffer);


    [[nodiscard]]
    HistoricalExperienceGenerationResult
    generate(
        std::int64_t environment_reset_time,
        double environment_reset_price,
        const std::vector<HistoricalMarketPrice>& market_prices);


    [[nodiscard]]
    const std::string&
    symbol() const noexcept;


private:
    static void
    validateResetState(
        std::int64_t environment_reset_time,
        double environment_reset_price);


    static void
    validateMarketPrices(
        const std::vector<HistoricalMarketPrice>& market_prices);


    std::string symbol_;

    DevEpisode& episode_;

    TradingEnvironment& environment_;

    DevPolicy& policy_;

    DevReplayBuffer& replay_buffer_;
};


} // namespace devai::strategy