#include "devai/strategy/dev_ai_strategy/forecast/DevMultiTimeframeFeatures_F3EF2C2.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
namespace mt=devai::forecast::f3ef2c2;
int main(){
    // 2024-12-24 09:15 IST in UTC epoch seconds
    const long long t=1735011900;
    std::vector<mt::MinuteClose> a,b;
    for(int i=0;i<100;++i){a.push_back({t+60LL*i,100.+i,0});b.push_back({t+60LL*i,200.+i,0});}
    auto bars5=mt::completed_bars(a,5),bars15=mt::completed_bars(a,15);
    assert(bars5.size()==20&&bars15.size()==6);
    assert(bars5.front().completed==t+300&&bars15.front().completed==t+900);
    assert(bars5.front().close==104&&bars15.front().close==114);
    auto ctx=mt::build_context(a,b);
    assert(!ctx.empty());
    for(const auto&[time,c]:ctx){assert(c.age5>=0&&c.age5<5&&c.age15>=0&&c.age15<15);assert(std::isfinite(c.z60));}
    // Changing future closes cannot change earlier features.
    auto altered=a;
    for(int i=90;i<100;++i)altered[i].close+=1000;
    auto prior=mt::build_context(altered,b);
    for(const auto&[time,c]:ctx)if(time<=t+90*60){
        auto it=prior.find(time);assert(it!=prior.end());
        assert(std::abs(c.ret5-it->second.ret5)<1e-12);
        assert(std::abs(c.z60-it->second.z60)<1e-12);
    }
    // A missing minute invalidates its 5m block, rather than synthesizing a candle.
    auto gap=a;gap.erase(gap.begin()+22);
    auto broken=mt::completed_bars(gap,5);
    assert(broken.size()==19);
    std::cout<<"F3E-F2C.2: PASS (completed 5m/15m, causal features, missing minute, staleness)\n";
}
