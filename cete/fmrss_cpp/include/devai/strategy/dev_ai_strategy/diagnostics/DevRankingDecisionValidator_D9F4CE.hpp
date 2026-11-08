#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
namespace devai::strategy::d9f4ce {
struct Row {
    long long time{};
    int raw{}, final{}, age{}, legs{};
    std::string held_before, desired, held_after, priority, recommendation;
    std::array<bool,7> legal{};
    std::array<double,7> q{};
    std::array<std::string,5> symbols{};
    std::array<double,5> scores{};
};
struct Summary {
    std::size_t rows{}, raw_action4{}, raw_not_greedy{}, illegal_final{}, forced_exits{},
        advisory_holds{}, candidate_slot_changes{}, score_changes{}, equal_score_rows{},
        missing_candidates{}, entry_cutoff_violations{}, deadline_violations{},
        stagnant_reviews{};
    bool all_scores_constant{true};
    std::vector<std::string> warnings;
};
std::vector<Row> readCsv(const std::filesystem::path& path);
Summary validate(const std::vector<Row>& rows);
std::string report(const Summary& s);
}
