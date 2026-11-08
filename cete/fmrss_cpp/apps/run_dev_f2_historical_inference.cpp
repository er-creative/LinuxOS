// D9F.4C-F2: read-only historical Top-5 -> shared DevQNetwork inference audit.
#include "devai/strategy/dev_ai_strategy/DevAgent.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevModelPersistence.hpp"
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ds = devai::strategy;

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4)
            throw std::invalid_argument("Usage: run_dev_f2_historical_inference <cache> --random-smoke | <cache> --checkpoint <model_file>");
        const std::string mode = argv[2];
        if ((mode == "--random-smoke" && argc != 3) ||
            (mode == "--checkpoint" && argc != 4) ||
            (mode != "--random-smoke" && mode != "--checkpoint"))
            throw std::invalid_argument("Select --random-smoke OR --checkpoint <model_file>");

        const auto dataset = ds::DevHistoricalTrainingCache::load(argv[1]);
        ds::DevHistoricalTrainingDatasetValidator::validate(dataset);
        ds::DevQNetwork network;
        if (mode == "--checkpoint") {
            const auto metadata = ds::DevModelPersistence{}.load(network, argv[3]);
            if (!metadata.numerical_integrity)
                throw std::runtime_error("Loaded model failed numerical integrity");
            std::cout << "Model: checkpoint loaded (training quality not established)\n";
        } else {
            std::cout << "Model: RANDOM INITIALIZATION; wiring test only, NOT trained inference\n";
        }
        ds::DevAgents agents(network);
        std::size_t evaluated = 0;
        std::int64_t previous_time = 0;
        for (const auto& record : dataset.records) {
            if (record.decision_time <= previous_time || record.benchmark_symbol.empty())
                throw std::runtime_error("Non-chronological or invalid decision record");
            previous_time = record.decision_time;
            if (record.candidates.size() > ds::DEV_AGENT_SLOT_COUNT)
                throw std::runtime_error("More than five candidates");
            ds::DevTopCandidateSelection selection;
            selection.decision_time = record.decision_time;
            selection.benchmark_symbol = record.benchmark_symbol;
            selection.selected_candidate_count = record.candidates.size();
            selection.eligible_candidate_count = record.candidates.size();
            selection.input_candidate_count = record.universe_stock_count;
            selection.numerical_integrity = record.numerical_integrity;
            const auto& names = ds::DevTopCandidateSelector::agentNames();
            std::unordered_set<std::string> seen;
            double previous_score = 0.0;
            for (std::size_t i = 0; i < ds::DEV_AGENT_SLOT_COUNT; ++i) {
                auto& a = selection.assignments[i];
                a.slot_index = i;
                a.agent_name = names[i];
                a.decision_time = record.decision_time;
                if (i >= record.candidates.size()) continue;
                const auto& c = record.candidates[i];
                if (!c.numerical_integrity || c.slot_index != i || c.agent_name != names[i] ||
                    c.candidate_rank != i + 1 || c.symbol.empty() ||
                    c.symbol == record.benchmark_symbol || !seen.insert(c.symbol).second ||
                    !std::isfinite(c.candidate_score) ||
                    (i > 0 && c.candidate_score > previous_score))
                    throw std::runtime_error("Invalid candidate identity, rank or score");
                previous_score = c.candidate_score;
                a.symbol = c.symbol;
                a.candidate_score = c.candidate_score;
                a.candidate_rank = c.candidate_rank;
                a.assigned = true;
            }
            agents.applySelection(selection);
            if (agents.assignedCount() != record.candidates.size())
                throw std::runtime_error("Assigned DevAgent count mismatch");
            std::cout << "\nDecision " << record.decision_time
                      << " | selected " << record.candidates.size() << '\n';
            for (const auto& c : record.candidates) {
                const auto& o = c.observation;
                if (!o.ready || !o.numerical_integrity || o.symbol != c.symbol ||
                    o.benchmark_symbol != record.benchmark_symbol ||
                    o.decision_time != record.decision_time)
                    throw std::runtime_error("Observation readiness/identity mismatch for " + c.symbol);
                for (double x : o.market)
                    if (!std::isfinite(x) || x < -1.00000001 || x > 1.00000001)
                        throw std::runtime_error("Invalid market percentile for " + c.symbol);
                for (double x : o.availability)
                    if (x != 0.0 && x != 1.0)
                        throw std::runtime_error("Invalid availability mask for " + c.symbol);
                const auto report = agents.agent(c.slot_index).evaluate(o);
                if (!report.numerical_integrity || !report.assigned ||
                    report.symbol != c.symbol || report.decision_time != record.decision_time ||
                    report.candidate_rank != c.candidate_rank ||
                    report.candidate_score != c.candidate_score)
                    throw std::runtime_error("DevAgent report mismatch");
                std::cout << "  " << report.agent_name << ' ' << std::setw(12) << std::left
                          << report.symbol << std::right << " rank=" << report.candidate_rank
                          << " F1=" << std::fixed << std::setprecision(6) << report.candidate_score
                          << " Q=[";
                for (std::size_t j = 0; j < report.q_values.values.size(); ++j) {
                    if (j) std::cout << ',';
                    std::cout << report.q_values.values[j];
                }
                std::cout << "] preferred_action=" << static_cast<int>(report.preferred_action) << '\n';
                ++evaluated;
            }
        }
        if (evaluated == 0) throw std::runtime_error("No candidates evaluated");
        std::cout << "\nF2 HISTORICAL SHARED-MODEL INFERENCE AUDIT: PASSED\n"
                  << "Records: " << dataset.records.size() << " | Evaluations: " << evaluated << '\n'
                  << "No training, portfolio changes, or trade execution performed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "F2 HISTORICAL SHARED-MODEL INFERENCE AUDIT: FAILED: " << e.what() << '\n';
        return 1;
    }
}
