#include "devai/strategy/dev_ai_strategy/reward/DevCostAwareRewardIntegration_D9F4B.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalExperienceIntegration.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
DevRotationIntent DevHistoricalExperienceIntegration::frozenModelDecision(
    const DevReplayTick& tick, DevSharedModelMultiManagerRuntime& runtime,
    const DevinderjeetSupervisor& supervisor) {
    if(tick.managers.size()!=1 || tick.managers.front().manager_id!="DevManager_001")
        throw std::invalid_argument("one manager required");
    const auto& m=tick.managers.front();
    const auto cycle=runtime.evaluate(m.manager_id,tick.decision_time,m.candidates);
    const auto account=supervisor.managerAccount(m.manager_id);
    if(account.positions.size()>1) throw std::logic_error("multiple positions not supported");
    if(cycle.action==DevManagerAction::NO_TRADE ||
       cycle.action==DevManagerAction::KEEP_CURRENT)
        return {DevRotationAction::HOLD,{},{}};
    if(!cycle.decision.selected_slot_index) throw std::logic_error("selection missing slot");
    const auto slot=*cycle.decision.selected_slot_index;
    if(slot>=m.candidates.size()) throw std::logic_error("selected slot missing");
    const auto& candidate=m.candidates[slot];
    const auto ids=DevSingleManagerTop5Replay::agentIds();
    if(candidate.slot!=slot) throw std::logic_error("candidate slot mismatch");
    const auto& agent=cycle.global_agent_reports.at(slot);
    if(agent.agent_name!=ids[slot] || agent.symbol!=candidate.symbol)
        throw std::logic_error("selected agent identity mismatch");
    if(account.positions.empty()) {
        if(agent.preferred_action==DevAction::LONG)
            return {DevRotationAction::BUY,candidate.symbol,ids[slot]};
        return {DevRotationAction::HOLD,{},{}};
    }
    const auto& held=account.positions.begin()->first;
    if(held==candidate.symbol) {
        if(agent.preferred_action==DevAction::EXIT)
            return {DevRotationAction::EXIT,{},{}};
        return {DevRotationAction::HOLD,{},{}};
    }
    if(agent.preferred_action==DevAction::LONG)
        return {DevRotationAction::ROTATE,candidate.symbol,ids[slot]};
    return {DevRotationAction::HOLD,{},{}};
}
DevHistoricalExperienceResult DevHistoricalExperienceIntegration::run(
    const std::vector<DevUniverseRankingTick>& ranking,
    const std::vector<std::string>& universe,
    DevSharedModelMultiManagerRuntime& runtime,
    DevinderjeetSupervisor& supervisor,
    const DevRealMinuteMarket& market,
    const DevHistoricalDecision& decision,
    double allocation_fraction,bool terminal_settlement) {
    if(!decision) throw std::invalid_argument("missing decision adapter");
    const auto ticks=DevSingleManagerTop5Replay::prepare(ranking,universe);
    const auto ids=DevSingleManagerTop5Replay::agentIds();
    const auto account=supervisor.managerAccount(DevSingleManagerTop5Replay::managerId);
    if(account.registration.agents!=std::vector<std::string>(ids.begin(),ids.end()))
        throw std::invalid_argument("supervisor agent registration mismatch");
    if(!account.positions.empty()) throw std::invalid_argument("requires flat initial portfolio");
    std::vector<std::int64_t> times;
    times.reserve(ticks.size());
    for(const auto& tick:ticks) times.push_back(tick.decision_time);
    std::size_t index=0;
    std::vector<DevRotationIntent> intents;
    intents.reserve(ticks.size());
    const auto policy=[&](std::int64_t t)->DevRotationIntent {
        if(index>=ticks.size() || ticks[index].decision_time!=t)
            throw std::logic_error("replay callback out of chronological order");
        auto intent=decision(ticks[index],runtime);
        if(intent.action==DevRotationAction::BUY || intent.action==DevRotationAction::ROTATE) {
            bool valid=false;
            for(const auto& c:ticks[index].managers.front().candidates)
                if(c.symbol==intent.symbol && ids[c.slot]==intent.agent_id) valid=true;
            if(!valid) throw std::invalid_argument("trade target not in current top-five slot");
        }
        intents.push_back(intent);
        ++index;
        return intent;
    };
    DevHistoricalExperienceResult result;
    result.execution=DevEventDrivenRotationReplay{}.run(times,"DevManager_001",policy,
        supervisor,market,allocation_fraction,terminal_settlement);
    if(index!=ticks.size() || result.execution.replay.equity_curve.size()!=ticks.size()+1)
        throw std::logic_error("incomplete event-driven replay accounting");
    const auto& eq=result.execution.replay.equity_curve;
    const double initial=result.execution.replay.initial_equity;
    if(!(initial>0) || !std::isfinite(initial)) throw std::logic_error("invalid starting equity");
    for(std::size_t i=0;i<ticks.size();++i) {
        DevHistoricalExperience sample{};
        sample.decision_time=ticks[i].decision_time;
        sample.candidates=ticks[i].managers.front().candidates;
        sample.intent=intents[i];
        // The policy at t_i produces a reward from t_i to t_(i+1).
        // At the last tick the reward includes optional terminal settlement.
        sample.equity_before=eq[i];
        sample.equity_after=eq[i+1];
        sample.portfolio_reward=(eq[i+1]-eq[i])/initial;
        sample.terminal=(i+1==ticks.size());
        if(!sample.terminal) {
            sample.next_decision_time=ticks[i+1].decision_time;
            sample.next_candidates=ticks[i+1].managers.front().candidates;
        }
        if(!std::isfinite(sample.equity_before)||!std::isfinite(sample.equity_after)||
           !std::isfinite(sample.portfolio_reward))
            throw std::logic_error("non-finite supervisor reward");
        result.experiences.push_back(std::move(sample));
    }
    // D9F.4B: reconcile historical rewards with net portfolio equity.
    // Costs are already reflected in the replay equity curve.
    devai::d9f4b::DevCostAwareRewardIntegration::validateHistorical(result);
    return result;
}
} // namespace devai::strategy


