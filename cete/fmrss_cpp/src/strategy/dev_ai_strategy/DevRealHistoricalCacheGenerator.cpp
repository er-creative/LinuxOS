#include "devai/strategy/dev_ai_strategy/DevRealHistoricalCacheGenerator.hpp"

#include "devai/strategy/dev_ai_strategy/ObservationBuilder.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy
{

DevPortfolioState
DevRealHistoricalCacheGenerator::flatPortfolio() noexcept
{
    return {};
}

void
DevRealHistoricalCacheGenerator::validateSnapshots(
    const DevHistoricalSnapshotSequence& snapshots)
{
    if (snapshots.empty())
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: snapshot sequence cannot be empty.");
    }

    std::int64_t previous_time = 0;
    std::string benchmark;

    for (const auto& snapshot : snapshots)
    {
        if (snapshot.decision_time <= 0)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: snapshot decision time must be positive.");
        }

        if (previous_time != 0 && snapshot.decision_time <= previous_time)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: snapshots must be strictly chronological.");
        }

        if (snapshot.benchmark_symbol.empty())
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: snapshot benchmark cannot be empty.");
        }

        if (benchmark.empty())
        {
            benchmark = snapshot.benchmark_symbol;
        }
        else if (snapshot.benchmark_symbol != benchmark)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: benchmark changed inside snapshot sequence.");
        }

        if (!snapshot.numerical_integrity)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: snapshot failed numerical integrity.");
        }

        if (snapshot.stock_count != snapshot.stocks.size())
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: snapshot stock count mismatch.");
        }

        std::unordered_set<std::string> symbols;
        for (const auto& stock : snapshot.stocks)
        {
            if (stock.symbol.empty() ||
                stock.symbol == snapshot.benchmark_symbol ||
                stock.decision_time != snapshot.decision_time ||
                !symbols.insert(stock.symbol).second)
            {
                throw std::invalid_argument(
                    "DevRealHistoricalCacheGenerator: invalid stock identity in snapshot.");
            }
        }

        previous_time = snapshot.decision_time;
    }
}

void
DevRealHistoricalCacheGenerator::validateSelection(
    const DevTopCandidateSelection& selection,
    const DevHistoricalSnapshot& snapshot)
{
    if (!selection.numerical_integrity)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: selection failed numerical integrity.");
    }

    if (selection.decision_time != snapshot.decision_time ||
        selection.benchmark_symbol != snapshot.benchmark_symbol)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: selection/snapshot identity mismatch.");
    }

    if (selection.selected_candidate_count > DEV_AGENT_SLOT_COUNT ||
        selection.eligible_candidate_count < selection.selected_candidate_count)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: invalid candidate counts.");
    }

    const auto& names = DevTopCandidateSelector::agentNames();
    std::unordered_set<std::string> selected_symbols;
    std::size_t assigned_count = 0;

    for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i)
    {
        const auto& assignment = selection.assignments[i];

        if (assignment.slot_index != i ||
            assignment.agent_name != names[i] ||
            assignment.decision_time != snapshot.decision_time)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: invalid DevAgent assignment identity.");
        }

        const bool should_be_assigned =
            i < selection.selected_candidate_count;

        if (assignment.assigned != should_be_assigned)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: assignments must form a contiguous prefix.");
        }

        if (!assignment.assigned)
        {
            continue;
        }

        ++assigned_count;

        if (assignment.symbol.empty() ||
            assignment.symbol == snapshot.benchmark_symbol ||
            !std::isfinite(assignment.candidate_score) ||
            assignment.candidate_rank != i + 1U)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: invalid assigned candidate.");
        }

        if (!selected_symbols.insert(assignment.symbol).second)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: duplicate selected candidate symbol.");
        }

        bool found = false;
        for (const auto& stock : snapshot.stocks)
        {
            if (stock.symbol == assignment.symbol)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            throw std::invalid_argument(
                "DevRealHistoricalCacheGenerator: selected symbol absent from CETE snapshot.");
        }
    }

    if (assigned_count != selection.selected_candidate_count)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: selected-candidate count mismatch.");
    }
}

DevRealHistoricalCacheGenerationResult
DevRealHistoricalCacheGenerator::generateWithObservationProvider(
    const DevHistoricalSnapshotSequence& snapshots,
    const DevHistoricalSelectionProvider& selection_provider,
    const DevHistoricalObservationProvider& observation_provider,
    const DevHistoricalPortfolioProvider& portfolio_provider,
    const DevHistoricalOutcomeProvider& outcome_provider) const
{
    validateSnapshots(snapshots);

    if (!selection_provider)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: selection provider is required.");
    }

    if (!observation_provider)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: observation provider is required.");
    }

    DevRealHistoricalCacheGenerationResult result;
    result.stats.input_snapshot_count = snapshots.size();
    result.dataset.records.reserve(snapshots.size());

    for (const auto& snapshot : snapshots)
    {
        const auto selection = selection_provider(snapshot);
        validateSelection(selection, snapshot);

        DevHistoricalDecisionRecord record;
        record.decision_time = snapshot.decision_time;
        record.benchmark_symbol = snapshot.benchmark_symbol;
        record.universe_stock_count = snapshot.stock_count;
        record.candidates.reserve(selection.selected_candidate_count);

        for (std::size_t i = 0;
             i < selection.selected_candidate_count;
             ++i)
        {
            const auto& assignment = selection.assignments[i];

            const DevPortfolioState portfolio =
                portfolio_provider
                    ? portfolio_provider(snapshot, assignment.symbol)
                    : flatPortfolio();

            DevHistoricalCandidateRecord candidate;
            candidate.slot_index = assignment.slot_index;
            candidate.agent_name = assignment.agent_name;
            candidate.symbol = assignment.symbol;
            candidate.candidate_score = assignment.candidate_score;
            candidate.candidate_rank = assignment.candidate_rank;
            candidate.observation = observation_provider(
                snapshot,
                assignment.symbol,
                portfolio);

            if (outcome_provider)
            {
                candidate.outcomes = outcome_provider(
                    snapshot,
                    assignment.symbol);
            }

            candidate.numerical_integrity = true;

            result.stats.outcome_count += candidate.outcomes.size();
            for (const auto& outcome : candidate.outcomes)
            {
                if (outcome.available)
                {
                    ++result.stats.available_outcome_count;
                }
            }

            record.candidates.push_back(std::move(candidate));
            ++result.stats.generated_candidate_count;
        }

        if (record.candidates.empty())
        {
            ++result.stats.empty_candidate_records;
        }

        record.numerical_integrity = true;
        result.dataset.records.push_back(std::move(record));
        ++result.stats.generated_record_count;
    }

    result.dataset.numerical_integrity = true;
    DevHistoricalTrainingDatasetValidator::validate(result.dataset);
    result.stats.numerical_integrity = true;
    return result;
}

DevRealHistoricalCacheGenerationResult
DevRealHistoricalCacheGenerator::generate(
    const DevHistoricalSnapshotSequence& snapshots,
    const DevHistoricalSelectionProvider& selection_provider,
    const DevHistoricalPortfolioProvider& portfolio_provider,
    const DevHistoricalOutcomeProvider& outcome_provider) const
{
    ObservationBuilder builder;

    return generateWithObservationProvider(
        snapshots,
        selection_provider,
        [&builder](const DevHistoricalSnapshot& snapshot,
                   const std::string& symbol,
                   const DevPortfolioState& portfolio)
        {
            return builder.build(snapshot, symbol, portfolio);
        },
        portfolio_provider,
        outcome_provider);
}

DevRealHistoricalCacheGenerationResult
DevRealHistoricalCacheGenerator::generateBoundedWithReplayAt(
    const HistoricalCETEReplay& replay,
    std::int64_t start_decision_time,
    std::int64_t end_decision_time,
    std::int64_t step_seconds,
    const DevHistoricalSelectionProvider& selection_provider,
    const DevHistoricalPortfolioProvider& portfolio_provider,
    const DevHistoricalOutcomeProvider& outcome_provider) const
{
    if (start_decision_time <= 0 ||
        end_decision_time <= 0 ||
        start_decision_time > end_decision_time ||
        step_seconds <= 0)
    {
        throw std::invalid_argument(
            "DevRealHistoricalCacheGenerator: invalid bounded replay interval.");
    }

    DevHistoricalSnapshotSequence snapshots;

    for (std::int64_t t = start_decision_time;
         t <= end_decision_time;)
    {
        const auto point = replay.replayAt(t);

        if (!point.stats.passed())
        {
            throw std::runtime_error(
                "DevRealHistoricalCacheGenerator: replayAt failed CETE integrity validation.");
        }

        if (point.snapshot.decision_time != t)
        {
            throw std::runtime_error(
                "DevRealHistoricalCacheGenerator: replayAt returned another decision time.");
        }

        snapshots.push_back(point.snapshot);

        if (t > std::numeric_limits<std::int64_t>::max() - step_seconds)
        {
            break;
        }
        t += step_seconds;
    }

    return generate(
        snapshots,
        selection_provider,
        portfolio_provider,
        outcome_provider);
}

} // namespace devai::strategy
