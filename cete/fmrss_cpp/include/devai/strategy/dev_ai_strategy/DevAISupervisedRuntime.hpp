#pragma once
#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedModelMultiManagerRuntime.hpp"
#include <array>
#include <string>
#include <vector>

namespace devai::strategy {
// The strategy-facing gateway. Do not expose the raw runtime to order callers.
// Administrative checks are repeated at evaluation AND submission time.
class DevAISupervisedRuntime final {
public:
    DevAISupervisedRuntime(DevAISupervisor& administrator,
                           devai::DevManagerRegistry& registry,
                           DevinderjeetSupervisor& central,
                           DevSharedModelMultiManagerRuntime& runtime);
    void attachManager(const std::string& manager_id,
                       const std::array<std::string, DEV_AGENT_SLOT_COUNT>& agents);
    [[nodiscard]] DevSharedManagerCycle evaluate(
        const std::string& manager_id, std::int64_t decision_time,
        const std::vector<DevSharedCandidate>& candidates) const;
    [[nodiscard]] DevOrderReport submit(const DevSharedManagerCycle& cycle,
        DevSupervisedAction action, const std::string& order_id,
        std::int64_t shares, double execution_price);
    // Exposed for unit tests and orchestration preflight; not an order authority.
    void requireEligible(const std::string& manager_id,
                         const std::vector<std::string>& symbols) const;
private:
    DevAISupervisor& administrator_;
    devai::DevManagerRegistry& registry_;
    DevinderjeetSupervisor& central_;
    DevSharedModelMultiManagerRuntime& runtime_;
};
} // namespace devai::strategy
