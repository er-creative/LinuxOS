#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingIntegration.hpp"
#include "devai/strategy/dev_ai_strategy/DevFrozenOOSPolicy.hpp"
#include <stdexcept>
#include <utility>

namespace devai::strategy {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
}

DevHistoricalTrainingIntegration::DevHistoricalTrainingIntegration(
    DevHistoricalTrainingIntegrationConfig config)
    : config_(std::move(config)), agent_(config_.agent), manager_(config_.manager) {
    require(config_.training_end_exclusive > 0,
            "3.19D-3: training_end_exclusive must be positive");
    require(config_.agent_updates_per_transition > 0 &&
            config_.manager_updates_per_transition > 0,
            "3.19D-3: update counts must be positive");
}

DevHistoricalTrainingIntegrationReport DevHistoricalTrainingIntegration::train(
    const std::vector<DevTransition>& agents,
    const std::vector<DevManagerTransition>& managers) {
    require(!trained_, "3.19D-3: training integration is single-use");
    require(!agents.empty() && !managers.empty(),
            "3.19D-3: both causal experience streams are required");

    // Validate all input before performing any network update.
    DevTransitionValidator agent_validator;
    std::int64_t previous = 0;
    for (const auto& t : agents) {
        agent_validator.validate(t);
        require(t.decision_time >= previous,
                "3.19D-3: agent experiences must be chronological");
        require(t.next_decision_time < config_.training_end_exclusive,
                "3.19D-3: agent experience crosses training cutoff");
        previous = t.decision_time;
    }
    previous = 0;
    for (const auto& t : managers) {
        DevManagerTrainer::validateTransition(t);
        require(t.decision_time > previous,
                "3.19D-3: manager experiences must be strictly chronological");
        require(t.next_decision_time < config_.training_end_exclusive,
                "3.19D-3: manager experience crosses training cutoff");
        previous = t.decision_time;
    }

    DevHistoricalTrainingIntegrationReport report;
    for (const auto& t : agents) {
        agent_.observe(t);
        ++report.agent_transitions;
        if (agent_.canTrain()) {
            for (std::size_t i=0; i<config_.agent_updates_per_transition; ++i) {
                const auto result = agent_.trainOneUpdate();
                require(result.numerical_integrity,
                        "3.19D-3: agent training numerical failure");
            }
        }
    }
    for (const auto& t : managers) {
        manager_.addExperience(t);
        ++report.manager_transitions;
        if (manager_.canTrain()) {
            for (std::size_t i=0; i<config_.manager_updates_per_transition; ++i) {
                const auto result = manager_.trainOneUpdate();
                require(result.numerical_integrity,
                        "3.19D-3: manager training numerical failure");
            }
        }
    }
    report.agent_updates = agent_.completedTrainingUpdates();
    report.manager_updates = manager_.completedTrainingUpdates();
    require(report.agent_updates > 0 && report.manager_updates > 0,
            "3.19D-3: insufficient rewarded experiences to train both networks");
    trained_ = true;
    return report;
}

void DevHistoricalTrainingIntegration::saveCheckpoints(
    const std::filesystem::path& agent_path,
    const std::filesystem::path& manager_path) const {
    require(trained_, "3.19D-3: cannot export before successful training");
    require(agent_path != manager_path, "3.19D-3: checkpoint paths must differ");
    if (!agent_path.parent_path().empty())
        std::filesystem::create_directories(agent_path.parent_path());
    if (!manager_path.parent_path().empty())
        std::filesystem::create_directories(manager_path.parent_path());
    saveDevFrozenAgentCheckpoint(agent_path, agent_.onlineNetwork());
    saveDevFrozenManagerCheckpoint(manager_path, manager_.onlineNetwork());
}

const DevSharedAgentTrainer&
DevHistoricalTrainingIntegration::agentTrainer() const noexcept { return agent_; }
const DevManagerTrainer&
DevHistoricalTrainingIntegration::managerTrainer() const noexcept { return manager_; }

} // namespace devai::strategy
