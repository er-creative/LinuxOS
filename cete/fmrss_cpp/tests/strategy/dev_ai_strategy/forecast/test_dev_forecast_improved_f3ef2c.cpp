#include "devai/strategy/dev_ai_strategy/forecast/DevForecastImproved_F3EF2C.hpp"
#include <cassert>
#include <iostream>
using namespace devai::forecast::f3ef2c;
int main(){
auto c=zerodha_nse(100,101,50,false);assert(std::abs(c.brokerage-(1.5+1.515))<1e-9);assert(std::abs(c.stt-1.2625)<1e-9);assert(std::abs(c.exchange-10050*.0000307)<1e-9);assert(std::abs(c.sebi-10050*.000001)<1e-9);assert(std::abs(c.stamp-.15)<1e-9);assert(std::abs(c.gst-.18*(c.brokerage+c.exchange+c.sebi))<1e-9);assert(std::abs(c.total-(c.brokerage+c.stt+c.exchange+c.sebi+c.stamp+c.gst))<1e-9);
auto rounded=zerodha_nse(100,101,50);assert(rounded.stt==1);auto capped=zerodha_nse(100,100,10000);assert(capped.brokerage==40);
Stats s;Vec x{};x[0]=1;s.add(x);x[0]=3;s.add(x);Scale scale;scale.fit(s);assert(std::abs(scale.mean[0]-2)<1e-12);assert(std::abs(scale.apply(x)[0]-1)<1e-12);
Normal n;Vec z{};for(int i=0;i<50;++i){z[0]=i/50.;n.add(z,.01+.02*z[0]);}auto w=solve(n,.000001);assert(std::abs(predict(w,z)-(.01+.02*z[0]))<1e-4);
std::cout<<"F3E-F2C: PASS (brokerage, STT, NSE fees, GST, stamp, scaling, ridge)\n";
}
