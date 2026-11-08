#include "devai/strategy/dev_ai_strategy/DevHistoricalSharedRLExecutable_H47_5E2F.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace devai;

struct FixtureModel final : benchmark::IDevRLModel {
    benchmark::DevRLCandidate candidate() const noexcept override {
        return benchmark::DevRLCandidate::SequentialTD;
    }
    std::string_view modelName() const noexcept override { return "fixture-only"; }
    benchmark::DevRLDecision evaluate(const benchmark::DevRLRequest& r) const override {
        std::array<double, benchmark::kOutputCount> values{};
        values[0] = 1.0;
        return benchmark::selectLegalAction(r, values);
    }
};

int main() {
    // 2026-10-16 10:15 IST, valid 30-minute ranking boundary.
    constexpr std::int64_t t = 1792125900LL;
    static_assert(t % 1800 == 900);
    std::size_t calls = 0;
    strategy::DevHistoricalSharedRLExecutable_H47_5E2F runner(
        std::make_shared<FixtureModel>(),
        [&](const std::string&, std::int64_t, std::size_t) {
            ++calls;
            benchmark::DevRLRequest r{};
            r.role = benchmark::DevRLRole::Agent;
            r.legal_actions[0] = true;
            return r;
        },
        [&](std::int64_t time) {
            strategy::DevTopCandidateSelection s{};
            s.decision_time = time;
            s.benchmark_symbol = "NIFTY 50";
            s.numerical_integrity = true;
            s.selected_candidate_count = 5;
            s.eligible_candidate_count = 10;
            s.input_candidate_count = 10;
            for (std::size_t i = 0; i < 5; ++i) {
                auto& a = s.assignments[i];
                a.assigned = true;
                a.slot_index = i;
                a.candidate_rank = i + 1;
                a.decision_time = time;
                a.candidate_score = 5.0 - static_cast<double>(i);
                a.agent_name = strategy::DevTopCandidateSelector::agentNames()[i];
                a.symbol = "STOCK" + std::to_string(i);
            }
            return s;
        });
    strategy::DevHistoricalSharedRLRunConfig_H47_5E2F cfg{};
    cfg.begin_epoch = t;
    cfg.end_epoch = t + 16 * 60;
    cfg.flat_epoch = t + 15 * 60; // Simulated flat boundary; not actual 15:15 IST.
    cfg.agent_csv = std::filesystem::temp_directory_path() / "cete_5e2f_fixture.csv";
    // Fixture flags only: not evidence of a validated production checkpoint.
    cfg.provenance = {"fixture", "fixture", "fixture", "fixture", true, true, true, true};
    const auto result = runner.run(cfg);
    assert(result.minutes == 16 && result.inferred_minutes == 15);
    assert(result.flat_minutes == 1 && result.agent_reports == 75 && calls == 75);
    assert(std::filesystem::exists(cfg.agent_csv));
    std::filesystem::remove(cfg.agent_csv);

    cfg.provenance.checkpoint_validated = false;
    bool rejected = false;
    try { (void)runner.run(cfg); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    std::cout << "Stage 5E.2F 30-minute orchestration PASS (fixture only)\n";
}
