#include "devai/runtime/DevApprovedRuntimeGateway.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace devai::runtime {
DevApprovedRuntimeGateway::DevApprovedRuntimeGateway(
    devai::DevManagerRegistry& registry,
    cete::supervision::DevinderjeetSupervisor& central,
    const devai::approval::DevModelApprovalVerifier& verifier,
    EnabledCheck enabled, RuntimeAuthorization authorized)
    : registry_(registry), central_(central), verifier_(verifier),
      enabled_(std::move(enabled)), authorized_(std::move(authorized)) {}

bool DevApprovedRuntimeGateway::eligible(const std::string& id,
                                         const std::string& symbol) const noexcept {
    try {
        if (id.empty() || symbol.empty() || !enabled_ || !authorized_) return false;
        const auto manager = registry_.load_manager(id);
        if (!manager.active || !enabled_(id) || !authorized_("submit_order", id)) return false;
        if (std::find(manager.symbols.begin(), manager.symbols.end(), symbol) == manager.symbols.end())
            return false; // NIFTY%2050 is not a tradable registered share.
        (void)central_.managerAccount(id); // must be registered in this process
        if (central_.snapshot().halted) return false;
        return verifier_.approved(id, manager.symbols);
    } catch (...) { return false; }
}

cete::supervision::DevOrderReport DevApprovedRuntimeGateway::submit(
    const cete::supervision::DevSupervisedOrder& order) const {
    if (!eligible(order.manager_id, order.symbol))
        throw std::runtime_error("CETE 5C-3G: order blocked by runtime eligibility / model approval");
    // Central supervisor still applies cash-segment risk and order checks.
    return central_.execute(order);
}
} // namespace devai::runtime
