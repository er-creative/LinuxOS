#include "devai/strategy/dev_ai_strategy/HistoricalCETESinglePassReplay.hpp"
#include "devai/strategy/dev_ai_strategy/DevProductionStatisticalRankingF1.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: " << argv[0]
                      << " <start_decision_epoch> <end_decision_epoch> <output.csv>\n";
            return 2;
        }
        const std::int64_t start = std::stoll(argv[1]);
        const std::int64_t end = std::stoll(argv[2]);
        if (start <= 0 || end < start || start % 60 || end % 60)
            throw std::invalid_argument("Use chronological minute-aligned epoch timestamps");

        devai::strategy::HistoricalCETEReplayConfig cfg{};
        cfg.data_folder =
            "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/"
            "kite_downloads/minute";
        cfg.five_minute_data_folder =
            "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/"
            "kite_downloads/5minutes";
        cfg.fifteen_minute_data_folder =
            "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/"
            "kite_downloads/15minutes";
        cfg.benchmark_symbol = "NIFTY%2050";
        cfg.minimum_universe_size = 10;
        cfg.maximum_stock_count = 10;

        devai::strategy::HistoricalCETESinglePassReplay replay(cfg);
        devai::strategy::DevTopCandidateSelector selector(5);
        std::ofstream csv(argv[3]);
        if (!csv) throw std::runtime_error("Cannot open CSV output");
        csv << "decision_time,rank,agent,symbol,statistical_score\n";

        std::size_t points = 0, complete = 0, selected = 0;
        const auto history = replay.run(start, end, 60);
        for (const auto& point : history) {
            ++points;
            const auto& snapshot = point.snapshot;
            if (!point.stats.passed() || !snapshot.numerical_integrity ||
                snapshot.stock_count != 10) continue;
            ++complete;
            const auto top = devai::strategy::DevProductionStatisticalRankingF1::
                select(snapshot, selector);
            if (!top.numerical_integrity) continue;
            for (const auto& slot : top.assignments) {
                if (!slot.assigned) continue;
                csv << snapshot.decision_time << ',' << slot.candidate_rank << ','
                    << slot.agent_name << ',' << slot.symbol << ','
                    << slot.candidate_score << '\n';
                ++selected;
            }
        }
        std::cout << "H4.7 Stage 4 production F1 ranking\n"
                  << "Replay points: " << points << "\n"
                  << "Integrity-passing snapshots: " << complete << "\n"
                  << "Candidate assignments: " << selected << "\n"
                  << "Output: " << argv[3] << "\n"
                  << "RL inference: NOT RUN (requires actual DevManagerEncodedState)\n"
                  << "D9F.4C paper ledger: NOT RUN (requires price provider)\n";
        return selected ? 0 : 3;
    } catch (const std::exception& e) {
        std::cerr << "Stage4 error: " << e.what() << '\n';
        return 1;
    }
}
