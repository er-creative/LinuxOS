
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace ds = devai::strategy;

namespace {

std::vector<std::string> readDates(const fs::path& manifest) {
    std::ifstream in(manifest);
    if (!in)
        throw std::runtime_error("Cannot open manifest: " + manifest.string());

    std::vector<std::string> dates;
    std::unordered_set<std::string> seen;
    std::string date;

    while (std::getline(in, date)) {
        if (!date.empty() && date.back() == '\r')
            date.pop_back();

        if (date.empty())
            continue;

        if (date.size() != 10 ||
            date[4] != '-' || date[7] != '-' ||
            !std::all_of(date.begin(), date.end(), [](unsigned char c) {
                return (c >= '0' && c <= '9') || c == '-';
            }))
            throw std::runtime_error("Invalid date: " + date);

        if (!seen.insert(date).second)
            throw std::runtime_error("Duplicate date: " + date);

        if (!dates.empty() && date <= dates.back())
            throw std::runtime_error("Manifest is not chronological: " + date);

        dates.push_back(date);
    }

    if (dates.empty())
        throw std::runtime_error("Manifest is empty");

    return dates;
}

ds::DevHistoricalTrainingDataset assemble(
    const fs::path& manifest,
    const fs::path& daily_dir)
{
    const auto dates = readDates(manifest);

    ds::DevHistoricalTrainingDataset combined{};
    combined.numerical_integrity = true;

    std::int64_t last_timestamp = 0;
    std::string benchmark;

    for (const auto& date : dates) {
        const auto path = daily_dir / (date + ".cache");

        auto daily = ds::DevHistoricalTrainingCache::load(path);

        if (daily.records.empty())
            throw std::runtime_error("Empty daily cache: " + date);

        // Daily cache must correspond to one IST calendar date.
        const auto first_day =
            (daily.records.front().decision_time + 19800) / 86400;
        const auto last_day =
            (daily.records.back().decision_time + 19800) / 86400;

        if (first_day != last_day)
            throw std::runtime_error(
                "Cache spans multiple IST dates: " + date);

        if (daily.records.front().decision_time <= last_timestamp)
            throw std::runtime_error(
                "Overlapping or out-of-order cache: " + date);

        if (benchmark.empty())
            benchmark = daily.records.front().benchmark_symbol;

        if (daily.records.front().benchmark_symbol != benchmark)
            throw std::runtime_error(
                "Benchmark changed: " + date);

        for (const auto& record : daily.records) {
            if (record.candidates.size() != ds::DEV_AGENT_SLOT_COUNT)
                throw std::runtime_error(
                    "Incomplete top-five record: " + date);
        }

        last_timestamp = daily.records.back().decision_time;

        const auto count = daily.records.size();

        for (auto& record : daily.records)
            combined.records.push_back(std::move(record));

        std::cout << date << " PASS records=" << count << '\n';
    }

    ds::DevHistoricalTrainingDatasetValidator::validate(combined);

    return combined;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr
            << "Usage: " << argv[0]
            << " <manifest.txt> <daily_cache_dir> <output.cache>\n";
        return 2;
    }

    try {
        const fs::path manifest = argv[1];
        const fs::path daily_dir = argv[2];
        const fs::path output = argv[3];

        auto dataset = assemble(manifest, daily_dir);

        ds::DevHistoricalTrainingCache::save(output, dataset);

        const auto reloaded =
            ds::DevHistoricalTrainingCache::load(output);

        if (reloaded.records.size() != dataset.records.size())
            throw std::runtime_error("Reload record count mismatch");

        std::cout
            << "ASSEMBLY PASSED\n"
            << "Records: " << reloaded.records.size() << '\n'
            << "Output: " << output << '\n';

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ASSEMBLY FAILED: " << e.what() << '\n';
        return 1;
    }
}
