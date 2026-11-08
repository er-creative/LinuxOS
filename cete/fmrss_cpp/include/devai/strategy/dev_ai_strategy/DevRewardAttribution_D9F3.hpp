#pragma once
#include <cmath>
#include <stdexcept>
namespace devai::strategy::d9f3 {
// Action is taken at current mark. Its reward includes immediate transaction costs
// and price movement until the next mark (or final liquidation at 15:15).
inline double transitionReward(double before_action_equity,double next_mark_equity,double capital){
 if(!std::isfinite(before_action_equity)||!std::isfinite(next_mark_equity)||!(capital>0))
  throw std::invalid_argument("D9F3 invalid equity or capital");
 return (next_mark_equity-before_action_equity)/capital;
}
inline double tradeGross(int quantity,double buy_fill,double sell_fill){
 if(quantity<=0||!(buy_fill>0)||!(sell_fill>0))throw std::invalid_argument("D9F3 invalid trade");
 return quantity*(sell_fill-buy_fill);
}
inline double tradeNet(double gross,double buy_fee,double sell_fee){
 if(buy_fee<0||sell_fee<0)throw std::invalid_argument("D9F3 negative fees");
 return gross-buy_fee-sell_fee;
}
}
