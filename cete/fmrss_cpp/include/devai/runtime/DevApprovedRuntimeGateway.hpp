#pragma once
#include "devai/approval/DevModelApprovalVerifier.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include "cete/supervision/DevinderjeetSupervisor.hpp"
#include <functional>
#include <string>
#include <vector>

namespace devai::runtime {
// Use the same verifier for 5C-3E activation and for each order submission.
// A missing callback means DENY. This gateway does not turn on trading.
using EnabledCheck = std::function<bool(const std::string&)>;
using RuntimeAuthorization = std::function<bool(const std::string&, const std::string&)>;

class DevApprovedRuntimeGateway {
public:
    DevApprovedRuntimeGateway(devai::DevManagerRegistry& registry,
        cete::supervision::DevinderjeetSupervisor& central,
        const devai::approval::DevModelApprovalVerifier& verifier,
        EnabledCheck enabled = {}, RuntimeAuthorization authorized = {});

    [[nodiscard]] bool eligible(const std::string& manager_id,
                                const std::string& symbol) const noexcept;
    // Re-checks every gate immediately before calling central.execute().
    // Rejections here never reach the execution supervisor.
    [[nodiscard]] cete::supervision::DevOrderReport submit(
        const cete::supervision::DevSupervisedOrder& order) const;
private:
    devai::DevManagerRegistry& registry_;
    cete::supervision::DevinderjeetSupervisor& central_;
    const devai::approval::DevModelApprovalVerifier& verifier_;
    EnabledCheck enabled_;
    RuntimeAuthorization authorized_;
};
} // namespace devai::runtime
