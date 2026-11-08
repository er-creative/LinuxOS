#pragma once
#include "devai/strategy/dev_ai_strategy/DevManagerSupervisorBridge.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace devai::strategy {
// Caller supplies causally ready observations, assignments and execution prices.
// Both model objects are shared read-only across every manager. No online training.
struct DevSharedCandidate {
    std::size_t slot{0};
    std::string symbol;
    double candidate_score{0};
    std::size_t candidate_rank{0};
    DevAIObservation observation{};
};
struct DevSharedManagerCycle {
    std::string manager_id;
    std::int64_t decision_time{0};
    DevManagerState state{};
    DevManagerAction action{DevManagerAction::NO_TRADE};
    DevManagerDecision decision{};
    DevManagerQValues manager_q{};
    std::vector<DevAgentReport> global_agent_reports;
};
class DevSharedModelMultiManagerRuntime {
public:
    DevSharedModelMultiManagerRuntime(const DevQNetwork& shared_agent_model,
        const DevManagerQNetwork& shared_manager_model,
        DevinderjeetSupervisor& supervisor,
        const devai::execution::DevSecureExecutionPath& execution);
    void addManager(std::string manager_id,
        std::array<std::string, DEV_AGENT_SLOT_COUNT> global_agent_ids);
    [[nodiscard]] DevSharedManagerCycle evaluate(const std::string& manager_id,
        std::int64_t decision_time, const std::vector<DevSharedCandidate>& candidates) const;
    // Execution is a separate explicit step: never infer an execution price from an observation.
    [[nodiscard]] DevOrderReport submit(const DevSharedManagerCycle& cycle,
        DevSupervisedAction action, const std::string& order_id,
        std::int64_t shares, double execution_price);
private:
    const DevQNetwork& agent_model_;
    const DevManagerQNetwork& manager_model_;
    DevinderjeetSupervisor& supervisor_;
    const devai::execution::DevSecureExecutionPath& execution_;
    std::map<std::string,std::unique_ptr<DevManagerSupervisorBridge>> managers_;
    DevManager manager_logic_;
    DevManagerStateEncoder encoder_;
};
} // namespace devai::strategy
