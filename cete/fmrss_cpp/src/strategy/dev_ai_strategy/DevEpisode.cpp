#include "devai/strategy/dev_ai_strategy/DevEpisode.hpp"

#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy
{

// ============================================================================
// Constructor
// ============================================================================

DevEpisode::DevEpisode(
    std::vector<Snapshot> snapshots,
    DevEpisodeConfig config)
    :
    config_(config),
    snapshots_(std::move(snapshots))
{
    if (config_.minimum_universe_size < 2)
    {
        throw std::invalid_argument(
            "DevEpisode minimum universe size must be at least 2.");
    }


    validateSnapshots();


    benchmark_symbol_ =
        snapshots_.front().benchmark_symbol;


    reset();
}


// ============================================================================
// Snapshot validation
// ============================================================================

void
DevEpisode::validateSnapshots() const
{
    if (snapshots_.empty())
    {
        throw std::invalid_argument(
            "DevEpisode requires at least one CETE snapshot.");
    }


    const std::string& expected_benchmark =
        snapshots_.front().benchmark_symbol;


    if (expected_benchmark.empty())
    {
        throw std::invalid_argument(
            "DevEpisode benchmark symbol cannot be empty.");
    }


    std::int64_t previous_decision_time = 0;


    for (std::size_t index = 0;
         index < snapshots_.size();
         ++index)
    {
        const Snapshot& snapshot =
            snapshots_[index];


        // --------------------------------------------------------------------
        // Numerical integrity
        // --------------------------------------------------------------------

        if (!snapshot.numerical_integrity)
        {
            throw std::invalid_argument(
                "DevEpisode received a CETE snapshot "
                "without numerical integrity.");
        }


        // --------------------------------------------------------------------
        // Valid decision time
        // --------------------------------------------------------------------

        if (snapshot.decision_time <= 0)
        {
            throw std::invalid_argument(
                "DevEpisode snapshot decision time must be positive.");
        }


        // --------------------------------------------------------------------
        // Strict chronological progression
        // --------------------------------------------------------------------

        if (index > 0 &&
            snapshot.decision_time <= previous_decision_time)
        {
            throw std::invalid_argument(
                "DevEpisode snapshots must be strictly chronological.");
        }


        // --------------------------------------------------------------------
        // Benchmark identity must remain constant
        // --------------------------------------------------------------------

        if (snapshot.benchmark_symbol != expected_benchmark)
        {
            throw std::invalid_argument(
                "DevEpisode benchmark symbol changed inside episode.");
        }


        // --------------------------------------------------------------------
        // Minimum universe
        // --------------------------------------------------------------------

        if (snapshot.stock_count <
            config_.minimum_universe_size)
        {
            throw std::invalid_argument(
                "DevEpisode snapshot contains fewer stocks "
                "than the required minimum universe size.");
        }


        // --------------------------------------------------------------------
        // Snapshot stock-count consistency
        // --------------------------------------------------------------------

        if (snapshot.stock_count !=
            snapshot.stocks.size())
        {
            throw std::invalid_argument(
                "DevEpisode snapshot stock_count does not "
                "match snapshot.stocks size.");
        }


        // --------------------------------------------------------------------
        // Stock identity / time integrity
        // --------------------------------------------------------------------

        std::unordered_set<std::string>
            seen_symbols;


        for (const auto& stock :
             snapshot.stocks)
        {
            if (stock.symbol.empty())
            {
                throw std::invalid_argument(
                    "DevEpisode snapshot contains empty stock symbol.");
            }


            if (stock.symbol ==
                snapshot.benchmark_symbol)
            {
                throw std::invalid_argument(
                    "DevEpisode benchmark cannot appear as "
                    "a tradable stock.");
            }


            if (stock.decision_time !=
                snapshot.decision_time)
            {
                throw std::invalid_argument(
                    "DevEpisode stock decision time does not "
                    "match universe snapshot decision time.");
            }


            if (!seen_symbols.insert(
                    stock.symbol).second)
            {
                throw std::invalid_argument(
                    "DevEpisode snapshot contains duplicate "
                    "stock symbols.");
            }
        }


        previous_decision_time =
            snapshot.decision_time;
    }
}


// ============================================================================
// Reset
// ============================================================================

void
DevEpisode::reset()
{
    if (snapshots_.empty())
    {
        throw std::logic_error(
            "DevEpisode cannot reset an empty episode.");
    }


    current_index_ = 0;

    initialized_ = true;
}


// ============================================================================
// Advance
// ============================================================================

void
DevEpisode::advance()
{
    if (!initialized_)
    {
        throw std::logic_error(
            "DevEpisode is not initialized.");
    }


    if (isTerminal())
    {
        throw std::out_of_range(
            "DevEpisode cannot advance beyond terminal state.");
    }


    const std::int64_t previous_time =
        snapshots_[current_index_].
            decision_time;


    ++current_index_;


    const std::int64_t current_time =
        snapshots_[current_index_].
            decision_time;


    if (current_time <=
        previous_time)
    {
        throw std::logic_error(
            "DevEpisode chronological integrity violated.");
    }
}


// ============================================================================
// Current snapshot
// ============================================================================

const DevEpisode::Snapshot&
DevEpisode::currentSnapshot() const
{
    if (!initialized_)
    {
        throw std::logic_error(
            "DevEpisode is not initialized.");
    }


    if (current_index_ >=
        snapshots_.size())
    {
        throw std::out_of_range(
            "DevEpisode current snapshot index is invalid.");
    }


    return snapshots_[current_index_];
}


// ============================================================================
// Current decision time
// ============================================================================

std::int64_t
DevEpisode::currentDecisionTime() const
{
    return currentSnapshot().
        decision_time;
}


// ============================================================================
// Step index
// ============================================================================

std::size_t
DevEpisode::stepIndex() const noexcept
{
    return current_index_;
}


// ============================================================================
// Total steps
// ============================================================================

std::size_t
DevEpisode::totalSteps() const noexcept
{
    return snapshots_.size();
}


// ============================================================================
// Initialized
// ============================================================================

bool
DevEpisode::isInitialized() const noexcept
{
    return initialized_;
}


// ============================================================================
// Terminal
//
// The final available historical snapshot is the terminal episode state.
// No snapshot after this point may be requested.
// ============================================================================

bool
DevEpisode::isTerminal() const noexcept
{
    if (!initialized_ ||
        snapshots_.empty())
    {
        return false;
    }


    return current_index_ + 1 >=
           snapshots_.size();
}


// ============================================================================
// Start decision time
// ============================================================================

std::int64_t
DevEpisode::startDecisionTime() const noexcept
{
    if (snapshots_.empty())
    {
        return 0;
    }


    return snapshots_.front().
        decision_time;
}


// ============================================================================
// End decision time
// ============================================================================

std::int64_t
DevEpisode::endDecisionTime() const noexcept
{
    if (snapshots_.empty())
    {
        return 0;
    }


    return snapshots_.back().
        decision_time;
}


// ============================================================================
// Benchmark symbol
// ============================================================================

const std::string&
DevEpisode::benchmarkSymbol() const noexcept
{
    return benchmark_symbol_;
}


// ============================================================================
// State
// ============================================================================

DevEpisodeState
DevEpisode::state() const noexcept
{
    DevEpisodeState output;

    output.step_index =
        current_index_;

    output.total_steps =
        snapshots_.size();

    output.start_decision_time =
        startDecisionTime();

    output.end_decision_time =
        endDecisionTime();

    output.initialized =
        initialized_;

    output.terminal =
        isTerminal();


    if (initialized_ &&
        current_index_ <
            snapshots_.size())
    {
        output.current_decision_time =
            snapshots_[current_index_].
                decision_time;
    }


    return output;
}

} // namespace devai::strategy