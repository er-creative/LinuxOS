#include "devai/execution/DevSecureExecutionPath.hpp"
#include <utility>

namespace devai::execution {

DevSecureExecutionPath::DevSecureExecutionPath(
    const devai::runtime::DevApprovedRuntimeGateway& gateway,
    AuditSink audit)
    : gateway_(gateway), audit_(std::move(audit)) {}

bool DevSecureExecutionPath::eligible(const std::string& manager_id,
                                     const std::string& symbol) const noexcept {
    return gateway_.eligible(manager_id, symbol);
}

cete::supervision::DevOrderReport DevSecureExecutionPath::submit(
    const cete::supervision::DevSupervisedOrder& order) const {
    // Do not rely on a prior eligible() result: gateway.submit() rechecks.
    // An audit failure must not create an execution bypass.
    try {
        auto report = gateway_.submit(order);
        if (audit_) {
            try { audit_(order.manager_id, order.symbol, true); }
            catch (...) { /* audit is best effort; central has already processed */ }
        }
        return report;
    } catch (...) {
        if (audit_) {
            try { audit_(order.manager_id, order.symbol, false); }
            catch (...) { /* preserve original execution rejection */ }
        }
        throw;
    }
}

} // namespace devai::execution
