// D9F.4C-F3C: read-only, full-session replay audit.
// Uses the existing production historical candle replay; never trains or sends orders.
#include "devai/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using devai::strategy::DevHistoricalTrainingCache;
using devai::strategy::DevHistoricalTrainingDatasetValidator;
using devai::strategy::d9f4c::DevCandleManagerReplay;
using devai::strategy::d9f4c::CandleReplayConfig;

namespace {
void require(bool ok, const std::string& why) {
    if (!ok) throw std::runtime_error("F3C audit: " + why);
}
int localMinute(std::int64_t t) { return DevCandleManagerReplay::minuteIST(t); }
std::string usage() {
    return "Usage: run_dev_f3c_full_session_replay_audit <full_session.cache> "
           "<minute_data_folder> <trained_unified_checkpoint> <output.csv>\n"
           "This command simulates trades in memory only; it never places live orders.\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 5) { std::cerr << usage(); return 2; }
        CandleReplayConfig cfg{};
        cfg.cache_file = argv[1];
        cfg.minute_folder = argv[2];
        cfg.shared_checkpoint = argv[3];
        const fs::path csv = argv[4];
        require(fs::is_regular_file(cfg.cache_file), "cache file does not exist");
        require(fs::is_directory(cfg.minute_folder), "minute-data folder does not exist");
        require(fs::is_regular_file(cfg.shared_checkpoint), "unified checkpoint does not exist");
        require(!csv.empty(), "CSV output path is empty");
        const auto data = DevHistoricalTrainingCache::load(cfg.cache_file);
        DevHistoricalTrainingDatasetValidator::validate(data);
        require(!data.records.empty(), "cache is empty");
        constexpr int session_start = 9*60+16;
        constexpr int session_end = 15*60+15;
        constexpr int entry_cutoff = 14*60+45;
        require(localMinute(data.records.front().decision_time) == session_start,
                "first decision must be 09:16 IST (completed 09:15 candle)");
        require(localMinute(data.records.back().decision_time) == session_end,
                "last decision must be 15:15 IST");
        require(data.records.size() == 360, "expected exactly 360 one-minute decisions");
        const std::int64_t day = data.records.front().decision_time / 86400;
        for (std::size_t i=0; i<data.records.size(); ++i) {
            const auto& r = data.records[i];
            require(r.decision_time / 86400 == day, "cache spans multiple UTC days");
            require(localMinute(r.decision_time) == session_start + static_cast<int>(i),
                    "session has a missing, duplicate, or out-of-order minute");
            require(r.candidates.size() <= 5, "more than five candidates");
        }
        std::cout << "F3C cache preflight: PASS (360 decisions, 09:16-15:15 IST)\n";
        std::cout << "Checkpoint is required; model quality/training is not certified by this audit.\n";
        const auto report = DevCandleManagerReplay::run(cfg);
        require(report.steps.size() == data.records.size(), "replay ended before full session");
        require(report.steps.back().terminal, "last replay step is not terminal");
        require(report.steps.back().held_after.empty(), "position remains at 15:15");
        require(std::isfinite(report.final_equity) && report.final_equity > 0,
                "final equity invalid");
        std::size_t illegal = 0, late_entries = 0, terminal_violations = 0;
        std::size_t candidate_selections = 0;
        for (std::size_t i=0; i<report.steps.size(); ++i) {
            const auto& s = report.steps[i];
            require(s.decision_time == data.records[i].decision_time,
                    "replay/cache decision time mismatch");
            require(s.action < s.state.valid_actions.size(), "action index out of range");
            if (!s.state.valid_actions[s.action]) ++illegal;
            if (s.action >= 2) ++candidate_selections;
            if (localMinute(s.decision_time) >= entry_cutoff &&
                !s.desired.empty() &&
                (i == 0 || s.desired != report.steps[i-1].held_after)) ++late_entries;
            if (localMinute(s.decision_time) >= session_end && !s.held_after.empty())
                ++terminal_violations;
            require(std::isfinite(s.equity_before) && std::isfinite(s.equity_after_mark),
                    "nonfinite replay equity");
        }
        std::cout << "Decisions: " << report.steps.size() << '\n'
                  << "Candidate-selection decisions: " << candidate_selections << '\n'
                  << "Executed ledger legs: " << report.executed_legs << '\n'
                  << "Final simulated equity: " << report.final_equity << '\n'
                  << "Illegal masked actions: " << illegal << '\n'
                  << "New entries/switches at or after 14:45: " << late_entries << '\n'
                  << "Invested after 15:15: " << terminal_violations << '\n';
        require(illegal == 0, "manager selected an action excluded by the replay mask");
        require(late_entries == 0,
                "production replay allowed an entry/switch at or after 14:45; "
                "integrate DevSessionSupervisor into replay before treating F3C as passed");
        require(terminal_violations == 0, "position not flat at deadline");
        if (!csv.parent_path().empty()) fs::create_directories(csv.parent_path());
        DevCandleManagerReplay::writeCsv(csv, report);
        std::cout << "Audit CSV: " << csv << '\n'
                  << "F3C FULL-SESSION HISTORICAL REPLAY AUDIT: PASSED\n"
                  << "Simulation only; no training, broker orders, or live fills.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "F3C FULL-SESSION HISTORICAL REPLAY AUDIT: FAILED\n"
                  << e.what() << '\n';
        return 1;
    }
}
