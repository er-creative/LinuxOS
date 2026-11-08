#include "devai/strategy/dev_ai_strategy/DevForwardReturnAudit_D9F3A.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace devai::strategy::d9f3a;
int main(){
 ForwardIndex idx;
 idx.insert(100,"RELIANCE",100*1440+600,{1000.0,0.004,true});
 idx.insert(100,"RELIANCE",100*1440+605,{998.5,-0.001,true});
 idx.insert(100,"SBIN",100*1440+605,{1050.0,0.0,true});
 auto a=idx.evaluate(100,"RELIANCE",100*1440+600,1000.0,true,0.004);
 assert(a.predicted_price&&std::abs(*a.predicted_price-1004.0)<1e-9);
 assert(a.actual_price&&std::abs(*a.actual_price-998.5)<1e-9);
 assert(a.actual_return&&std::abs(*a.actual_return+0.0015)<1e-9);
 assert(a.error_return&&std::abs(*a.error_return-0.0055)<1e-9);
 auto b=idx.evaluate(100,"RELIANCE",100*1440+605,998.5,true,-0.001);
 assert(!b.horizon_available&&!b.actual_price);
 auto c=idx.evaluate(101,"RELIANCE",100*1440+600,1000.0,false,0.0);
 assert(!c.predicted_price&&!c.actual_price);
 auto d=idx.evaluate(100,"SBIN",100*1440+600,1000.0,true,0.001);
 assert(d.horizon_available && d.actual_price && *d.actual_price==1050.0); // Own-symbol outcome.
 std::cout<<"D9F3A_FORWARD_AUDIT_TESTS PASS\n";
}
