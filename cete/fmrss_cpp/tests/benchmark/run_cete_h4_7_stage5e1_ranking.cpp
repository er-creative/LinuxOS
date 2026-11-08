#include "devai/strategy/dev_ai_strategy/HistoricalCETESinglePassReplay.hpp"
#include "devai/strategy/dev_ai_strategy/DevProductionStatisticalRankingF1.hpp"
#include "devai/strategy/dev_ai_strategy/DevRankingRefreshController_H47_5E1.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: " << argv[0]
                      << " <start_epoch> <end_epoch> <output_prefix>\n";
            return 2;
        }
        const auto start=std::stoll(argv[1]);
        const auto end=std::stoll(argv[2]);
        using namespace devai::strategy;
        if (!DevRankingRefreshController_H47_5E1::isDecisionTime(start) ||
            !DevRankingRefreshController_H47_5E1::isDecisionTime(end) || end < start)
            throw std::invalid_argument("5E1: invalid minute-aligned interval");
        const std::string prefix=argv[3];
        std::ofstream ranking(prefix+"_ranking.csv");
        std::ofstream cycles(prefix+"_cycles.csv");
        if (!ranking || !cycles) throw std::runtime_error("5E1: output file open failed");
        ranking << "decision_time,rank,agent,symbol,statistical_score\n";
        cycles << "decision_time,ranking_time,status,selected_count\n";
        HistoricalCETEReplayConfig cfg{};
        cfg.minimum_universe_size=10; cfg.maximum_stock_count=10;
        HistoricalCETESinglePassReplay replay(cfg);
        DevTopCandidateSelector selector(5);
        DevRankingRefreshController_H47_5E1 controller;
        std::size_t published=0, available_cycles=0, blocked_cycles=0;
        const auto history=replay.run(start,end,60);
        for (const auto& point:history) {
            const auto& snapshot=point.snapshot;
            const auto t=snapshot.decision_time;
            std::string status="RETAINED";
            if (controller.needsRefresh(t)) {
                status="REFRESH_BLOCKED";
                if (point.stats.passed() && snapshot.numerical_integrity &&
                    snapshot.stock_count == 10) {
                    try {
                        auto top=DevProductionStatisticalRankingF1::select(snapshot,selector);
                        controller.publish(top);
                        ++published;
                        status="PUBLISHED";
                        for (const auto& a:top.assignments)
                            ranking << t << ',' << a.candidate_rank << ','
                                    << a.agent_name << ',' << a.symbol << ','
                                    << a.candidate_score << '\n';
                    } catch (const std::exception& e) {
                        std::cerr << "[5E1] refresh blocked at " << t
                                  << ": " << e.what() << '\n';
                    }
                }
            }
            const auto* active=controller.activeAt(t);
            if (active) ++available_cycles;
            else { ++blocked_cycles; if(status=="RETAINED") status="NO_FRESH_RANKING"; }
            cycles << t << ',' << (active ? active->decision_time : 0)
                   << ',' << status << ','
                   << (active ? active->selected_candidate_count : 0) << '\n';
        }
        std::cout << "5E1 published=" << published
                  << " usable_minute_cycles=" << available_cycles
                  << " blocked_minute_cycles=" << blocked_cycles << '\n'
                  << "Ranking CSV: " << prefix << "_ranking.csv\n"
                  << "Cycles CSV: " << prefix << "_cycles.csv\n"
                  << "RL inference: NOT RUN; orders: NOT RUN; 30m labels: NOT RUN\n";
        return published ? 0 : 3;
    } catch(const std::exception& e) {
        std::cerr << "5E1 error: " << e.what() << '\n';
        return 1;
    }
}
