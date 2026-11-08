#pragma once
#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <functional>
#include <string>
#include <vector>

namespace devai::activation {
// A real approval service must verify the approved model artifact, its exact
// stock universe, version, and OOS result. Never infer approval from a path.
using ModelApprovalCheck = std::function<bool(const std::string& manager_id,
                                               const std::vector<std::string>& symbols)>;
using ConfigurationEnabledCheck = std::function<bool(const std::string& manager_id)>;

class DevManagerActivationController {
public:
    DevManagerActivationController(devai::strategy::DevAISupervisor& supervisor,
                                   devai::DevManagerRegistry& registry,
                                   ModelApprovalCheck model_approved = {},
                                   ConfigurationEnabledCheck configured_enabled = {});
    // Throws on any failed gate; no registry state is changed on gate failure.
    void activate(const std::string& manager_id);
    // Administrative deactivation through DevAISupervisor authorization.
    void deactivate(const std::string& manager_id);
private:
    devai::strategy::DevAISupervisor& supervisor_;
    devai::DevManagerRegistry& registry_;
    ModelApprovalCheck model_approved_;
    ConfigurationEnabledCheck configured_enabled_;
};
} // namespace devai::activation
