#pragma once
#include "cete/replay/ReplayEligibilityGate.hpp"
#include <string>

namespace devai::replay {
// This policy gates *new entries*, not exits, manager decisions, or RL actions.
// It consumes causal eligibility computed by CETE Step 2.
enum class ConfirmationLevel { None, Historical, Current1m, Current5m, Current15m };
enum class EntryRequirement { Current1m, Current5m, Current15m };

struct HybridConfirmationConfig {
    EntryRequirement minimumEntryConfirmation = EntryRequirement::Current1m;
    // If true, once a current-session 5m/15m candle has completed, require
    // the most recent such candle to be present. This is a data-integrity
    // check, not a bullish/bearish signal confirmation.
    bool requireLatestCompletedTimeframes = true;
};

struct HybridConfirmationResult {
    bool evaluationAllowed = false;
    bool entryDataEligible = false;
    ConfirmationLevel level = ConfirmationLevel::None;
    bool current1m = false;
    bool current5m = false;
    bool current15m = false;
    std::string reason;
};

class DevHybridConfirmationPolicy {
public:
    explicit DevHybridConfirmationPolicy(HybridConfirmationConfig config = {});
    HybridConfirmationResult evaluate(const cete::replay::DecisionStatus& eligibility,
                                      const std::string& time) const;
private:
    HybridConfirmationConfig config_;
};
const char* toString(ConfirmationLevel level) noexcept;
} // namespace devai::replay
