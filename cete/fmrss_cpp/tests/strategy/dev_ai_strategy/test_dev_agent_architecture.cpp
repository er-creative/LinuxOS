#include "devai/strategy/dev_ai_strategy/DevAgent.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

using namespace devai::strategy;

namespace
{
int checks = 0;
int failures = 0;

void check(bool condition, const std::string& message)
{
    ++checks;
    if (condition) std::cout << "[PASS] " << message << '\n';
    else { ++failures; std::cout << "[FAIL] " << message << '\n'; }
}

template <typename F>
void rejects(F&& fn, const std::string& message)
{
    try { fn(); check(false, message); }
    catch (const std::exception&) { check(true, message); }
}

DevAIObservation observationFor(const DevAgentAssignment& a, double seed)
{
    DevAIObservation o{};
    o.agent_name = a.agent_name;
    o.symbol = a.symbol;
    o.benchmark_symbol = "NIFTY%2050";
    o.decision_time = a.decision_time;
    for (std::size_t i = 0; i < o.market.size(); ++i)
        o.market[i] = std::fmod(seed + static_cast<double>(i) * 0.031, 1.8) - 0.9;
    o.availability.fill(1.0);
    o.portfolio = {};
    o.numerical_integrity = true;
    o.ready = true;
    return o;
}

DevTopCandidateSelection selectionOf(std::size_t count)
{
    DevTopCandidateSelection s{};
    s.decision_time = 1000;
    s.benchmark_symbol = "NIFTY%2050";
    s.input_candidate_count = count;
    s.eligible_candidate_count = count;
    s.selected_candidate_count = count;
    s.numerical_integrity = true;
    const std::array<std::string,5> symbols{"RELIANCE","SBIN","BHEL","INFY","HDFCBANK"};
    const auto& names = DevTopCandidateSelector::agentNames();
    for (std::size_t i=0;i<DEV_AGENT_SLOT_COUNT;++i)
    {
        s.assignments[i].slot_index=i;
        s.assignments[i].agent_name=names[i];
        if (i<count)
        {
            s.assignments[i].symbol=symbols[i];
            s.assignments[i].decision_time=s.decision_time;
            s.assignments[i].candidate_score=1.0-static_cast<double>(i)*0.1;
            s.assignments[i].candidate_rank=(i+1);
            s.assignments[i].assigned=true;
        }
    }
    return s;
}
} // namespace

int main()
{
    std::cout << "\n======================================================================\n"
              << "DEVAI PHASE 3.12 — DEVAGENTS ARCHITECTURE\n"
              << "======================================================================\n";

    DevQNetwork network{DevQNetworkConfig{.random_seed=12345U,.zero_initialize=false}};
    DevAgents agents{network};
    const auto& names=DevTopCandidateSelector::agentNames();

    check(DEV_AGENT_SLOT_COUNT==5,"Exactly five DevAgent slots remain frozen");


    rejects([&]{DevAgents rejection_agents{network};auto bad = selectionOf(5);bad.assignments[4].candidate_rank = 10;rejection_agents.applySelection(bad);}, 
            "DevAgents reject legacy rank-10 fifth-slot assignment");
    check(agents.assignedCount()==0,
          "Rejected rank-10 selection leaves all five agents EMPTY");
    for(std::size_t i=0;i<5;++i)
    {
        check(agents.agent(i).name()==names[i],"DevAgent slot preserves frozen identity " + names[i]);
        check(!agents.agent(i).assigned(),"DevAgent starts EMPTY " + names[i]);
    }

    for(std::size_t count=0;count<=5;++count)
    {
        auto s=selectionOf(count);
        std::cout << "[DIAGNOSTIC] applying count=" << count;
        for (std::size_t i = 0; i < DEV_AGENT_SLOT_COUNT; ++i)
            if (s.assignments[i].assigned)
                std::cout << " slot=" << i << ":rank=" << s.assignments[i].candidate_rank;
        std::cout << std::endl;
        agents.applySelection(s);
        check(agents.assignedCount()==count,"Selection with " + std::to_string(count) + " candidates assigns exactly " + std::to_string(count) + " agents");
        for(std::size_t i=0;i<5;++i)
            check(agents.agent(i).assigned()==(i<count),"Five-slot dynamic assignment preserves EMPTY/assigned state for slot " + std::to_string(i+1));
    }

    auto five=selectionOf(5);
    agents.applySelection(five);
    rejects([&]{DevAgents rejection_agents{network};auto bad = selectionOf(5);bad.assignments[4].candidate_rank = 10;rejection_agents.applySelection(bad);}, 
            "DevAgents reject legacy rank-10 fifth-slot assignment");
    check(agents.assignedCount()==5 && agents.agent(4).assignment().candidate_rank==5,
          "Rejected replacement preserves previous five LONG assignments");
    for(std::size_t i=0;i<5;++i)
    {
        const auto& a=agents.agent(i);
        auto obs=observationFor(a.assignment(),0.11*static_cast<double>(i+1));
        const auto r=a.evaluate(obs);
        check(r.assigned,"Assigned DevAgent produces an assigned report");
        check(r.agent_name==a.name(),"Report preserves DevAgent identity");
        check(r.symbol==a.assignment().symbol,"Report preserves assigned symbol");
        check(r.decision_time==a.assignment().decision_time,"Report preserves causal decision time");
        check(r.candidate_rank==(i+1),"Report preserves contiguous LONG rank 1..5");
        check(r.q_values.numerical_integrity,"Shared Q-network output has numerical integrity");
        check(isValidAction(r.preferred_action),"DevAgent preferred action is legal");
        check(std::isfinite(r.preferred_q_value),"DevAgent preferred Q-value is finite");
        check(r.numerical_integrity,"DevAgent report has numerical integrity");
    }

    agents.applySelection(selectionOf(1));
    check(agents.agent(0).assigned(),"One-candidate session assigns DevAgent_001");
    for(std::size_t i=1;i<5;++i)
        check(!agents.agent(i).assigned(),"One-candidate session leaves remaining DevAgent EMPTY");
    rejects([&]{ auto dummy=DevAIObservation{}; agents.agent(1).evaluate(dummy); },"EMPTY DevAgent cannot evaluate or manufacture a recommendation");

    const auto a0=agents.agent(0).assignment();
    auto good=observationFor(a0,0.3);
    auto wrong_symbol=good; wrong_symbol.symbol="SBIN";
    rejects([&]{ agents.agent(0).evaluate(wrong_symbol); },"DevAgent rejects observation for another stock");
    auto wrong_time=good; wrong_time.decision_time+=60;
    rejects([&]{ agents.agent(0).evaluate(wrong_time); },"DevAgent rejects observation from another decision time");
    auto not_ready=good; not_ready.ready=false;
    rejects([&]{ agents.agent(0).evaluate(not_ready); },"DevAgent rejects observation that is not ready");
    auto bad_integrity=good; bad_integrity.numerical_integrity=false;
    rejects([&]{ agents.agent(0).evaluate(bad_integrity); },"DevAgent rejects observation without numerical integrity");

    auto bad_assignment=a0; bad_assignment.agent_name="DevAgent_005";
    rejects([&]{ agents.agent(0).assign(bad_assignment); },"DevAgent rejects assignment belonging to another agent identity");

    agents.clear();
    check(agents.assignedCount()==0,"clear() returns all five DevAgents to EMPTY state");

    std::cout << "\n======================================================================\n"
              << "PHASE 3.12 DEVAGENTS ARCHITECTURE SUMMARY\n"
              << "======================================================================\n"
              << "Checks   : " << checks << '\n'
              << "Failures : " << failures << '\n'
              << "======================================================================\n";
    if(failures==0) std::cout << "DEVAI PHASE 3.12 DEVAGENTS ARCHITECTURE: PASSED\n";
    else std::cout << "DEVAI PHASE 3.12 DEVAGENTS ARCHITECTURE: FAILED\n";
    return failures==0 ? 0 : 1;
}
