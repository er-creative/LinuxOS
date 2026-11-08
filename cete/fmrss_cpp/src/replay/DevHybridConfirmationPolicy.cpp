#include "devai/replay/DevHybridConfirmationPolicy.hpp"
#include <cctype>
#include <stdexcept>

namespace devai::replay {
namespace {
int minuteOfDay(const std::string& s) {
    if(s.size()!=5 || s[2]!=':' || !std::isdigit(static_cast<unsigned char>(s[0])) ||
       !std::isdigit(static_cast<unsigned char>(s[1])) ||
       !std::isdigit(static_cast<unsigned char>(s[3])) ||
       !std::isdigit(static_cast<unsigned char>(s[4]))) return -1;
    int h=(s[0]-'0')*10+s[1]-'0', m=(s[3]-'0')*10+s[4]-'0';
    return h<24 && m<60 ? h*60+m : -1;
}
}
DevHybridConfirmationPolicy::DevHybridConfirmationPolicy(HybridConfirmationConfig config)
    : config_(config) {}
HybridConfirmationResult DevHybridConfirmationPolicy::evaluate(
        const cete::replay::DecisionStatus& s, const std::string& time) const {
    HybridConfirmationResult r;
    const int now=minuteOfDay(time);
    if(now<555 || now>=915) {r.reason="OUTSIDE_DECISION_WINDOW";return r;}
    if(!s.allowed) {r.reason=s.reason.empty()?"HISTORICAL_DATA_NOT_READY":s.reason;return r;}
    r.evaluationAllowed=true;
    r.level=ConfirmationLevel::Historical;
    r.current1m=s.current1m;
    r.current5m=s.current5m;
    r.current15m=s.current15m;
    if(s.current1m) r.level=ConfirmationLevel::Current1m;
    if(s.current5m) r.level=ConfirmationLevel::Current5m;
    if(s.current15m) r.level=ConfirmationLevel::Current15m;

    // Never override the Step 2 gate's data/timing OPEN restriction.
    if(!s.openEligible) {r.reason=s.reason.empty()?"BASE_ENTRY_GATE_BLOCKED":s.reason;return r;}
    if(!s.current1m) {r.reason="AWAIT_CURRENT_1M";return r;}
    const int elapsed=now-555;
    if(config_.requireLatestCompletedTimeframes) {
        if(elapsed>=5 && !s.current5m) {r.reason="LATEST_5M_MISSING";return r;}
        if(elapsed>=15 && !s.current15m) {r.reason="LATEST_15M_MISSING";return r;}
    }
    if(config_.minimumEntryConfirmation==EntryRequirement::Current5m && !s.current5m) {
        r.reason="AWAIT_CURRENT_5M";return r;
    }
    if(config_.minimumEntryConfirmation==EntryRequirement::Current15m && !s.current15m) {
        r.reason="AWAIT_CURRENT_15M";return r;
    }
    r.entryDataEligible=true;
    r.reason="ENTRY_DATA_READY_MANAGER_DECIDES";
    return r;
}
const char* toString(ConfirmationLevel level) noexcept {
    switch(level) {
        case ConfirmationLevel::None: return "NONE";
        case ConfirmationLevel::Historical: return "HISTORICAL";
        case ConfirmationLevel::Current1m: return "CURRENT_1M";
        case ConfirmationLevel::Current5m: return "CURRENT_5M";
        case ConfirmationLevel::Current15m: return "CURRENT_15M";
    }
    return "UNKNOWN";
}
} // namespace devai::replay
