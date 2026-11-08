#include "devai/strategy/dev_ai_strategy/DevProductionStatisticalRankingF1.hpp"
#include "devai/strategy/dev_ai_strategy/DevRealHistoricalCacheGenerator.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include "devai/strategy/dev_ai_strategy/HistoricalCETEReplay.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
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
        if (end < start || (end - start) / step > 60)
            throw std::invalid_argument("First validation run must contain at most 61 decision points");

        ds::HistoricalCETEReplayConfig config;
        config.data_folder = argv[4];
        config.benchmark_symbol = "NIFTY%2050";
        config.minimum_universe_size = 2;
        config.maximum_stock_count = 0;
        ds::HistoricalCETEReplay replay(config);

        ds::DevTopCandidateSelector selector(5);
        const ds::DevHistoricalSelectionProvider selection =
            [&selector](const ds::DevHistoricalSnapshot& snapshot) {
                return ds::DevProductionStatisticalRankingF1::select(snapshot, selector);
            };

        std::cout << "Replay folder: " << config.data_folder << '\n';
        std::cout << "Decision times: " << start << " .. " << end
                  << " step " << step << " seconds\n";
        std::cout << "Selection: F1 return-Z statistical baseline (Forecast-OFF; NOT D8 close-Z)\n";
        std::cout << "Historical outcomes: not generated in this smoke test\n";
        std::cout.flush();

        ds::DevRealHistoricalCacheGenerator generator;
        auto result = generator.generateBoundedWithReplayAt(
            replay, start, end, step, selection);
        ds::DevHistoricalTrainingDatasetValidator::validate(result.dataset);
        const std::filesystem::path output(argv[3]);
        if (output.has_parent_path())
            std::filesystem::create_directories(output.parent_path());
        ds::DevHistoricalTrainingCache::save(output, result.dataset);
        const auto verified = ds::DevHistoricalTrainingCache::load(output);
        ds::DevHistoricalTrainingDatasetValidator::validate(verified);
        std::cout << "3.19D-3B-1 PASSED\n"
                  << "Records: " << verified.records.size() << '\n'
                  << "Candidates: " << result.stats.generated_candidate_count << '\n'
                  << "Empty-candidate records: " << result.stats.empty_candidate_records << '\n'
                  << "Saved and reloaded: " << output << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "3.19D-3B-1 FAILED: " << error.what() << '\n';
        return 1;
    }
}
