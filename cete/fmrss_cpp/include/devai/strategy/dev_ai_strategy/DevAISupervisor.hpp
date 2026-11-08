#pragma once
#include "devai/strategy/dev_ai_strategy/DevinderjeetSupervisor.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include "devai/config/DevAIRuntimeConfig.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace devai::strategy {
// Strategy-level administration only. The CETE supervisor remains the sole
// execution and accounting authority. This component owns no trading ledger.
class DevAISupervisor {
public:
    using Authorize = std::function<bool(const std::string& operation,
                                         const std::string& manager_id)>;
    DevAISupervisor(devai::DevManagerRegistry& registry,
                    DevinderjeetSupervisor& central,
                    Authorize authorization);

    // Creates a persistent manager and registers its five agent identities and
    // capital with CETE. A failed CETE registration leaves an INACTIVE registry
    // entry for operator reconciliation; IDs are never silently reused.
    [[nodiscard]] devai::ManagedUniverse createManager(
        const std::vector<std::string>& symbols,
        const std::array<std::string, 5>& agent_ids,
        double allocated_capital);
    void assignShares(const std::string& manager_id,
                      const std::vector<std::string>& symbols);
    void setActive(const std::string& manager_id, bool active);
    [[nodiscard]] std::vector<devai::ManagedUniverse> managers() const;
    [[nodiscard]] DevManagerAccount account(const std::string& manager_id) const;
    [[nodiscard]] DevSupervisorSnapshot consolidatedSnapshot() const;
private:
    void requireAuthorized(const std::string& operation,
                           const std::string& manager_id) const;
    devai::DevManagerRegistry& registry_;
    DevinderjeetSupervisor& central_;
    Authorize authorization_;
};
} // namespace devai::strategy
