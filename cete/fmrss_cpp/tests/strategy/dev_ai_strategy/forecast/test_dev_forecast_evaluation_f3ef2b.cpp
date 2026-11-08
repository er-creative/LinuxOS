#include "devai/strategy/dev_ai_strategy/forecast/DevForecastEvaluation_F3EF2B.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace devai::forecast::f3ef2b;
int main(){Model m;for(int i=0;i<5;++i){m.sd[i]=1;m.intercept[i]=0.01*(i+1);m.w[i][0]=0.5;}V5 x{2,0,0,0,0};auto p=m.predict(x);assert(std::abs(p[0]-1.01)<1e-12);assert(std::abs(p[4]-1.05)<1e-12);Stats s;s.add(0.02,0.03,0,0.001);assert(s.n==1&&s.positive==1&&s.modelBetter==1);auto fields=split("a,b,c");assert(fields.size()==3&&fields[2]=="c");std::cout<<"F3E-F2B EVALUATION: PASS (multi-horizon inference, stats, CSV parsing)\n";}
