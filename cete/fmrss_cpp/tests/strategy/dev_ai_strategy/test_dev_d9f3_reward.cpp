#include "devai/strategy/dev_ai_strategy/DevRewardAttribution_D9F3.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
 using namespace devai::strategy::d9f3;
 constexpr double capital=7000;
 // Entry fee is charged to the OPEN transition, not the following action.
 assert(std::abs(transitionReward(7000,6993,capital)+0.001)<1e-12);
 // Mark-to-market movement while holding.
 assert(std::abs(transitionReward(6993,7003,capital)-10.0/7000)<1e-12);
 // Terminal sale charge and price movement belong to last decision.
 assert(std::abs(transitionReward(7003,6998,capital)+5.0/7000)<1e-12);
 assert(std::abs(tradeGross(10,100,102)-20.0)<1e-12);
 assert(std::abs(tradeNet(20,3,4)-13.0)<1e-12);
 // Telescoping reward sum matches portfolio PnL.
 const double r=transitionReward(7000,6993,capital)+transitionReward(6993,7003,capital)+transitionReward(7003,6998,capital);
 assert(std::abs(r-(-2.0/7000))<1e-12);
 bool threw=false;try{(void)transitionReward(1,2,0);}catch(const std::invalid_argument&){threw=true;}assert(threw);
 std::cout<<"D9F3_REWARD_TESTS PASS\n";
}
