#include "devai/replay/DevHybridConfirmationPolicy.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace devai::replay;
using cete::replay::DecisionStatus;
static DecisionStatus status(bool allowed,bool open,bool one,bool five,bool fifteen,
                             std::string reason="OK") {
    DecisionStatus s;
    s.allowed=allowed;s.openEligible=open;s.current1m=one;
    s.current5m=five;s.current15m=fifteen;s.reason=std::move(reason);
    return s;
}
int main() {
    DevHybridConfirmationPolicy hybrid;
    auto a=hybrid.evaluate(status(true,false,false,false,false,"AWAIT_CURRENT_1M_FOR_OPEN"),"09:15");
    assert(a.evaluationAllowed && !a.entryDataEligible && a.level==ConfirmationLevel::Historical);
    auto b=hybrid.evaluate(status(true,true,true,false,false),"09:16");
    assert(b.entryDataEligible && b.level==ConfirmationLevel::Current1m);
    auto c=hybrid.evaluate(status(true,true,true,true,false),"09:20");
    assert(c.entryDataEligible && c.level==ConfirmationLevel::Current5m);
    auto d=hybrid.evaluate(status(true,true,true,true,false),"09:29");
    assert(d.entryDataEligible && !d.current15m);
    auto e=hybrid.evaluate(status(true,true,true,true,true),"09:30");
    assert(e.entryDataEligible && e.level==ConfirmationLevel::Current15m);
    auto f=hybrid.evaluate(status(true,true,true,true,true),"15:15");
    assert(!f.entryDataEligible && !f.evaluationAllowed);
    auto g=hybrid.evaluate(status(true,true,true,false,false),"09:20");
    assert(!g.entryDataEligible && g.reason=="LATEST_5M_MISSING");
    auto h=hybrid.evaluate(status(true,true,true,true,false),"09:30");
    assert(!h.entryDataEligible && h.reason=="LATEST_15M_MISSING");
    auto i=hybrid.evaluate(status(false,false,false,false,false,"INSUFFICIENT_HISTORICAL_WARMUP"),"09:16");
    assert(!i.evaluationAllowed && !i.entryDataEligible);
    DevHybridConfirmationPolicy five({EntryRequirement::Current5m,true});
    assert(!five.evaluate(status(true,true,true,false,false),"09:16").entryDataEligible);
    assert(five.evaluate(status(true,true,true,true,false),"09:20").entryDataEligible);
    DevHybridConfirmationPolicy fifteen({EntryRequirement::Current15m,true});
    assert(!fifteen.evaluate(status(true,true,true,true,false),"09:29").entryDataEligible);
    assert(fifteen.evaluate(status(true,true,true,true,true),"09:30").entryDataEligible);
    auto j=hybrid.evaluate(status(true,true,true,false,false),"bad");
    assert(!j.entryDataEligible);
    std::cout<<"PASS CETE Step 3 hybrid confirmation (13 checks)\n"
             <<"09:15 HISTORICAL evaluation, OPEN blocked\n"
             <<"09:16 CURRENT_1M entry data ready\n"
             <<"09:20 CURRENT_5M entry data ready\n"
             <<"09:29 CURRENT_15M not ready\n"
             <<"09:30 CURRENT_15M entry data ready\n"
             <<"15:15 outside entry window\n"
             <<"Missing 5m/15m and warmup: blocked\n";
}
