#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <original_training_cache> <held_out_output.cache>\n";
        return 2;
    }
    try {
        namespace ds = devai::strategy;
        const auto source = ds::DevHistoricalTrainingCache::load(argv[1]);
        ds::DevHistoricalTrainingDatasetValidator::validate(source);
        const auto count = source.records.size();
        if (count < 20) throw std::runtime_error("cache has fewer than 20 records");
        const auto boundary = static_cast<std::size_t>(count * 0.80);
        if (boundary < 10 || boundary + 3 >= count)
            throw std::runtime_error("invalid chronological 80/20 boundary");
        for (std::size_t i = 1; i < count; ++i)
            if (source.records[i].decision_time <= source.records[i-1].decision_time)
                throw std::runtime_error("historical records are not strictly chronological");
        const std::int64_t train_last = source.records[boundary-1].decision_time;
        const std::int64_t hold_first = source.records[boundary].decision_time;
        ds::DevHistoricalTrainingDataset hold{};
        hold.format_version = source.format_version;
        hold.numerical_integrity = source.numerical_integrity;
        hold.records.assign(source.records.begin() + boundary, source.records.end());
        ds::DevHistoricalTrainingDatasetValidator::validate(hold);
        ds::DevHistoricalTrainingCache::save(argv[2], hold);
        std::cout << "Total records: " << count << '\n'
                  << "Training records: " << boundary << '\n'
                  << "Held-out records: " << hold.records.size() << '\n'
                  << "TRAIN_LAST_UNIX_TIMESTAMP=" << train_last << '\n'
                  << "HOLDOUT_FIRST_UNIX_TIMESTAMP=" << hold_first << '\n'
                  << "Held-out cache: " << argv[2] << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Cache split FAILED: " << e.what() << '\n';
        return 1;
    }
}
