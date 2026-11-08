#include "devai/strategy/dev_ai_strategy/forecast/DevForecastImproved_F3EF2C.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::forecast::f3ef2c;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(){
 try{
  auto c=zerodha_nse(100,100,52,false);
  check(std::abs(c.brokerage-3.12)<1e-9,"per-side brokerage");
  check(std::abs(c.stt-1.30)<1e-9,"sell-side STT");
  check(std::abs(c.exchange-0.31928)<1e-8,"NSE transaction charges");
  check(std::abs(c.sebi-0.0104)<1e-9,"SEBI");
  check(std::abs(c.stamp-0.156)<1e-9,"buy stamp");
  check(std::abs(c.gst-0.18*(3.12+0.31928+0.0104))<1e-8,"GST");
  check(zerodha_nse(100,100,52).stt==1,"rounded STT");
  check(zerodha_nse(100,101,52).total>0,"cost positive");
  std::cout<<"F3E-F2C.1 DIAGNOSTICS: PASS (brokerage, STT, exchange, SEBI, stamp, GST)\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
