#include "devai/strategy/dev_ai_strategy/DevDirectionalTraining_F3EF2C5B.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy::f3ef2c5b;
int main() {
 const auto flat=agentMask(false,true,true,true);
 assert(flat[0]&&flat[1]&&flat[2]&&!flat[3]);
 const auto blocked=agentMask(false,false,true,true);
 assert(blocked[1]&&!blocked[2]);
 const auto long_only=agentMask(false,true,false,true);
 assert(long_only[1]&&!long_only[2]);
 const auto held=agentMask(true,true,true,true);
 assert(held[0]&&!held[1]&&!held[2]&&held[3]);
 const auto cutoff=agentMask(false,true,true,false);
 assert(cutoff[0]&&!cutoff[1]&&!cutoff[2]);
 assert(targetForEntry("RELIANCE",1)=="RELIANCE");
 assert(targetForEntry("RELIANCE",2)=="SHORT:RELIANCE");
 assert(preserveTarget("RELIANCE",-3)=="SHORT:RELIANCE");
 assert(preserveTarget("RELIANCE",3)=="RELIANCE");
 assert(preserveTarget("",0).empty());
 assert(std::fabs(markedEquity(10000.,-3,101.)-9697.)<1e-9);
 assert(std::fabs(markedEquity(10000.,-3,99.)-9703.)<1e-9);
 bool rejected=false;
 try { (void)targetForEntry("RELIANCE",3); } catch(const std::invalid_argument&) { rejected=true; }
 assert(rejected);
 // 2026-10-25 09:15 IST is 03:45 UTC; use fixed UTC epoch day base.
 constexpr std::int64_t day=86400*20000;
 assert(minuteIST(day+3*3600+45*60)==9*60+15);
 assert(mayEnter(day+3*3600+45*60));
 assert(!mayEnter(day+9*3600+15*60)); // 14:45 IST
 assert(mustFlatten(day+9*3600+45*60)); // 15:15 IST
 std::cout<<"F3E-F2C.5B PASS (signed PnL, directional masks, shortability, cutoff)\n";
}
