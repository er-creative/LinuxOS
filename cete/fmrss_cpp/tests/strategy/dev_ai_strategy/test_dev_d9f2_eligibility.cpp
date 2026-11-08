#include "devai/strategy/dev_ai_strategy/DevEntryEligibility_D9F2.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy::d9f2;
int main(){
 const auto t=trainingScoreThreshold({-1,0,1,2,3},{0.75,0});
 assert(t==2);
 assert(!entryEligibility(true,1.9,t,true,true).accepted);
 assert(entryEligibility(true,2.0,t,true,true).accepted);
 assert(!entryEligibility(true,3.0,t,false,true).accepted);
 assert(!entryEligibility(true,3.0,t,true,false).accepted);
 assert(!switchEligibility(true,3.0,t,0.5,0.75,true,true).accepted);
 assert(switchEligibility(true,3.0,t,0.8,0.75,true,true).accepted);
 bool threw=false;try{trainingScoreThreshold({});}catch(const std::invalid_argument&){threw=true;}
 assert(threw);
 std::cout<<"D9F2_ELIGIBILITY_TESTS PASS\n";
}
