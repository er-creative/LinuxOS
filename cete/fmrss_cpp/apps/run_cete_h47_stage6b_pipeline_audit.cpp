// H4.7 5E.2K.6B — read-only historical cache / mathematical input audit.
// Offline outcomes are written ONLY to audit CSV, never to a decision policy.
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevAIObservation.hpp"
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ds = devai::strategy;

static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: run_cete_h47_stage6b_pipeline_audit <historical.cache> <audit.csv>\n";
            return 2;
        }
        const auto dataset = ds::DevHistoricalTrainingCache::load(argv[1]);
        ds::DevHistoricalTrainingDatasetValidator::validate(dataset);
        check(!dataset.records.empty(), "empty historical dataset");
        const std::filesystem::path output(argv[2]);
        if (output.has_parent_path()) std::filesystem::create_directories(output.parent_path());
        std::ofstream csv(output);
        check(static_cast<bool>(csv), "cannot create audit CSV");
        csv << "decision_time,slot,symbol,rank,candidate_score,"
               "return_z_percentile_1m,return_z_percentile_5m,return_z_percentile_15m,"
               "available_1m,available_5m,available_15m,"
               "observation_ready,observation_integrity,"
               "outcome_horizon_bars,offline_forward_return,offline_outcome_available\n";
        csv << std::setprecision(17);
        std::size_t candidates = 0, available = 0, outcomes = 0;
        for (const auto& record : dataset.records) {
            for (const auto& candidate : record.candidates) {
                const auto& o = candidate.observation;
                const auto i1 = ds::marketIndex(ds::DevTimeframe::ONE_MINUTE, ds::DevMetric::RETURN_Z_SCORE);
                const auto i5 = ds::marketIndex(ds::DevTimeframe::FIVE_MINUTE, ds::DevMetric::RETURN_Z_SCORE);
                const auto i15 = ds::marketIndex(ds::DevTimeframe::FIFTEEN_MINUTE, ds::DevMetric::RETURN_Z_SCORE);
                check(candidate.symbol == o.symbol, "candidate/observation symbol mismatch");
                check(record.decision_time == o.decision_time, "candidate/observation time mismatch");
                check(std::isfinite(candidate.candidate_score), "nonfinite candidate score");
                for (auto idx : {i1, i5, i15}) {
                    check(std::isfinite(o.market[idx]), "nonfinite percentile");
                    check(o.market[idx] >= -1.0 && o.market[idx] <= 1.0, "percentile outside [-1,1]");
                    check(o.availability[idx] == 0.0 || o.availability[idx] == 1.0, "invalid availability flag");
                }
                ++candidates;
                if (o.availability[i1] && o.availability[i5] && o.availability[i15]) ++available;
                const auto emit = [&](std::size_t horizon, double forward, bool has_outcome) {
                    csv << record.decision_time << ',' << candidate.slot_index << ','
                        << candidate.symbol << ',' << candidate.candidate_rank << ','
                        << candidate.candidate_score << ',' << o.market[i1] << ','
                        << o.market[i5] << ',' << o.market[i15] << ','
                        << o.availability[i1] << ',' << o.availability[i5] << ','
                        << o.availability[i15] << ',' << (o.ready ? 1 : 0) << ','
                        << (o.numerical_integrity ? 1 : 0) << ',' << horizon << ','
                        << forward << ',' << (has_outcome ? 1 : 0) << '\n';
                };
                if (candidate.outcomes.empty()) emit(0, 0.0, false);
                for (const auto& outcome : candidate.outcomes) {
                    const bool usable = outcome.available && outcome.numerical_integrity;
                    if (usable) {
                        check(std::isfinite(outcome.forward_return), "nonfinite forward outcome");
                        check(outcome.outcome_time > record.decision_time, "noncausal outcome timestamp");
                        ++outcomes;
                    }
                    emit(outcome.horizon_bars, usable ? outcome.forward_return : 0.0, usable);
                }
            }
        }
        check(static_cast<bool>(csv), "failed writing CSV");
        std::cout << "[CETE][H47][5E2K6B] pipeline audit PASS\n"
                  << "Records: " << dataset.records.size() << " Candidates: " << candidates
                  << " Complete 1m/5m/15m Z-percentile availability: " << available
                  << " Usable offline outcomes: " << outcomes << '\n'
                  << "Audit: " << output.string() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[CETE][H47][5E2K6B] pipeline audit FAIL: " << e.what() << '\n';
        return 1;
    }
}
