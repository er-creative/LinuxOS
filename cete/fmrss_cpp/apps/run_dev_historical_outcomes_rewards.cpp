#include "devai/strategy/dev_ai_strategy/DevHistoricalOutcomeRewardValidator.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
namespace ds = devai::strategy;
int main(int argc, char** argv) {
    if (argc < 5 || argc > 7) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.cache> <output.cache> <reward_report.csv> <minute_folder>"
                  << " [cost_bps_per_side=5] [slippage_bps_per_side=2]\n";
        return 2;
    }
    try {
        ds::DevOutcomeRewardConfig config;
        config.minute_data_folder = argv[4];
        if (argc >= 6) config.transaction_cost_bps_per_side = std::stod(argv[5]);
        if (argc >= 7) config.slippage_bps_per_side = std::stod(argv[6]);
        const auto input = std::filesystem::path(argv[1]);
        const auto output = std::filesystem::path(argv[2]);
        if (std::filesystem::absolute(input) == std::filesystem::absolute(output))
            throw std::invalid_argument("Input and output cache must be different files");
        std::cout << "Loading baseline cache: " << input << std::endl;
        auto dataset = ds::DevHistoricalTrainingCache::load(input);
        ds::DevHistoricalTrainingDatasetValidator::validate(dataset);
        ds::DevHistoricalOutcomeRewardValidator validator(config);
        std::cout << "Computing offline outcomes and counterfactual rewards..." << std::endl;
        const auto stats = validator.enrich(dataset, argv[3]);
        if (output.has_parent_path()) std::filesystem::create_directories(output.parent_path());
        ds::DevHistoricalTrainingCache::save(output, dataset);
        const auto verified = ds::DevHistoricalTrainingCache::load(output);
        ds::DevHistoricalTrainingDatasetValidator::validate(verified);
        if (verified.records.size() != dataset.records.size())
            throw std::runtime_error("Cache round-trip record count mismatch");
        std::cout << "3.19D-3B-3 OFFLINE OUTCOME VALIDATION PASSED\n"
                  << "Records: " << stats.records << "\nCandidates: " << stats.candidates
                  << "\nAvailable targets: " << stats.available
                  << "\nUnavailable targets: " << stats.unavailable
                  << "\nRejected gaps: " << stats.rejected_gaps
                  << "\nRejected cross-session: " << stats.rejected_session
                  << "\nRejected missing future data: " << stats.rejected_history
                  << "\nEnriched cache: " << output
                  << "\nCounterfactual reward CSV: " << argv[3] << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "3.19D-3B-3 FAILED: " << e.what() << '\n';
        return 1;
    }
}
