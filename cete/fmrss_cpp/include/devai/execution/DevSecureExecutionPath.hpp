#pragma once

#include "devai/runtime/DevApprovedRuntimeGateway.hpp"
#include <functional>
#include <string>

namespace devai::execution {

// Mandatory manager-originated order entry point. Does not own the gateway;
// its lifetime must exceed this object's lifetime.
// This component never calls DevinderjeetSupervisor::execute directly.
class DevSecureExecutionPath final {
public:
    using AuditSink = std::function<void(const std::string& manager_id,
                                         const std::string& symbol,
                                         bool accepted)>;

    explicit DevSecureExecutionPath(
        const devai::runtime::DevApprovedRuntimeGateway& gateway,
        AuditSink audit = {});

    [[nodiscard]] bool eligible(const std::string& manager_id,
                                const std::string& symbol) const noexcept;

    // Gateway performs the definitive eligibility check immediately before
    // forwarding the order to the central supervisor. Exceptions propagate.
    [[nodiscard]] cete::supervision::DevOrderReport submit(
        const cete::supervision::DevSupervisedOrder& order) const;

private:
    const devai::runtime::DevApprovedRuntimeGateway& gateway_;
    AuditSink audit_;
};

} // namespace devai::execution
