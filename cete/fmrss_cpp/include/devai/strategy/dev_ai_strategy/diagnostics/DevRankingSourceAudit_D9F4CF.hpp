#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace devai::strategy::diagnostics::d9f4cf {
struct Candidate {
    std::int64_t decision_time{};
    std::string symbol;
    double mean_return{};
    double stddev_return{};
    double forecast_return{};
    bool forecast_enabled{};
    double score{};
    std::int64_t last_input_time{};
    std::int64_t forecast_last_input_time{};
};
struct Report {
    std::size_t rows{}, decisions{}, unique_symbols{}, invalid_numbers{}, future_inputs{}, future_forecasts{}, duplicates{}, incomplete_universe{}, score_ties{}, static_score_decisions{}, static_top5_decisions{}, ranking_mismatches{}, missing_audit_decisions{}, top5_mapping_mismatches{}, top5_score_mismatches{};
    std::vector<std::string> examples;
    bool integrityPass() const { return invalid_numbers==0 && future_inputs==0 && future_forecasts==0 && duplicates==0 && incomplete_universe==0 && ranking_mismatches==0 && top5_mapping_mismatches==0 && top5_score_mismatches==0; }
};
// Input is an exported trace of the ACTUAL ten-stock scorer, not a reconstructed score.
Report inspect(const std::string& universe_csv, const std::string& decision_audit_csv = {}, std::size_t expected_universe = 10);
std::string summary(const Report&);
}
