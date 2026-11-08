#pragma once
#include "devai/config/DevAIRuntimeConfig.hpp"
#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace devai::config {
class DevAIConfigurationBootstrap final {
public:
    // The application supplies the real train + OOS-test + approval pipeline.
    // Return true ONLY after a model trained for exactly these symbols is approved
    // and safely deployed for this manager. No cross-manager model reuse.
    using TrainTestApprove = std::function<bool(const std::string& manager_id,
                                                 const std::vector<std::string>& symbols,
                                                 const std::string& model_directory)>;
    DevAIConfigurationBootstrap(devai::strategy::DevAISupervisor& supervisor,
                                devai::DevManagerRegistry& registry,
                                TrainTestApprove train_test_approve);
    // Applies startup policy. A changed universe is deactivated BEFORE updating
    // symbols/training. On training failure the manager remains inactive.
    void apply(const DevAIRuntimeConfig& configuration,
               const std::vector<std::string>& default_symbols,
               double approved_capital);
    static std::array<std::string, 5> agentIds(const std::string& manager_id);
private:
    devai::strategy::DevAISupervisor& supervisor_;
    devai::DevManagerRegistry& registry_;
    TrainTestApprove train_test_approve_;
};
} // namespace devai::config
