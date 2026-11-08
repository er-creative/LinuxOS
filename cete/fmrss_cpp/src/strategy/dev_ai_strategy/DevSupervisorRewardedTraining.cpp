#include "devai/strategy/dev_ai_strategy/DevSupervisorRewardedTraining.hpp"
#include <cmath>
#include <stdexcept>
namespace devai::strategy {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
}
void DevSupervisorRewardedTraining::validate(
    const std::vector<DevRewardedTrainingSample>& samples,
    std::int64_t cutoff) {
    require(cutoff > 0, "training cutoff must be positive");
    require(!samples.empty(), "empty training experiences");
    std::int64_t last = 0;
    std::size_t agent_count = 0;
    DevTransitionValidator agent_validator;
    for (const auto& s : samples) {
        require(s.manager_id == "DevManager_001", "only DevManager_001 supported");
        const auto& h = s.historical;
        require(h.decision_time > last, "non-monotonic historical timestamps");
        require(h.next_decision_time > h.decision_time &&
                h.next_decision_time < cutoff, "experience crosses training boundary");
        require(std::isfinite(h.portfolio_reward), "nonfinite supervisor reward");
        require(std::isfinite(h.equity_before) && std::isfinite(h.equity_after) &&
                h.equity_before > 0 && h.equity_after > 0, "invalid supervisor equity");
        require(s.manager_reward_from_supervisor, "manager reward not supervisor sourced");
        const auto& m = s.manager;
        DevManagerTrainer::validateTransition(m);
        require(m.decision_time == h.decision_time &&
                m.next_decision_time == h.next_decision_time &&
                m.terminal == h.terminal, "manager/historical transition mismatch");
        require(std::abs(m.reward - h.portfolio_reward) <=
                1e-10 * (1.0 + std::abs(h.portfolio_reward)),
                "manager reward must equal supervisor portfolio reward");
        require(!s.agents.empty() && s.agents.size() <= 5,
                "expected 1 to 5 attributed agent experiences");
        for (const auto& a : s.agents) {
            agent_validator.validate(a);
            require(a.decision_time == h.decision_time &&
                    a.next_decision_time == h.next_decision_time &&
                    a.terminal == h.terminal, "agent/historical transition mismatch");
            ++agent_count;
        }
        last = h.decision_time;
    }
    require(agent_count > 0, "no agent experiences");
}
DevRewardedTrainingReport DevSupervisorRewardedTraining::train(
    const std::vector<DevRewardedTrainingSample>& samples,
    DevHistoricalTrainingIntegration& trainer, std::int64_t cutoff) {
    validate(samples, cutoff); // validate all before any optimizer update
    std::vector<DevTransition> agents;
    std::vector<DevManagerTransition> managers;
    for (const auto& s : samples) {
        managers.push_back(s.manager);
        agents.insert(agents.end(), s.agents.begin(), s.agents.end());
    }
    DevRewardedTrainingReport report;
    report.decision_samples = samples.size();
    report.attributed_agent_samples = agents.size();
    report.optimizer = trainer.train(agents, managers);
    return report;
}
void DevSupervisorRewardedTraining::save(
    const DevHistoricalTrainingIntegration& trainer,
    const std::filesystem::path& agent_path,
    const std::filesystem::path& manager_path) {
    trainer.saveCheckpoints(agent_path, manager_path);
}
} // namespace devai::strategy
