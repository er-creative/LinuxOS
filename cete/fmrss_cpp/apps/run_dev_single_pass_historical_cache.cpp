#include "devai/strategy/dev_ai_strategy/DevProductionStatisticalRankingF1.hpp"
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalCacheGenerator.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include "devai/strategy/dev_ai_strategy/DevRankingRefreshController_H47_5E1.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETESinglePassReplay.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace ds = devai::strategy;
namespace dr = devai::ranking;

static std::int64_t positiveInteger(const char* value, const char* name)
{
    std::size_t used = 0;
    const std::string input(value);
    const auto number = std::stoll(input, &used);
    if (used != input.size() || number <= 0)
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    return number;
}

int main(int argc, char** argv)
{
    if (argc != 5 && argc != 6)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <start_epoch_seconds> <end_epoch_seconds> <output.cache>"
                  << " <minute_data_folder> [step_seconds=60]\n"
                  << "Times are Unix epoch seconds (UTC-based), not local wall-clock numbers.\n"
                  << "Use only completed trading-minute decision timestamps.\n";
        return 2;
    }

    try
    {
        const auto start = positiveInteger(argv[1], "start");
        const auto end = positiveInteger(argv[2], "end");
        const auto step = argc == 6 ? positiveInteger(argv[5], "step") : 60;
        if (end < start || (end - start) / step > 10000)
            throw std::invalid_argument("Maximum 10001 decision points per run");

        ds::HistoricalCETEReplayConfig config;
        config.data_folder = argv[4];
        config.benchmark_symbol = "NIFTY%2050";
        config.minimum_universe_size = 2;
        config.maximum_stock_count = 0;
        ds::HistoricalCETESinglePassReplay replay(config);

        std::cerr << "[DEBUG] Before selector construction"
          << " | slots=" << ds::DEV_AGENT_SLOT_COUNT
          << '\n';

        ds::DevTopCandidateSelector selector(5);

        std::cerr << "[DEBUG] After selector construction"
                  << " | selected=" << selector.maximumSelected()
                  << '\n';
        ds::DevRankingRefreshController_H47_5E1 ranking_controller;
        const ds::DevHistoricalSelectionProvider selection =
            [&selector, &ranking_controller](const ds::DevHistoricalSnapshot& snapshot)
            {
                const auto t = snapshot.decision_time;
                if (ranking_controller.needsRefresh(t)) {
                    ranking_controller.publish(
                        ds::DevProductionStatisticalRankingF1::select(snapshot, selector));
                }
                const auto* active = ranking_controller.activeAt(t);
                if (!active)
                    throw std::runtime_error(
                        "Missing valid 30-minute ranking at decision epoch " +
                        std::to_string(t));
                auto current = *active;
                current.decision_time = t;
                for (auto& assignment : current.assignments)
                    assignment.decision_time = t;
                return current;
            };

        std::cout << "Replay folder: " << config.data_folder << '\n';
        std::cout << "Decision times: " << start << " .. " << end
                  << " step " << step << " seconds\n";
        std::cout << "Selection: F1 return-Z statistical baseline (Forecast-OFF; NOT D8 close-Z)\n";
        std::cout << "Historical outcomes: not generated in this smoke test\n";
        std::cout.flush();

        ds::DevRealHistoricalCacheGenerator generator;
        if (step != 60)
            throw std::invalid_argument("30-minute ranking cache requires 60-second decisions");
        if (!ds::DevRankingRefreshController_H47_5E1::isSessionTime(start) ||
            !ds::DevRankingRefreshController_H47_5E1::isSessionTime(end) ||
            ds::DevRankingRefreshController_H47_5E1::localDay(start) !=
                ds::DevRankingRefreshController_H47_5E1::localDay(end))
            throw std::invalid_argument("Replay must lie within one IST trading session");
        // Reconstruct the true prior refresh when the requested interval starts mid-block.
        const auto warmup_start =
            ds::DevRankingRefreshController_H47_5E1::rankingEpoch(start);
        const auto points = replay.run(warmup_start, end, step,

            [](std::size_t completed, std::size_t total, const std::string& symbol) {
                std::cout << "[" << completed << "/" << total << "] "
                          << symbol << std::endl;
            });
        ds::DevHistoricalSnapshotSequence snapshots;
        snapshots.reserve(points.size());
        bool ranking_window_available = false;
        for (const auto& point : points) {
            if (!point.stats.passed())
                throw std::runtime_error("Single-pass replay integrity failed");
            if (point.snapshot.decision_time < start) {
                if (point.snapshot.decision_time == warmup_start) {
                    ranking_controller.publish(
                        ds::DevProductionStatisticalRankingF1::select(
                            point.snapshot, selector));
                    ranking_window_available = true;
                }
                continue;
            }
            // An unavailable opening refresh (typically 09:15) must not
            // fabricate a top-five selection. Wait until the next valid
            // scheduled 30-minute boundary before producing training records.
            const auto t = point.snapshot.decision_time;
            const bool refresh_boundary =
                ds::DevRankingRefreshController_H47_5E1::rankingEpoch(t) == t;
            if (refresh_boundary)
                ranking_window_available = true;
            if (!ranking_window_available) {
                std::cerr << "[RANKING][WAIT] decision_time=" << t
                          << " reason=no_valid_prior_refresh\n";
                continue;
            }
            snapshots.push_back(point.snapshot);
        }
        if (snapshots.empty())
            throw std::runtime_error(
                "No eligible snapshots after waiting for a valid 30-minute ranking boundary");
        // Optional expensive regression against validated 3.19D-3B-1 snapshot.
        if (std::getenv("CETE_COMPARE_BASELINE") && !points.empty()) {
            const ds::HistoricalCETEReplay baseline(config);
            const auto reference = baseline.replayAt(points.front().snapshot.decision_time);
            const auto& a = reference.snapshot;
            const auto& b = points.front().snapshot;
            if (a.decision_time != b.decision_time ||
                a.benchmark_symbol != b.benchmark_symbol ||
                a.stock_count != b.stock_count ||
                a.stocks.size() != b.stocks.size())
                throw std::runtime_error("Baseline snapshot mismatch");
            for (std::size_t i = 0; i < a.stocks.size(); ++i)
                if (a.stocks[i].symbol != b.stocks[i].symbol ||
                    a.stocks[i].decision_time != b.stocks[i].decision_time ||
                    a.stocks[i].one_minute_ready != b.stocks[i].one_minute_ready ||
                    a.stocks[i].five_minute_ready != b.stocks[i].five_minute_ready ||
                    a.stocks[i].fifteen_minute_ready != b.stocks[i].fifteen_minute_ready)
                    throw std::runtime_error("Baseline stock/readiness mismatch");
            std::cout << "Baseline structural comparison: PASSED\n";
        }
        auto result = generator.generate(snapshots, selection);
        ds::DevHistoricalTrainingDatasetValidator::validate(result.dataset);
        const std::filesystem::path output(argv[3]);
        if (output.has_parent_path())
            std::filesystem::create_directories(output.parent_path());
        ds::DevHistoricalTrainingCache::save(output, result.dataset);
        const auto verified = ds::DevHistoricalTrainingCache::load(output);
        ds::DevHistoricalTrainingDatasetValidator::validate(verified);
        // F1 audit: print the persisted values, not recomputed scores.
        // Rank is an ordering metric; it is not a BUY probability.
        std::cout << "\nF1 TOP-5 HISTORICAL CANDIDATE AUDIT\n";
        for (const auto& record : verified.records)
        {
            std::cout << "Decision epoch: " << record.decision_time
                      << " | benchmark: " << record.benchmark_symbol
                      << " | universe: " << record.universe_stock_count << '\n';
            std::cout << "Rank  Agent         Symbol          F1 score\n";
            double previous_score = std::numeric_limits<double>::infinity();
            std::size_t expected_slot = 0;
            for (const auto& candidate : record.candidates)
            {
                if (!std::isfinite(candidate.candidate_score))
                    throw std::runtime_error("F1 audit: nonfinite candidate score");
                if (expected_slot >= 5 || candidate.slot_index != expected_slot)
                    throw std::runtime_error("F1 audit: invalid agent slot ordering");
                const std::size_t expected_rank = expected_slot + 1U;
                if (candidate.candidate_rank != expected_rank)
                    throw std::runtime_error("F1 audit: incorrect rank-to-agent mapping");

                if (expected_slot < 4 &&
                    candidate.candidate_score > previous_score + 1e-12)
                    throw std::runtime_error("F1 audit: top-four scores not descending");
                std::cout << std::setw(4) << candidate.candidate_rank << "  "
                          << std::left << std::setw(13) << candidate.agent_name
                          << std::setw(16) << candidate.symbol << std::right
                          << std::fixed << std::setprecision(8)
                          << candidate.candidate_score << '\n';
                if (expected_slot < 4) previous_score = candidate.candidate_score;
                ++expected_slot;
            }
            std::cout << '\n';
        }
        std::cout << "F1 persisted candidate ordering audit: PASSED\n";
        std::cout << "3.19D-3B-2 PASSED\n"
                  << "Records: " << verified.records.size() << '\n'
                  << "Candidates: " << result.stats.generated_candidate_count << '\n'
                  << "Empty-candidate records: " << result.stats.empty_candidate_records << '\n'
                  << "Saved and reloaded: " << output << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "3.19D-3B-2 FAILED: " << error.what() << '\n';
        return 1;
    }
}
