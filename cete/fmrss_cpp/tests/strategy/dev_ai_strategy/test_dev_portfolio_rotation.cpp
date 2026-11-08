#include "devai/strategy/dev_ai_strategy/DevPortfolioRotation.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace devai::strategy;

struct TestStats { std::size_t checks{0}; std::size_t failures{0}; };

void require(bool ok, const std::string& text, TestStats& s)
{
    ++s.checks;
    if (ok) std::cout << "[PASS] " << text << '\n';
    else { ++s.failures; std::cerr << "[FAIL] " << text << '\n'; }
}

template <typename F>
void expectThrow(F&& f, const std::string& text, TestStats& s)
{
    bool threw=false;
    try { f(); } catch (const std::exception&) { threw=true; }
    require(threw, text, s);
}

bool same(double a,double b){ return std::fabs(a-b)<=1e-12; }

DevManagerPortfolioState flatPortfolio()
{
    DevManagerPortfolioState p{};
    p.portfolio.position=DEV_POSITION_FLAT;
    p.portfolio.drawdown=.01;
    p.numerical_integrity=true;
    return p;
}

DevManagerPortfolioState portfolio(const std::string& symbol,double position)
{
    auto p=flatPortfolio();
    p.current_symbol=symbol;
    p.portfolio.position=position;
    p.portfolio.unrealized_return=.01;
    p.portfolio.holding_period=5;
    return p;
}

DevAgentReport report(std::size_t slot,const std::string& symbol,std::int64_t t,
                      DevAction action,double score=.9)
{
    DevAgentReport r{};
    r.slot_index=slot;
    r.agent_name=DevTopCandidateSelector::agentNames()[slot];
    r.assigned=true;
    r.symbol=symbol;
    r.decision_time=t;
    r.candidate_score=score;
    r.candidate_rank=slot+1;
    r.q_values.values={-.2,-.3,-.4,-.5};
    r.q_values.values[static_cast<std::size_t>(action)]=.8;
    r.q_values.numerical_integrity=true;
    r.preferred_action=action;
    r.preferred_q_value=.8;
    r.numerical_integrity=true;
    return r;
}

void testAllocationConfig(TestStats& s)
{
    DevPortfolioRotationEngine e;
    require(same(e.config().deployed_capital_fraction,.75),
            "Default DevAI deployed capital fraction is 75%",s);
    require(same(1.0-e.config().deployed_capital_fraction,.25),
            "Default DevAI reserve capital fraction is 25%",s);
    expectThrow([]{ DevPortfolioRotationEngine e({0.0}); },
                "Rotation engine rejects zero deployed fraction",s);
    expectThrow([]{ DevPortfolioRotationEngine e({1.01}); },
                "Rotation engine rejects deployed fraction above 100%",s);
    expectThrow([]{ DevPortfolioRotationEngine e({std::numeric_limits<double>::quiet_NaN()}); },
                "Rotation engine rejects non-finite deployed fraction",s);
}

void testNoTrade(TestStats& s)
{
    DevManager m; DevPortfolioRotationEngine e; constexpr std::int64_t t=11000;
    auto flat=m.buildState(t,{},flatPortfolio());
    auto d=m.makeNoTradeDecision(flat);
    auto p=e.plan(flat,d,m);
    require(p.rotation_type==DevPortfolioRotationType::STAY_FLAT,"NO_TRADE while flat stays flat",s);
    require(p.legs.empty(),"NO_TRADE while flat creates no execution leg",s);
    require(same(p.deployed_capital_fraction,0)&&same(p.reserve_capital_fraction,1),
            "Flat NO_TRADE keeps 100% reserve",s);

    auto held=m.buildState(t,{},portfolio("RELIANCE",DEV_POSITION_LONG));
    auto hd=m.makeNoTradeDecision(held);
    auto hp=e.plan(held,hd,m);
    require(hp.rotation_type==DevPortfolioRotationType::EXIT_TO_FLAT,
            "NO_TRADE while holding means exit to flat",s);
    require(hp.legs.size()==1&&hp.legs[0].action==DevAction::EXIT&&hp.legs[0].symbol=="RELIANCE",
            "Exit-to-flat plan contains one explicit EXIT leg",s);
    require(same(hp.reserve_capital_fraction,1.0),"Exit-to-flat target is 100% reserve",s);
}

void testKeepCurrent(TestStats& s)
{
    DevManager m; DevPortfolioRotationEngine e; constexpr std::int64_t t=12000;
    auto st=m.buildState(t,{},portfolio("SBIN",DEV_POSITION_SHORT));
    auto d=m.makeKeepCurrentDecision(st);
    auto p=e.plan(st,d,m);
    require(p.rotation_type==DevPortfolioRotationType::KEEP_CURRENT,"KEEP_CURRENT preserves open position",s);
    require(p.target_symbol=="SBIN"&&same(p.target_position,DEV_POSITION_SHORT),
            "KEEP_CURRENT preserves symbol and direction",s);
    require(p.legs.empty(),"KEEP_CURRENT creates no unnecessary turnover",s);
    require(same(p.deployed_capital_fraction,.75)&&same(p.reserve_capital_fraction,.25),
            "KEEP_CURRENT preserves 75% deployed / 25% reserve policy",s);
}

void testEnterAndRotate(TestStats& s)
{
    DevManager m; DevPortfolioRotationEngine e; constexpr std::int64_t t=13000;
    auto r0=report(0,"RELIANCE",t,DevAction::LONG);
    auto flat=m.buildState(t,{r0},flatPortfolio());
    auto d=m.makeSelectAgentDecision(flat,0);
    auto p=e.plan(flat,d,m);
    require(p.rotation_type==DevPortfolioRotationType::ENTER_SELECTED,"Flat portfolio can enter selected DevAgent",s);
    require(p.target_symbol=="RELIANCE"&&same(p.target_position,DEV_POSITION_LONG),
            "Selected LONG report becomes long portfolio target",s);
    require(p.legs.size()==1&&p.legs[0].action==DevAction::LONG,"Entry has one LONG execution intent",s);
    require(same(p.legs[0].target_exposure_fraction,.75),"Entry requests exactly 75% target exposure",s);
    require(same(p.reserve_capital_fraction,.25),"Entry leaves exactly 25% reserve",s);

    auto r1=report(0,"INFY",t,DevAction::SHORT);
    auto held=m.buildState(t,{r1},portfolio("RELIANCE",DEV_POSITION_LONG));
    auto rd=m.makeSelectAgentDecision(held,0);
    auto rp=e.plan(held,rd,m);
    require(rp.rotation_type==DevPortfolioRotationType::ROTATE_TO_SELECTED,
            "Different selected symbol creates portfolio rotation",s);
    require(rp.legs.size()==2,"Cross-symbol rotation contains exactly two ordered legs",s);
    require(rp.legs[0].symbol=="RELIANCE"&&rp.legs[0].action==DevAction::EXIT,
            "Rotation closes current symbol first",s);
    require(rp.legs[1].symbol=="INFY"&&rp.legs[1].action==DevAction::SHORT,
            "Rotation enters selected symbol second",s);
    require(same(rp.legs[1].target_exposure_fraction,.75),"Rotated position requests 75% exposure",s);
}

void testSameSymbolBehavior(TestStats& s)
{
    DevManager m; DevPortfolioRotationEngine e; constexpr std::int64_t t=14000;
    auto same_long=report(0,"RELIANCE",t,DevAction::LONG);
    auto st=m.buildState(t,{same_long},portfolio("RELIANCE",DEV_POSITION_LONG));
    auto d=m.makeSelectAgentDecision(st,0);
    auto p=e.plan(st,d,m);
    require(p.rotation_type==DevPortfolioRotationType::KEEP_CURRENT&&p.legs.empty(),
            "Selecting already-held same direction avoids needless turnover",s);

    auto reverse=report(0,"RELIANCE",t,DevAction::SHORT);
    auto rs=m.buildState(t,{reverse},portfolio("RELIANCE",DEV_POSITION_LONG));
    auto rd=m.makeSelectAgentDecision(rs,0);
    auto rp=e.plan(rs,rd,m);
    require(rp.rotation_type==DevPortfolioRotationType::REVERSE_SELECTED,
            "Opposite selected direction creates explicit reversal",s);
    require(rp.legs.size()==2&&rp.legs[0].action==DevAction::EXIT&&rp.legs[1].action==DevAction::SHORT,
            "Reversal closes first and opens opposite direction second",s);
}

void testZeroToFiveAndBoundaries(TestStats& s)
{
    DevManager m; DevPortfolioRotationEngine e; constexpr std::int64_t t=15000;
    const std::array<std::string,5> syms{"A","B","C","D","E"};
    for(std::size_t n=0;n<=5;++n)
    {
        std::vector<DevAgentReport> rs;
        for(std::size_t i=0;i<n;++i) rs.push_back(report(i,syms[i],t,DevAction::LONG,1.0-.1*i));
        auto st=m.buildState(t,rs,flatPortfolio());
        auto nd=m.makeNoTradeDecision(st);
        auto np=e.plan(st,nd,m);
        require(np.numerical_integrity,"Rotation planning accepts "+std::to_string(n)+" assigned DevAgents",s);
        if(n>0)
        {
            auto sd=m.makeSelectAgentDecision(st,n-1);
            auto sp=e.plan(st,sd,m);
            require(sp.target_symbol==syms[n-1],"Rotation respects explicit selected slot for "+std::to_string(n)+" reports",s);
        }
    }

    auto hold_report=report(0,"RELIANCE",t,DevAction::HOLD);
    auto hs=m.buildState(t,{hold_report},flatPortfolio());
    auto hd=m.makeSelectAgentDecision(hs,0);
    expectThrow([&]{(void)e.plan(hs,hd,m);},
                "SELECT_AGENT rejects stock-level HOLD as new deployable target",s);

    auto exit_report=report(0,"RELIANCE",t,DevAction::EXIT);
    auto es=m.buildState(t,{exit_report},flatPortfolio());
    auto ed=m.makeSelectAgentDecision(es,0);
    expectThrow([&]{(void)e.plan(es,ed,m);},
                "SELECT_AGENT rejects stock-level EXIT as new deployable target",s);
}

void testTamperValidation(TestStats& s)
{
    DevManager m; DevPortfolioRotationEngine e; constexpr std::int64_t t=16000;
    auto r=report(0,"RELIANCE",t,DevAction::LONG);
    auto st=m.buildState(t,{r},flatPortfolio());
    auto d=m.makeSelectAgentDecision(st,0);
    auto p=e.plan(st,d,m);

    auto bad=p; bad.reserve_capital_fraction=.30;
    expectThrow([&]{e.validatePlan(st,d,bad,m);},"Plan validator rejects broken capital conservation",s);
    bad=p; bad.legs[0].target_exposure_fraction=.90;
    expectThrow([&]{e.validatePlan(st,d,bad,m);},"Plan validator rejects tampered 75% entry exposure",s);
    bad=p; bad.decision_time+=60;
    expectThrow([&]{e.validatePlan(st,d,bad,m);},"Plan validator rejects wrong decision time",s);
    bad=p; bad.target_symbol="SBIN";
    expectThrow([&]{e.validatePlan(st,d,bad,m);},"Plan validator rejects target not selected by DevManager",s);
}

} // namespace

int main()
{
    TestStats s{};
    std::cout<<"\n======================================================================\n";
    std::cout<<"DEVAI PHASE 3.18 — PORTFOLIO ROTATION / 75% ALLOCATION\n";
    std::cout<<"======================================================================\n";

    testAllocationConfig(s);
    testNoTrade(s);
    testKeepCurrent(s);
    testEnterAndRotate(s);
    testSameSymbolBehavior(s);
    testZeroToFiveAndBoundaries(s);
    testTamperValidation(s);

    std::cout<<"\n======================================================================\n";
    std::cout<<"PHASE 3.18 PORTFOLIO ROTATION / 75% ALLOCATION SUMMARY\n";
    std::cout<<"======================================================================\n";
    std::cout<<"Checks   : "<<s.checks<<'\n';
    std::cout<<"Failures : "<<s.failures<<'\n';
    std::cout<<"======================================================================\n";
    if(s.failures==0){std::cout<<"DEVAI PHASE 3.18 PORTFOLIO ROTATION / 75% ALLOCATION: PASSED\n";return 0;}
    std::cout<<"DEVAI PHASE 3.18 PORTFOLIO ROTATION / 75% ALLOCATION: FAILED\n";
    return 1;
}
