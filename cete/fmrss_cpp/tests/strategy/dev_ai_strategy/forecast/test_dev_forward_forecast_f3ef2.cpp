#include "devai/strategy/dev_ai_strategy/forecast/DevForwardForecast_F3EF2.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
using namespace devai::forecast::f3ef2;
int main(){
 Moments m;for(int i=0;i<100;++i)m.add({double(i),0.001,double(i)*.0001,0.002,double(i)*.00001});m.finish();
 Accumulator a;for(int i=0;i<100;++i){Features x{double(i),0.001,double(i)*.0001,0.002,double(i)*.00001};a.add(m.transform(x),0.03+0.002*i);}
 auto w=solve(a,1.0);auto x=m.transform({50,0.001,.005,.002,.0005});assert(std::abs(predict(w,x)-.13)<.01);
 Model model;model.moments=m;for(int h=0;h<H;++h){model.weights[h]=w;model.baselines[h]=.01;}
 auto path=(std::filesystem::temp_directory_path()/"dev_f3ef2_test.model").string();model.save(path);auto loaded=Model::load(path);std::filesystem::remove(path);
 assert(std::abs(model.forecast({50,0.001,.005,.002,.0005})[0]-loaded.forecast({50,0.001,.005,.002,.0005})[0])<1e-12);
 std::cout<<"F3E-F2 FORECAST MODEL: PASS (standardization, ridge fit, multi-horizon, serialization)\n";
}
