#pragma once
#include "cete/replay/ReplayEligibilityGate.hpp"
#include "devai/replay/DevHybridConfirmationPolicy.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace devai::replay {
struct HybridOOSAudit {
    std::string date, time, requested, approved, reason;
    std::size_t eligibleCount = 0;
};
using HybridOOSAuditSink = std::function<void(const HybridOOSAudit&)>;

// Wraps an EXISTING frozen DevManager policy; never substitutes an RL decision.
// Only NEW entries/rotations are gated. HOLD and EXIT remain available.
// 15:15 IST forces FLAT even if the manager requests HOLD.
class DevHybridOOSPolicyAdapter {
public:
    DevHybridOOSPolicyAdapter(
        const cete::replay::ReplayEligibilityGate& gate,
        DevHybridConfirmationPolicy confirmation,
        std::vector<std::string> universe,
        devai::strategy::DevFrozenManagerPolicy manager,
        std::size_t minimumCandidates = 5,
        HybridOOSAuditSink audit = {});

    std::string operator()(const devai::strategy::DevOOSDecisionPoint& point,
                           const devai::strategy::DevOOSLedgerState& ledger) const;
private:
    const cete::replay::ReplayEligibilityGate& gate_;
    DevHybridConfirmationPolicy confirmation_;
    std::vector<std::string> universe_;
    devai::strategy::DevFrozenManagerPolicy manager_;
    std::size_t minimumCandidates_;
    HybridOOSAuditSink audit_;
};
} // namespace devai::replay
