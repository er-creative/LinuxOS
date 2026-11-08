#include "devai/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace devai::strategy::d9f4c;
namespace fs = std::filesystem;

static void require(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

int main() {
    fs::path output;
    fs::path diagnostic;
    try {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        output = fs::temp_directory_path() / ("cete_d9f4c_audit_test_" + std::to_string(stamp) + ".csv");
        diagnostic = output.parent_path() / (output.stem().string() + "_decision_audit.csv");

        CandleReplayReport report{};
        CandleReplayStep step{};
        step.decision_time = 1790049600;
        step.next_decision_time = step.decision_time + 60;
        step.action = 4;
        step.desired = "BHARTIARTL";
        step.held_after = "BHARTIARTL";
        step.equity_before = 7000.0;
        step.equity_after_mark = 6995.78;
        step.reward.execution_costs = 4.02;
        report.steps.push_back(step);

        CandleDecisionAudit audit{};
        audit.decision_time = step.decision_time;
        audit.raw_action = 4;
        audit.final_action = 4;
        audit.legal_actions = {true, false, true, true, true, true, true};
        audit.q_values = {0.1, 0.2, 0.3, 0.4, 0.9, 0.5, 0.6};
        audit.candidate_symbols[2] = "BHARTIARTL";
        audit.candidate_scores[2] = 1.25;
        audit.desired = "BHARTIARTL";
        audit.held_after = "BHARTIARTL";
        audit.supervisor_priority = "NORMAL";
        audit.supervisor_recommendation = "HOLD";
        audit.executed_legs = 1;
        report.audit.push_back(audit);

        DevCandleManagerReplay::writeCsv(output, report);
        require(fs::exists(output), "missing execution CSV");
        require(fs::exists(diagnostic), "missing diagnostic CSV");
        std::ifstream in(diagnostic);
        std::string header, row, extra;
        require(static_cast<bool>(std::getline(in, header)), "missing audit header");
        require(static_cast<bool>(std::getline(in, row)), "missing audit record");
        require(!static_cast<bool>(std::getline(in, extra)), "unexpected extra audit record");
        for (const auto* field : {"raw_action", "final_action", "legal_4", "q_4", "slot_2_symbol", "supervisor_recommendation"})
            require(header.find(field) != std::string::npos, "missing required audit field");
        require(row.find("BHARTIARTL") != std::string::npos, "missing selected symbol");
        require(row.find(",4,4,") != std::string::npos, "raw/final action not serialized");
        require(row.find(",0.90000000000000002,") != std::string::npos || row.find(",0.9,") != std::string::npos,
                "Q-value not serialized");
        fs::remove(output);
        fs::remove(diagnostic);
        std::cout << "D9F4C_DECISION_AUDIT_PASS csv=1 actions=1 legal_mask=1 q_values=1 candidate_mapping=1\n";
        return 0;
    } catch (const std::exception& e) {
        if (!output.empty()) fs::remove(output);
        if (!diagnostic.empty()) fs::remove(diagnostic);
        std::cerr << "D9F4C_DECISION_AUDIT_FAIL " << e.what() << '\n';
        return 1;
    }
}
