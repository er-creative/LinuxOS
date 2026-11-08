#include "devai/strategy/dev_ai_strategy/DevTargetNetworkSynchronizer.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <string>

namespace {
using namespace devai::strategy;

struct Stats { std::size_t checks{0}; std::size_t failures{0}; };

void check(bool condition, const std::string& text, Stats& s) {
    ++s.checks;
    if (condition) std::cout << "[PASS] " << text << '\n';
    else { ++s.failures; std::cerr << "[FAIL] " << text << '\n'; }
}

template<class F>
void throws(F&& f, const std::string& text, Stats& s) {
    bool did=false;
    try { f(); } catch (const std::exception&) { did=true; }
    check(did,text,s);
}

bool same(const DevQNetworkParameters& a,const DevQNetworkParameters& b) {
    return a.weights1==b.weights1 && a.biases1==b.biases1 &&
           a.weights2==b.weights2 && a.biases2==b.biases2 &&
           a.weights3==b.weights3 && a.biases3==b.biases3;
}

void perturb(DevQNetwork& n,double x) {
    auto p=n.parameters();
    const auto a=static_cast<std::size_t>(DevAction::LONG);
    p.biases3[a]+=x;
    p.weights3[a*DEV_Q_HIDDEN2_SIZE]+=x*0.5;
    n.setParameters(p);
}

void testConfig(Stats& s) {
    DevTargetNetworkSynchronizer sync({.synchronization_interval=3});
    check(sync.config().synchronization_interval==3,"Configuration preserves interval",s);
    check(sync.completedOnlineUpdates()==0,"Initial total-update count is zero",s);
    check(sync.updatesSinceSynchronization()==0,"Initial interval count is zero",s);
    throws([]{ DevTargetNetworkSynchronizer x({.synchronization_interval=0}); (void)x; },
           "Zero interval is rejected",s);
}

void testInitialSync(Stats& s) {
    DevQNetwork online({.random_seed=11U,.zero_initialize=false});
    DevQNetwork target({.random_seed=22U,.zero_initialize=false});
    check(!same(online.parameters(),target.parameters()),"Different seeds initially differ",s);
    DevTargetNetworkSynchronizer sync({.synchronization_interval=3});
    auto r=sync.reset(online,target);
    check(r.synchronized,"Reset performs initial hard synchronization",s);
    check(r.numerical_integrity,"Reset result has numerical integrity",s);
    check(same(online.parameters(),target.parameters()),"Initial sync copies all parameters",s);
    check(r.completed_online_updates==0,"Reset total count is zero",s);
    check(r.updates_since_synchronization==0,"Reset interval count is zero",s);
}

void testBoundaryAndFrozenTarget(Stats& s) {
    DevQNetwork online({.random_seed=31U,.zero_initialize=false});
    DevQNetwork target({.random_seed=32U,.zero_initialize=false});
    DevTargetNetworkSynchronizer sync({.synchronization_interval=3});
    (void)sync.reset(online,target);
    auto frozen=target.parameters();

    perturb(online,0.1);
    auto r1=sync.onOnlineUpdateCompleted(online,target);
    check(!r1.synchronized,"Update 1 does not synchronize",s);
    check(r1.completed_online_updates==1,"Update 1 increments total count",s);
    check(r1.updates_since_synchronization==1,"Update 1 increments interval count",s);
    check(same(frozen,target.parameters()),"Target remains frozen after update 1",s);

    perturb(online,0.2);
    auto r2=sync.onOnlineUpdateCompleted(online,target);
    check(!r2.synchronized,"Update 2 does not synchronize",s);
    check(r2.updates_since_synchronization==2,"Update 2 increments interval count",s);
    check(same(frozen,target.parameters()),"Target remains frozen after update 2",s);
    check(!same(online.parameters(),target.parameters()),"Online may diverge from target",s);

    perturb(online,0.3);
    auto online_before=online.parameters();
    auto r3=sync.onOnlineUpdateCompleted(online,target);
    check(r3.synchronized,"Update 3 synchronizes at exact boundary",s);
    check(r3.completed_online_updates==3,"Boundary preserves total update count",s);
    check(r3.updates_since_synchronization==0,"Boundary resets interval count",s);
    check(same(online.parameters(),target.parameters()),"Boundary copies online to target",s);
    check(same(online_before,online.parameters()),"Synchronization never mutates online network",s);
}

void testRepeatedCycles(Stats& s) {
    DevQNetwork online({.random_seed=41U,.zero_initialize=false});
    DevQNetwork target({.random_seed=42U,.zero_initialize=false});
    DevTargetNetworkSynchronizer sync({.synchronization_interval=2});
    (void)sync.reset(online,target);

    for(int cycle=0;cycle<3;++cycle) {
        perturb(online,0.1+cycle);
        auto a=sync.onOnlineUpdateCompleted(online,target);
        check(!a.synchronized,"First update in cycle keeps target frozen",s);
        perturb(online,0.2+cycle);
        auto b=sync.onOnlineUpdateCompleted(online,target);
        check(b.synchronized,"Second update in cycle synchronizes",s);
        check(same(online.parameters(),target.parameters()),"Cycle ends with identical networks",s);
    }
    check(sync.completedOnlineUpdates()==6,"Repeated cycles preserve cumulative update count",s);
    check(sync.updatesSinceSynchronization()==0,"Repeated cycles end at zero interval count",s);
}

void testIntervalOneAndExplicitSync(Stats& s) {
    DevQNetwork online({.random_seed=51U,.zero_initialize=false});
    DevQNetwork target({.random_seed=52U,.zero_initialize=false});
    DevTargetNetworkSynchronizer sync({.synchronization_interval=1});
    (void)sync.reset(online,target);
    perturb(online,0.5);
    auto r=sync.onOnlineUpdateCompleted(online,target);
    check(r.synchronized,"Interval one synchronizes every update",s);
    check(same(online.parameters(),target.parameters()),"Interval one copies current online parameters",s);

    DevTargetNetworkSynchronizer sync2({.synchronization_interval=10});
    (void)sync2.reset(online,target);
    perturb(online,0.7);
    auto stale=sync2.onOnlineUpdateCompleted(online,target);
    check(!stale.synchronized,"Explicit-sync precondition leaves target stale",s);
    check(!same(online.parameters(),target.parameters()),"Networks differ before explicit sync",s);
    auto now=sync2.synchronizeNow(online,target);
    check(now.synchronized,"synchronizeNow performs hard copy",s);
    check(same(online.parameters(),target.parameters()),"synchronizeNow makes networks identical",s);
    check(now.completed_online_updates==1,"Explicit sync does not fabricate training update",s);
    check(now.updates_since_synchronization==0,"Explicit sync resets interval counter",s);
}

void testResetAndArchitecture(Stats& s) {
    DevQNetwork online({.random_seed=61U,.zero_initialize=false});
    DevQNetwork target({.random_seed=62U,.zero_initialize=false});
    DevTargetNetworkSynchronizer sync({.synchronization_interval=5});
    (void)sync.reset(online,target);
    for(int i=0;i<3;++i) {
        perturb(online,0.1);
        (void)sync.onOnlineUpdateCompleted(online,target);
    }
    check(sync.completedOnlineUpdates()==3,"Precondition records prior training updates",s);
    auto r=sync.reset(online,target);
    check(r.synchronized,"Reset after training performs hard sync",s);
    check(r.completed_online_updates==0,"Reset clears total counter",s);
    check(r.updates_since_synchronization==0,"Reset clears interval counter",s);
    check(same(online.parameters(),target.parameters()),"Reset copies current online parameters",s);
    check(online.parameterCount()==7652,"Online architecture remains 7,652 parameters",s);
    check(target.parameterCount()==7652,"Target architecture remains 7,652 parameters",s);
    check(DEV_Q_INPUT_SIZE==84,"Frozen 84-value input remains unchanged",s);
    check(DEV_Q_OUTPUT_SIZE==4,"Frozen four-action output remains unchanged",s);
}

}

int main() {
    Stats s{};
    std::cout << "\n======================================================================\n"
              << "DEVAI PHASE 3.8D — TARGET NETWORK SYNCHRONIZATION TEST\n"
              << "======================================================================\n";
    try {
        testConfig(s);
        testInitialSync(s);
        testBoundaryAndFrozenTarget(s);
        testRepeatedCycles(s);
        testIntervalOneAndExplicitSync(s);
        testResetAndArchitecture(s);
    } catch(const std::exception& e) {
        ++s.failures;
        std::cerr << "[FATAL] " << e.what() << '\n';
    }
    std::cout << "\n======================================================================\n"
              << "PHASE 3.8D TEST SUMMARY\n"
              << "======================================================================\n"
              << "Checks   : " << s.checks << '\n'
              << "Failures : " << s.failures << '\n'
              << "======================================================================\n";
    if(s.failures==0) {
        std::cout << "DEVAI PHASE 3.8D TARGET NETWORK SYNCHRONIZATION: PASSED\n"
                  << "======================================================================\n";
        return 0;
    }
    std::cerr << "DEVAI PHASE 3.8D TARGET NETWORK SYNCHRONIZATION: FAILED\n"
              << "======================================================================\n";
    return 1;
}
