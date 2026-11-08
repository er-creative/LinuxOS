#include "devai/replay/DevHybridOOSPolicyAdapter.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace devai;
static std::int64_t epoch(const std::string& date, int h, int m) {
    using namespace std::chrono;
    const int y=std::stoi(date.substr(0,4)),mo=std::stoi(date.substr(5,2)),d=std::stoi(date.substr(8,2));
    return duration_cast<seconds>(sys_days{year{y}/month{static_cast<unsigned>(mo)}/day{static_cast<unsigned>(d)}}.time_since_epoch()
            +hours{h}+minutes{m}-minutes{330}).count();
}
int main(int argc,char**argv) {
    if(argc!=3) {std::cerr<<"Usage: test_dev_hybrid_oos_policy_adapter PHASE3B_DIR PHASE3A_DIR\n";return 2;}
    const std::string b=argv[1],a=argv[2];
    cete::replay::ReplayEligibilityGate gate;
    gate.loadHybrid(b+"/daily_eligibility.csv",b+"/decision_eligibility.csv",a);
    const std::vector<std::string> universe={"ADANIPORTS","AXISBANK","BHARTIARTL","HCLTECH","HDFCBANK","ICICIBANK","ITC","NTPC","RELIANCE","SBIN"};
    std::string wanted="RELIANCE";
    std::vector<replay::HybridOOSAudit> audits;
    replay::DevHybridOOSPolicyAdapter adapter(gate,replay::DevHybridConfirmationPolicy{},universe,
        [&](const strategy::DevOOSDecisionPoint&,const strategy::DevOOSLedgerState&){return wanted;},5,
        [&](const replay::HybridOOSAudit& x){audits.push_back(x);});
    strategy::DevOOSDecisionPoint point{};
    strategy::DevOOSLedgerState ledger{};
    auto call=[&](int h,int m){point.decision_time=epoch("2025-10-30",h,m);return adapter(point,ledger);};
    assert(call(9,15).empty());
    assert(audits.back().reason=="INSUFFICIENT_CONFIRMED_STOCKS");
    assert(call(9,16)=="RELIANCE");
    assert(call(9,20)=="RELIANCE");
    assert(call(9,29)=="RELIANCE");
    assert(call(9,30)=="RELIANCE");
    ledger.held_symbol="RELIANCE";ledger.quantity=2;
    assert(call(9,15)=="RELIANCE"); // HOLD independent of entry confirmation
    wanted="";
    assert(call(9,15).empty()); // manager exit independent of entry confirmation
    wanted="AXISBANK";
    assert(call(15,15).empty()); // mandatory flat independent of manager
    std::cout<<"PASS CETE Step 4 hybrid OOS policy adapter (entry/hold/exit/flat)\n";
    std::cout<<"09:15 blocked new OPEN; 09:16/09:20/09:29/09:30 permitted; 15:15 forced FLAT\n";
}
