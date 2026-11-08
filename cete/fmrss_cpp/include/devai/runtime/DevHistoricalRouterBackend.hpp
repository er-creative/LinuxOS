#pragma once
#include "devai/runtime/DevModeAwareExecutionRouter.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include <filesystem>
#include <map>
#include <tuple>

namespace devai::runtime {
struct HistoricalRunInputs {
    std::filesystem::path cache, minute_folder, agent_checkpoint, manager_checkpoint, output_csv;
    std::int64_t last_training_decision_time = 0;
    double brokerage_rate = 0, slippage_rate = 0, other_cost_rate = 0;
};
class DevHistoricalRouterBackend {
public:
    void setInputs(const std::string& manager, RouteKind kind, HistoricalRunInputs inputs);
    void connect(DevModeAwareExecutionRouter& router);
    strategy::DevHistoricalOOSReport run(const ManagerCycleContext& ctx, RouteKind kind) const;
private:
    std::map<std::pair<std::string,RouteKind>,HistoricalRunInputs> inputs_;
};
}
