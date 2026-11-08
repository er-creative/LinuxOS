// D9F.4C-F3E-D1 — isolated 0/1/3/5/10-minute holding counterfactual.
// Research-only frozen-model replay: no training, checkpoint writes or live orders.
#include "devai/strategy/dev_ai_strategy/reward/DevCandleManagerReplay_D9F4C.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using devai::strategy::d9f4c::CandleReplayConfig;
using devai::strategy::d9f4c::CandleReplayReport;
using devai::strategy::d9f4c::DevCandleManagerReplay;

namespace {
constexpr double kCapital = 7000.0;
constexpr std::array<int, 5> kPeriods{0, 1, 3, 5, 10};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string name(int minutes) {
    return minutes == 0 ? "baseline" : std::to_string(minutes) + "_minute";
}

struct Stats {
    int minutes{};
    std::size_t decisions{}, opens{}, switches{}, holds{}, exits{}, waits{};
    std::size_t legs{}, risk_exits{}, terminal_exits{}, early_violations{};
    std::size_t invalid_actions{}, late_entries{};
    double gross_rewards{}, net_rewards{}, execution_costs{};
    double direct_pnl{}, final_equity{}, return_pct{};
    double summed_equity_deltas{}, continuity_gap{}, reward_gap{};
    double max_drawdown{}, max_drawdown_pct{};
    bool equity_continuity_ok{}, reward_reconciliation_ok{};
};

Stats analyze(const CandleReplayReport& report, int minutes) {
    Stats x{};
    x.minutes = minutes;
    x.decisions = report.steps.size();
    x.legs = report.executed_legs;
    x.final_equity = report.final_equity;
    x.direct_pnl = x.final_equity - kCapital;
    x.return_pct = 100.0 * x.direct_pnl / kCapital;
    require(x.decisions != 0 && x.decisions == report.audit.size(),
            name(minutes) + ": step/audit count mismatch");
    require(report.steps.back().terminal && report.steps.back().held_after.empty(),
            name(minutes) + ": missing terminal flat position");
    require(std::isfinite(x.final_equity) && x.final_equity > 0,
            name(minutes) + ": invalid final equity");

    double previous_mark = kCapital;
    double peak = kCapital;
    double total_gap = 0.0;
    double total_reward_gap = 0.0;
    for (std::size_t i = 0; i < x.decisions; ++i) {
        const auto& step = report.steps[i];
        const auto& audit = report.audit[i];
        require(step.decision_time == audit.decision_time,
                name(minutes) + ": step/audit timestamp mismatch");
        require(step.action < step.state.valid_actions.size() &&
                    step.state.valid_actions[step.action],
                name(minutes) + ": illegal selected action");
        require(std::isfinite(step.equity_before) &&
                    std::isfinite(step.equity_after_mark) &&
                    std::isfinite(step.reward.net_reward) &&
                    std::isfinite(step.reward.gross_reward) &&
                    std::isfinite(step.reward.execution_costs),
                name(minutes) + ": nonfinite ledger value");

        const bool before = !audit.held_before.empty();
        const bool after = !audit.held_after.empty();
        if (!before && after) ++x.opens;
        else if (before && after && audit.held_before != audit.held_after) ++x.switches;
        else if (before && !after) ++x.exits;
        else if (before) ++x.holds;
        else ++x.waits;

        const bool changed = after && audit.held_before != audit.held_after;
        const int local_minute = DevCandleManagerReplay::minuteIST(step.decision_time);
        if (local_minute >= 14 * 60 + 45 && changed) ++x.late_entries;
        if (step.terminal && after) ++x.invalid_actions;
        if (before && !after && audit.supervisor_recommendation == "EXIT_REQUIRED")
            ++x.risk_exits;
        if (step.terminal && before && !after) ++x.terminal_exits;

        const bool exempt = step.terminal ||
            audit.supervisor_recommendation == "EXIT_REQUIRED";
        const bool early_exit = before && !after && !exempt;
        const bool early_switch = before && after &&
            audit.held_before != audit.held_after && !exempt;
        if (minutes > 0 && audit.holding_age_minutes < minutes &&
            (early_exit || early_switch)) ++x.early_violations;

        const double delta = step.equity_after_mark - step.equity_before;
        x.summed_equity_deltas += delta;
        x.gross_rewards += step.reward.gross_reward;
        x.net_rewards += step.reward.net_reward;
        x.execution_costs += step.reward.execution_costs;
        total_gap += std::abs(step.equity_before - previous_mark);
        total_reward_gap += std::abs(delta - step.reward.net_reward);
        previous_mark = step.equity_after_mark;
        peak = std::max(peak, previous_mark);
        x.max_drawdown = std::max(x.max_drawdown, peak - previous_mark);
        if (peak > 0) x.max_drawdown_pct =
            std::max(x.max_drawdown_pct, 100.0 * (peak - previous_mark) / peak);
    }
    x.continuity_gap = total_gap;
    x.reward_gap = x.direct_pnl - x.net_rewards;
    // Mark-to-market equity is the authoritative cash-P&L measure; model reward
    // fields may have different units. Report discrepancies, never relabel them
    // as a validated rupee P&L.
    x.equity_continuity_ok = total_gap < 1e-5 &&
        std::abs(x.direct_pnl - x.summed_equity_deltas) < 1e-5;
    x.reward_reconciliation_ok = std::abs(x.reward_gap) < 1e-5 &&
        total_reward_gap < 1e-5;
    require(x.late_entries == 0, name(minutes) + ": late entry/switch");
    require(x.invalid_actions == 0, name(minutes) + ": terminal/illegal action");
    require(x.early_violations == 0, name(minutes) + ": premature ordinary exit/switch");
    require(x.equity_continuity_ok, name(minutes) + ": equity marks do not telescope");
    return x;
}

void writeComparison(const fs::path& output, const std::vector<Stats>& stats) {
    std::ofstream f(output);
    require(static_cast<bool>(f), "cannot create comparison CSV");
    f << std::setprecision(17)
      << "holding_minutes,decisions,opens,switches,holds,exits,waits,executed_legs,"
         "final_equity,direct_net_pnl,return_pct,gross_reward_sum,net_reward_sum,"
         "execution_cost_sum,max_drawdown,max_drawdown_pct,"
         "risk_exits,terminal_exits,early_violations,late_entries,"
         "equity_continuity_ok,reward_reconciliation_ok,equity_pnl_minus_reward_gap\n";
    for (const auto& x : stats) {
        f << x.minutes << ',' << x.decisions << ',' << x.opens << ','
          << x.switches << ',' << x.holds << ',' << x.exits << ','
          << x.waits << ',' << x.legs << ',' << x.final_equity << ','
          << x.direct_pnl << ',' << x.return_pct << ',' << x.gross_rewards << ','
          << x.net_rewards << ',' << x.execution_costs << ','
          << x.max_drawdown << ',' << x.max_drawdown_pct << ','
          << x.risk_exits << ',' << x.terminal_exits << ','
          << x.early_violations << ',' << x.late_entries << ','
          << (x.equity_continuity_ok ? 1 : 0) << ','
          << (x.reward_reconciliation_ok ? 1 : 0) << ','
          << x.reward_gap << '\n';
    }
    require(static_cast<bool>(f), "comparison CSV write failed");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            std::cerr << "Usage: run_dev_f3d1_multi_holding_counterfactual "
                         "<cache> <minute_folder> <checkpoint> <output_directory>\n";
            return 2;
        }
        CandleReplayConfig cfg{};
        cfg.cache_file = argv[1];
        cfg.minute_folder = argv[2];
        cfg.shared_checkpoint = argv[3];
        const fs::path out = argv[4];
        require(fs::is_regular_file(cfg.cache_file), "cache missing");
        require(fs::is_directory(cfg.minute_folder), "minute-data folder missing");
        require(fs::is_regular_file(cfg.shared_checkpoint), "checkpoint missing");
        require(!fs::exists(out) || fs::is_directory(out), "output is not directory");

        // Refuse any overwrite, including companion decision-audit CSVs.
        std::vector<fs::path> outputs{out / "comparison_0_1_3_5_10.csv"};
        for (int minutes : kPeriods) {
            const auto base = name(minutes);
            outputs.push_back(out / (base + ".csv"));
            outputs.push_back(out / (base + "_decision_audit.csv"));
        }
        for (const auto& p : outputs)
            require(!fs::exists(p), "refusing to overwrite: " + p.string());
        fs::create_directories(out);

        std::vector<Stats> all;
        all.reserve(kPeriods.size());
        std::cout << std::fixed << std::setprecision(6);
        for (int minutes : kPeriods) {
            cfg.minimum_holding_minutes = minutes;
            std::cout << "Running minimum hold " << minutes << " minute(s)...\n" << std::flush;
            const auto report = DevCandleManagerReplay::run(cfg);
            const auto stats = analyze(report, minutes);
            if (minutes == 0) {
                require(stats.decisions == 360, "baseline decision count changed");
                require(stats.switches == 281, "baseline switch count changed");
                require(std::abs(stats.final_equity - 4600.602415) < 1e-3,
                        "baseline final equity changed");
            }
            // Save each validated replay separately, including its action audit.
            DevCandleManagerReplay::writeCsv(out / (name(minutes) + ".csv"), report);
            all.push_back(stats);
            std::cout << "Hold " << minutes << "m: switches=" << stats.switches
                      << " costs=" << stats.execution_costs
                      << " net_PnL=" << stats.direct_pnl
                      << " final_equity=" << stats.final_equity
                      << " reward_gap=" << stats.reward_gap << '\n';
            if (!stats.reward_reconciliation_ok)
                std::cerr << "WARNING " << minutes << "m: reward fields do not telescope "
                             "to rupee equity P&L. Inspect reward accounting; "
                             "equity P&L is reported separately.\n";
        }
        writeComparison(out / "comparison_0_1_3_5_10.csv", all);
        const auto best = std::max_element(all.begin(), all.end(),
            [](const Stats& a, const Stats& b) { return a.final_equity < b.final_equity; });
        std::cout << "Best IN-SAMPLE final equity: " << best->minutes << " minute(s), "
                  << best->final_equity << " INR\n"
                  << "F3E-D1 MULTI-HOLD COUNTERFACTUAL: PASS (equity checks)\n"
                  << "WARNING: One-session ranking is not proof of profitability. "
                     "Validate on unseen sessions before selecting a rule.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "F3E-D1 MULTI-HOLD COUNTERFACTUAL: FAILED: " << e.what() << '\n';
        return 1;
    }
}
