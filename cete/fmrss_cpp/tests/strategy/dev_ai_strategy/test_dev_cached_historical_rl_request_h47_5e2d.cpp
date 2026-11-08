#include "devai/strategy/dev_ai_strategy/DevCachedHistoricalRLRequest_H47_5E2D.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai;
template<class F> bool rejects(F&& f) {
    try { f(); } catch(const std::exception&) { return true; }
    return false;
}
int main() {
    constexpr std::int64_t t=1792123200;
    statistics::MathematicalStateFeatures s{};
    s.stock_symbol="AXISBANK";s.benchmark_symbol="NIFTY 50";
    s.decision_time=t;s.fully_ready=true;s.numerical_integrity=true;
    s.one_minute_ready=true;s.five_minute_ready=true;s.fifteen_minute_ready=true;
    cache::HistoricalMathematicalFeatureCache::States states{{t,s}};
    auto encoder=[](const statistics::MathematicalStateFeatures& state) {
        assert(state.decision_time>0);
        std::array<double,benchmark::kInputCount> f{};
        f[0]=42.0; // TEST FIXTURE ONLY. Not the production 148-feature encoder.
        return f;
    };
    auto mask=[](const std::string&,std::int64_t,std::size_t) {
        std::array<bool,benchmark::kOutputCount> m{};
        m[0]=true;return m;
    };
    strategy::DevCachedHistoricalRLRequest_H47_5E2D provider(
        states,"AXISBANK","NIFTY 50",encoder,mask);
    auto r=provider("AXISBANK",t,0);
    assert(r.role==benchmark::DevRLRole::Agent && r.features[0]==42.0);
    assert(r.legal_actions[0]);
    assert(rejects([&]{(void)provider("AXISBANK",t+60,0);}));
    assert(rejects([&]{(void)provider("SBIN",t,0);}));
    assert(rejects([&]{(void)provider("AXISBANK",t,5);}));
    auto bad=s;bad.fully_ready=false;
    strategy::DevCachedHistoricalRLRequest_H47_5E2D invalid(
        {{t,bad}},"AXISBANK","NIFTY 50",encoder,mask);
    assert(rejects([&]{(void)invalid("AXISBANK",t,0);}));
    auto badmask=[](const std::string&,std::int64_t,std::size_t) {
        std::array<bool,benchmark::kOutputCount> m{};m[6]=true;return m;
    };
    strategy::DevCachedHistoricalRLRequest_H47_5E2D illegal(
        states,"AXISBANK","NIFTY 50",encoder,badmask);
    assert(rejects([&]{(void)illegal("AXISBANK",t,0);}));
    std::cout<<"5E.2D cached historical RL request contract PASS (test encoder only)\n";
}
