#include "devai/benchmark/DevMaskablePPO.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace devai::benchmark;
void expect(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
int main() {
 try {
    DevMaskablePPO model;
    PPOState s{};s[0]=0.5;s[83]=-0.25;s[147]=1.0;
    PPOMask mask{};mask[0]=true;mask[2]=true;
    auto c=model.choose(s,mask,true);
    expect(mask[c.action],"illegal deterministic action");
    for(int i=0;i<20;++i) expect(model.choose(s,mask,true).action==c.action,
                                  "nondeterministic evaluation");
    for(int i=0;i<200;++i) expect(mask[model.choose(s,mask,false).action],
                                   "illegal stochastic action");
    bool rejected=false;
    try {model.choose(s,PPOMask{},true);} catch(const std::runtime_error&){rejected=true;}
    expect(rejected,"empty mask accepted");
    std::vector<PPOStep> steps;
    for(int i=0;i<16;++i) {
        auto x=model.choose(s,mask,false);
        PPOStep step{};
        step.state=s;step.legal=mask;step.action=x.action;
        step.old_log_probability=x.log_probability;step.old_value=x.value;
        step.reward=(i%3==0)?1.0:-0.1;step.terminal=(i==15);
        steps.push_back(step);
    }
    model.computeAdvantages(steps,0.0);
    for(const auto& x:steps) expect(std::isfinite(x.advantage) &&
       std::isfinite(x.target_value),"nonfinite advantage");
    model.update(steps);
    auto after=model.choose(s,mask,true);
    expect(std::isfinite(after.log_probability) &&
           std::isfinite(after.value),"invalid trained prediction");
    auto checkpoint=std::filesystem::temp_directory_path()/
        "cete_h3_maskable_ppo_test.chk";
    model.save(checkpoint);
    DevMaskablePPO restored;
    restored.load(checkpoint);
    auto r=restored.choose(s,mask,true);
    expect(r.action==after.action &&
           std::abs(r.log_probability-after.log_probability)<1e-12 &&
           std::abs(r.value-after.value)<1e-12,"checkpoint roundtrip mismatch");
    auto corrupt=std::filesystem::temp_directory_path()/
        "cete_h3_maskable_ppo_corrupt.chk";
    {std::ofstream out(corrupt);out<<"CETE_MASKABLE_PPO_V1\n148 32 7\n"; }
    rejected=false;
    try {restored.load(corrupt);}catch(const std::runtime_error&){rejected=true;}
    expect(rejected,"truncated checkpoint accepted");
    auto still=restored.choose(s,mask,true);
    expect(still.action==r.action &&
           std::abs(still.value-r.value)<1e-12,"failed load mutated model");
    std::filesystem::remove(checkpoint);std::filesystem::remove(corrupt);
    std::cout<<"CETE H3 MASKABLE PPO CORE PASS\n"
             <<"Legal-action masking: PASS\n"
             <<"Deterministic evaluation: PASS\n"
             <<"Stochastic sampling: PASS\n"
             <<"GAE and PPO optimization: PASS\n"
             <<"Checkpoint roundtrip and malformed rejection: PASS\n"
             <<"Historical CETE replay integration: NOT IMPLEMENTED\n"
             <<"Live trading: DISABLED\n";
    return 0;
 } catch(const std::exception& e) {
    std::cerr<<"CETE H3 MASKABLE PPO FAIL: "<<e.what()<<'\n';return 1;
 }
}
