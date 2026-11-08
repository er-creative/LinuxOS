#include "devai/activation/DevManagerActivationController.hpp"
#include <stdexcept>
#include <utility>

namespace devai::activation {
DevManagerActivationController::DevManagerActivationController(
    devai::strategy::DevAISupervisor& supervisor,
    devai::DevManagerRegistry& registry,
    ModelApprovalCheck model_approved,
    ConfigurationEnabledCheck configured_enabled)
    : supervisor_(supervisor), registry_(registry),
      model_approved_(std::move(model_approved)),
      configured_enabled_(std::move(configured_enabled)) {}

void DevManagerActivationController::activate(const std::string& manager_id) {
    // Loading verifies the persistent manager exists and its symbols are valid.
    const auto manager = registry_.load_manager(manager_id);
    if (!configured_enabled_ || !configured_enabled_(manager_id))
        throw std::runtime_error("Activation denied: manager not enabled by configuration");
    // Explicitly require central registration, independently of registry status.
    (void)supervisor_.account(manager_id);
    if (!model_approved_ || !model_approved_(manager_id, manager.symbols))
        throw std::runtime_error("Activation denied: no verified model/OOS approval");
    // DevAISupervisor performs the separate set_active authorization check.
    supervisor_.setActive(manager_id, true);
}

void DevManagerActivationController::deactivate(const std::string& manager_id) {
    (void)registry_.load_manager(manager_id);
    supervisor_.setActive(manager_id, false);
}
} // namespace devai::activation
