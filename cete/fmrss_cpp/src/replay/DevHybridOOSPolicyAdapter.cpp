#include "devai/replay/DevHybridOOSPolicyAdapter.hpp"
#include <ctime>
#include <stdexcept>
#include <utility>

namespace devai::replay {
namespace {
struct IST { std::string date, time; int minute; };
IST toIST(std::int64_t utcSeconds) {
    const std::time_t adjusted = static_cast<std::time_t>(utcSeconds + 19800);
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &adjusted)) throw std::runtime_error("invalid timestamp");
#else
    if (!gmtime_r(&adjusted, &tm)) throw std::runtime_error("invalid timestamp");
#endif
    char date[11], time[6];
    if (!std::strftime(date,sizeof(date),"%Y-%m-%d",&tm) ||
        !std::strftime(time,sizeof(time),"%H:%M",&tm))
        throw std::runtime_error("invalid timestamp formatting");
    return {date,time,tm.tm_hour*60+tm.tm_min};
}
std::string plainSymbol(const std::string& desired) {
    if (desired.rfind("SHORT:",0)==0) return desired.substr(6);
    return desired;
}
}
DevHybridOOSPolicyAdapter::DevHybridOOSPolicyAdapter(
    const cete::replay::ReplayEligibilityGate& gate,
    DevHybridConfirmationPolicy confirmation,
    std::vector<std::string> universe,
    devai::strategy::DevFrozenManagerPolicy manager,
    std::size_t minimumCandidates,
    HybridOOSAuditSink audit)
    :gate_(gate),confirmation_(std::move(confirmation)),
     universe_(std::move(universe)),manager_(std::move(manager)),
     minimumCandidates_(minimumCandidates),audit_(std::move(audit)) {
    if(!manager_) throw std::invalid_argument("Hybrid OOS requires frozen DevManager policy");
    if(universe_.empty() || minimumCandidates_==0 || minimumCandidates_>universe_.size())
        throw std::invalid_argument("Hybrid OOS invalid universe/minimum candidates");
}
std::string DevHybridOOSPolicyAdapter::operator()(
    const devai::strategy::DevOOSDecisionPoint& point,
    const devai::strategy::DevOOSLedgerState& ledger) const {
    const auto stamp=toIST(point.decision_time);
    HybridOOSAudit row{stamp.date,stamp.time,"","","",0};
    auto finish=[&](std::string approved, std::string reason){
        row.approved=approved; row.reason=reason;
        if(audit_) audit_(row);
        return approved;
    };
    // Mandatory flat is independent of candidate eligibility or manager inference.
    if(stamp.minute>=915) return finish("","MANDATORY_FLAT_1515");
    // Preserve manager authority: call existing frozen policy on unchanged inputs.
    const std::string requested=manager_(point,ledger);
    row.requested=requested;
    if(requested.empty()) return finish("","MANAGER_EXIT_OR_WAIT");
    const std::string symbol=plainSymbol(requested);
    if(symbol.empty()) throw std::runtime_error("Hybrid OOS malformed manager instruction");
    // A hold must not be blocked by an ENTRY-only eligibility gate.
    if(symbol==ledger.held_symbol &&
       ((requested.rfind("SHORT:",0)==0)==(ledger.quantity<0)))
        return finish(requested,"MANAGER_HOLD");
    // If manager asks to rotate, and entry is blocked, retain existing position.
    const std::string fallback=ledger.quantity<0?"SHORT:"+ledger.held_symbol:ledger.held_symbol;
    if(stamp.minute<555) return finish(fallback,"OUTSIDE_ENTRY_WINDOW");
    const auto universeStatus=gate_.evaluate(stamp.date,stamp.time,universe_,1);
    std::size_t eligible=0;
    for(const auto& s:universe_) {
        auto it=universeStatus.byStock.find(s);
        if(it==universeStatus.byStock.end()) continue;
        if(confirmation_.evaluate(it->second,stamp.time).entryDataEligible) ++eligible;
    }
    row.eligibleCount=eligible;
    if(eligible<minimumCandidates_) return finish(fallback,"INSUFFICIENT_CONFIRMED_STOCKS");
    const auto status=gate_.check(stamp.date,stamp.time,symbol);
    const auto confirm=confirmation_.evaluate(status,stamp.time);
    if(!confirm.entryDataEligible) return finish(fallback,confirm.reason);
    return finish(requested,"ENTRY_DATA_READY_MANAGER_DECIDES");
}
} // namespace devai::replay
