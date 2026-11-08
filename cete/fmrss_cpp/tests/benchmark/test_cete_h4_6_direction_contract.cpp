#include "devai/strategy/dev_ai_strategy/DevManagerDirectionContract_H46.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy::h46;
int main() {
 assert(permits("DevManager_001",Side::LONG));
 assert(!permits("DevManager_001",Side::SHORT));
 assert(permits("DevManager_002",Side::SHORT));
 assert(!permits("DevManager_002",Side::LONG));
 assert(agentForAction("DevManager_001",6)=="DevAgent_005");
 assert(agentForAction("DevManager_002",6)=="DevAgent_010");
 bool rejected=false;
 try { (void)agentForAction("DevManager_003",2); }
 catch(const std::invalid_argument&) {rejected=true;}
 assert(rejected);
 std::cout<<"CETE H4.6 DIRECTION CONTRACT PASS\n";
}
