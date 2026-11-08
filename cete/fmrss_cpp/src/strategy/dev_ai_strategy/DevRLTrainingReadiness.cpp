#include "devai/strategy/dev_ai_strategy/DevRLTrainingReadiness.hpp"
#include <array>
#include <cmath>
#include <set>
#include <string>

namespace devai::readiness {
Report validate(const std::vector<TrainingDecision>& decisions,
                std::int64_t training_end_exclusive,
                double reward_tolerance) {
    Report report{};
    const auto fail = [&](std::size_t i, const std::string& message) {
        report.errors.push_back("decision[" + std::to_string(i) + "]: " + message);
    };
    if (training_end_exclusive <= 0) report.errors.push_back("invalid training cutoff");
    if (!std::isfinite(reward_tolerance) || reward_tolerance < 0)
        report.errors.push_back("invalid reward tolerance");
    if (decisions.empty()) report.errors.push_back("no training decisions");
    std::int64_t previous = 0;
    const std::array<std::string,5> agents = {
        "DevAgent_001", "DevAgent_002", "DevAgent_003", "DevAgent_004", "DevAgent_005"};
    for (std::size_t i=0; i<decisions.size(); ++i) {
        const auto& d = decisions[i];
        ++report.checked_decisions;
        if (d.manager_id != "DevManager_001") fail(i, "unexpected manager ID");
        if (d.decision_time <= previous || d.decision_time <= 0)
            fail(i, "non-increasing decision timestamp");
        if (d.next_decision_time <= d.decision_time ||
            d.next_decision_time >= training_end_exclusive)
            fail(i, "transition crosses training cutoff or has invalid next timestamp");
        if (i+1 < decisions.size() && d.next_decision_time != decisions[i+1].decision_time)
            fail(i, "next timestamp does not match next decision");
        if (i+1 < decisions.size() && d.terminal)
            fail(i, "terminal flag set before last decision");
        if (i+1 == decisions.size() && !d.terminal)
            fail(i, "last decision must be terminal");
        if (d.candidates.empty() || d.candidates.size() > 5)
            fail(i, "candidate count must be 1 to 5");
        std::set<std::string> symbols;
        for (std::size_t j=0; j<d.candidates.size(); ++j) {
            const auto& c = d.candidates[j];
            if (c.slot != j || c.slot >= agents.size() ||
                (c.slot < agents.size() && c.agent_id != agents[c.slot]))
                fail(i, "candidate slot/agent mismatch");
            if (c.symbol.empty() || !symbols.insert(c.symbol).second)
                fail(i, "empty or duplicate candidate symbol");
            if (!c.causal_observation || !c.forecast_from_past_only ||
                c.has_realized_forward_return_in_observation)
                fail(i, "non-causal observation or leaked forward return");
        }
        if (!d.held_symbol.empty() && !d.held_position_visible)
            fail(i, "held position not visible to manager");
        if (!d.manager_action_valid) fail(i, "invalid manager action");
        if (!d.supervisor_authorized) fail(i, "supervisor authorization absent");
        if (!(std::isfinite(d.initial_equity) && d.initial_equity > 0 &&
              std::isfinite(d.equity_before) && d.equity_before > 0 &&
              std::isfinite(d.equity_after) && d.equity_after > 0 &&
              std::isfinite(d.manager_reward))) {
            fail(i, "non-finite or non-positive portfolio accounting");
        } else {
            const double expected = (d.equity_after-d.equity_before)/d.initial_equity;
            if (std::abs(d.manager_reward-expected) >
                reward_tolerance*(1.0+std::abs(expected)))
                fail(i, "manager reward differs from supervisor equity reward");
        }
        if (i>0 && std::isfinite(decisions[i-1].equity_after) &&
            std::isfinite(d.equity_before) &&
            std::abs(decisions[i-1].equity_after-d.equity_before) >
                1e-8*(1.0+std::abs(d.equity_before)))
            fail(i, "equity discontinuity between decisions");
        previous = d.decision_time;
    }
    report.ready = report.errors.empty();
    return report;
}
} // namespace devai::readiness
