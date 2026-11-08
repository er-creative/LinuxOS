#include "devai/strategy/dev_ai_strategy/DevEpsilonGreedyPolicy.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string>

namespace
{
using namespace devai::strategy;

struct Stats { std::size_t checks{0}; std::size_t failures{0}; };

void check(bool ok, const std::string& text, Stats& s)
{
    ++s.checks;
    if (ok) std::cout << "[PASS] " << text << '\n';
    else { ++s.failures; std::cerr << "[FAIL] " << text << '\n'; }
}

template<class F>
void expectThrow(F&& f, const std::string& text, Stats& s)
{
    bool threw=false;
    try { f(); } catch (const std::exception&) { threw=true; }
    check(threw,text,s);
}

bool near(double a,double b,double t=1e-12)
{
    return std::fabs(a-b)<=t;
}

DevAIObservation observation()
{
    DevAIObservation o{};
    o.numerical_integrity=true;
    o.ready=true;
    return o;
}

DevQNetwork greedyLongNetwork()
{
    DevQNetwork n({.random_seed=123U,.zero_initialize=true});
    auto p=n.parameters();
    p.biases3[static_cast<std::size_t>(DevAction::HOLD)]=0.10;
    p.biases3[static_cast<std::size_t>(DevAction::LONG)]=0.80;
    p.biases3[static_cast<std::size_t>(DevAction::SHORT)]=-0.30;
    p.biases3[static_cast<std::size_t>(DevAction::EXIT)]=0.20;
    n.setParameters(p);
    return n;
}

void configuration(Stats& s)
{
    auto n=greedyLongNetwork();
    DevEpsilonGreedyPolicy p(n,{1.0,0.10,100,7U});
    check(near(p.currentEpsilon(),1.0),"Initial epsilon equals epsilon_start",s);
    check(p.completedActionSelections()==0,"Initial selection count is zero",s);

    expectThrow([&]{ DevEpsilonGreedyPolicy x(n,{-0.1,0.0,10,1U}); },
                "Negative epsilon_start rejected",s);
    expectThrow([&]{ DevEpsilonGreedyPolicy x(n,{1.1,0.0,10,1U}); },
                "epsilon_start above one rejected",s);
    expectThrow([&]{ DevEpsilonGreedyPolicy x(n,{0.5,0.6,10,1U}); },
                "epsilon_end above epsilon_start rejected",s);
    expectThrow([&]{ DevEpsilonGreedyPolicy x(n,{1.0,0.1,0,1U}); },
                "Zero decay_steps rejected",s);
}

void greedy(Stats& s)
{
    auto n=greedyLongNetwork();
    DevEpsilonGreedyPolicy p(n,{0.0,0.0,10,123U});
    for(int i=0;i<20;++i)
    {
        auto d=p.selectActionDetailed(observation(),1000+i*60);
        check(d.action==DevAction::LONG,"Epsilon zero selects greedy LONG",s);
        check(d.greedy && !d.exploratory,"Epsilon zero marks greedy decision",s);
    }
    check(p.completedActionSelections()==20,"Greedy selections counted",s);
}

void exploration(Stats& s)
{
    auto n=greedyLongNetwork();
    DevEpsilonGreedyPolicy p(n,{1.0,1.0,10,456U});
    bool h=false,l=false,sh=false,e=false;
    for(int i=0;i<200;++i)
    {
        auto d=p.selectActionDetailed(observation(),3000+i*60);
        check(d.exploratory && !d.greedy,"Epsilon one marks exploratory decision",s);
        h |= d.action==DevAction::HOLD;
        l |= d.action==DevAction::LONG;
        sh |= d.action==DevAction::SHORT;
        e |= d.action==DevAction::EXIT;
    }
    check(h,"Exploration reaches HOLD",s);
    check(l,"Exploration reaches LONG",s);
    check(sh,"Exploration reaches SHORT",s);
    check(e,"Exploration reaches EXIT",s);
}

void decay(Stats& s)
{
    auto n=greedyLongNetwork();
    DevEpsilonGreedyPolicy p(n,{1.0,0.0,4,99U});
    check(near(p.currentEpsilon(),1.0),"Decay starts at 1.00",s);
    (void)p.selectAction(observation(),5000);
    check(near(p.currentEpsilon(),0.75),"Decay step 1 = 0.75",s);
    (void)p.selectAction(observation(),5060);
    check(near(p.currentEpsilon(),0.50),"Decay step 2 = 0.50",s);
    (void)p.selectAction(observation(),5120);
    check(near(p.currentEpsilon(),0.25),"Decay step 3 = 0.25",s);
    (void)p.selectAction(observation(),5180);
    check(near(p.currentEpsilon(),0.0),"Decay boundary reaches epsilon_end",s);
    (void)p.selectAction(observation(),5240);
    check(near(p.currentEpsilon(),0.0),"Epsilon stays clamped at end",s);
}

void determinismReset(Stats& s)
{
    auto n=greedyLongNetwork();
    DevEpsilonGreedyPolicyConfig c{0.75,0.25,50,2026U};
    DevEpsilonGreedyPolicy a(n,c), b(n,c);
    bool identical=true;
    for(int i=0;i<40;++i)
    {
        auto x=a.selectActionDetailed(observation(),7000+i*60);
        auto y=b.selectActionDetailed(observation(),7000+i*60);
        if(x.action!=y.action || x.exploratory!=y.exploratory ||
           !near(x.epsilon,y.epsilon) || !near(x.random_draw,y.random_draw))
        { identical=false; break; }
    }
    check(identical,"Same seed produces deterministic policy sequence",s);
    a.reset();
    check(a.completedActionSelections()==0,"Reset clears selection counter",s);
    check(near(a.currentEpsilon(),c.epsilon_start),"Reset restores epsilon_start",s);
    DevEpsilonGreedyPolicy fresh(n,c);
    auto x=a.selectActionDetailed(observation(),10000);
    auto y=fresh.selectActionDetailed(observation(),10000);
    check(x.action==y.action && x.exploratory==y.exploratory &&
          near(x.random_draw,y.random_draw),
          "Reset restores RNG sequence",s);
}

void validationAndReadOnly(Stats& s)
{
    auto n=greedyLongNetwork();
    auto before=n.parameters();
    DevEpsilonGreedyPolicy p(n,{0.5,0.1,100,1234U});
    for(int i=0;i<50;++i) (void)p.selectAction(observation(),12000+i*60);
    auto after=n.parameters();
    check(before.weights1==after.weights1 && before.biases1==after.biases1 &&
          before.weights2==after.weights2 && before.biases2==after.biases2 &&
          before.weights3==after.weights3 && before.biases3==after.biases3,
          "Policy never mutates online Q-network",s);

    expectThrow([&]{ auto o=observation(); o.ready=false; (void)p.selectAction(o,20000); },
                "Unready observation rejected",s);
    expectThrow([&]{ auto o=observation(); o.numerical_integrity=false; (void)p.selectAction(o,20000); },
                "Invalid observation rejected",s);
    expectThrow([&]{ (void)p.selectAction(observation(),0); },
                "Non-positive decision time rejected",s);
}

void contract(Stats& s)
{
    auto n=greedyLongNetwork();
    DevEpsilonGreedyPolicy ep(n,{0.0,0.0,10,5U});
    DevPolicy& p=ep;
    check(p.name()==std::string_view("DevEpsilonGreedyPolicy"),
          "Implements DevPolicy name contract",s);
    check(p.selectAction(observation(),25000)==DevAction::LONG,
          "Works through DevPolicy interface",s);
    check(n.parameterCount()==7652,"Preserves 7,652 parameters",s);
    check(DEV_Q_INPUT_SIZE==84,"Preserves 84-value input",s);
    check(DEV_Q_OUTPUT_SIZE==4,"Preserves four actions",s);
}

}

int main()
{
    Stats s{};
    std::cout << "\n======================================================================\n"
              << "DEVAI PHASE 3.8E — EPSILON-GREEDY LEARNING POLICY TEST\n"
              << "======================================================================\n";
    try {
        configuration(s); greedy(s); exploration(s); decay(s);
        determinismReset(s); validationAndReadOnly(s); contract(s);
    } catch(const std::exception& e) {
        ++s.failures;
        std::cerr << "[FATAL] " << e.what() << '\n';
    }
    std::cout << "\n======================================================================\n"
              << "PHASE 3.8E TEST SUMMARY\n"
              << "======================================================================\n"
              << "Checks   : " << s.checks << '\n'
              << "Failures : " << s.failures << '\n'
              << "======================================================================\n";
    if(s.failures==0) {
        std::cout << "DEVAI PHASE 3.8E EPSILON-GREEDY LEARNING POLICY: PASSED\n"
                  << "======================================================================\n";
        return 0;
    }
    std::cerr << "DEVAI PHASE 3.8E EPSILON-GREEDY LEARNING POLICY: FAILED\n"
              << "======================================================================\n";
    return 1;
}
