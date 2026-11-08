#include "devai/strategy/dev_ai_strategy/DevSharedMultiManagerHistoricalReplay.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

namespace devai::strategy {
namespace {
double priceAt(const DevReplayCompletedPrice& p,const std::string& symbol,std::int64_t t) {
    const double v=p(symbol,t);
    if(!std::isfinite(v)||v<=0) throw std::runtime_error("missing/invalid completed price: "+symbol);
    return v;
}
std::string key(std::int64_t t,const std::string& manager,const std::string& suffix) {
    return "replay:"+std::to_string(t)+":"+manager+":"+suffix;
}
}
DevSharedMultiManagerHistoricalReplay::DevSharedMultiManagerHistoricalReplay(DevReplayConfig c):config_(c) {
    if(!std::isfinite(c.target_manager_allocation_fraction)||c.target_manager_allocation_fraction<=0||
       c.target_manager_allocation_fraction>1) throw std::invalid_argument("invalid allocation fraction");
}
DevReplayResult DevSharedMultiManagerHistoricalReplay::run(
    const std::vector<DevReplayTick>& ticks, DevSharedModelMultiManagerRuntime& runtime,
    DevinderjeetSupervisor& supervisor, const DevReplayCompletedPrice& completed_price) const {
    if(ticks.empty()||!completed_price) throw std::invalid_argument("ticks/prices required");
    DevReplayResult out;
    // Snapshot includes registered capital; register all allocations before calling run.
    out.initial_equity=supervisor.snapshot().equity;
    if(out.initial_equity<=0) throw std::invalid_argument("supervisor has no allocated equity");
    double peak=out.initial_equity;
    std::int64_t last=0;
    std::set<std::string> seen_managers;
    auto record=[&](std::int64_t t,const std::string& m,const std::string& s,
                    const std::string& a,DevOrderReport r){
        if(r.status==DevOrderStatus::FILLED) ++out.filled;
        if(r.status==DevOrderStatus::REJECTED) ++out.rejected;
        out.events.push_back({t,m,s,a,std::move(r)});
    };
    auto markHeld=[&](std::int64_t t){
        for(const auto& id:seen_managers) {
            const auto account=supervisor.managerAccount(id);
            for(const auto& [symbol,position]:account.positions)
                supervisor.updateMark(symbol,priceAt(completed_price,symbol,t));
        }
    };
    for(const auto& tick:ticks) {
        if(tick.decision_time<=last||tick.decision_time<=0)
            throw std::invalid_argument("replay ticks must be strictly increasing");
        last=tick.decision_time;
        std::set<std::string> current;
        for(const auto& input:tick.managers) {
            if(!current.insert(input.manager_id).second)
                throw std::invalid_argument("duplicate manager in tick");
            seen_managers.insert(input.manager_id);
        }
        // Mark all previously known held positions before model inference.
        markHeld(last);
        for(const auto& input:tick.managers) {
            const auto cycle=runtime.evaluate(input.manager_id,last,input.candidates);
            auto account=supervisor.managerAccount(input.manager_id);
            if(account.positions.size()>1) throw std::logic_error("one position per manager required");
            const bool held=!account.positions.empty();
            const std::string old=held?account.positions.begin()->first:"";
            // Manager's NO_TRADE / KEEP_CURRENT are never treated as a new BUY.
            if(cycle.decision.type!=DevManagerDecisionType::SELECT_AGENT) continue;
            const std::string desired=cycle.decision.selected_symbol;
            if(desired.empty()) continue;
            if(held && old==desired) continue;
            // Existing bridge permits EXIT on a SELECT_AGENT only if it selects
            // the held symbol. Rotation is therefore intentionally not attempted:
            // doing so would bypass the validated decision/execution contract.
            if(held) continue;
            const auto slot=cycle.decision.selected_slot_index;
            if(!slot || !cycle.state.agent_reports[*slot] ||
               cycle.state.agent_reports[*slot]->preferred_action!=DevAction::LONG) continue;
            const double px=priceAt(completed_price,desired,last);
            const double budget=account.cash*config_.target_manager_allocation_fraction;
            const auto quantity=static_cast<std::int64_t>(std::floor(budget/px));
            if(quantity<=0) continue;
            const auto r=runtime.submit(cycle,DevSupervisedAction::BUY,
                key(last,input.manager_id,"buy"),quantity,px);
            record(last,input.manager_id,desired,"BUY",r);
        }
        markHeld(last);
        const double equity=supervisor.snapshot().equity;
        out.equity_curve.push_back(equity);
        peak=std::max(peak,equity);
        out.max_drawdown_fraction=std::max(out.max_drawdown_fraction,(peak-equity)/peak);
    }
    // Terminal liquidation at the LAST decision boundary using a completed price.
    // No next-session data, incomplete bar open, or post-decision future close.
    if(config_.liquidate_at_end) {
        for(const auto& id:seen_managers) {
            const auto account=supervisor.managerAccount(id);
            for(const auto& [symbol,pos]:account.positions) {
                const auto px=priceAt(completed_price,symbol,last);
                DevSupervisedOrder order{};
                order.order_id=key(last,id,"terminal:"+symbol);
                order.manager_id=id;order.agent_id=pos.agent_id;order.symbol=symbol;
                order.action=DevSupervisedAction::EXIT;order.timestamp=last;
                order.market_price=px;
                // Settlement is a supervisor risk-checked liquidation, not a
                // fabricated DevManager model decision.
                record(last,id,symbol,"TERMINAL_EXIT",supervisor.execute(order));
            }
        }
        out.equity_curve.push_back(supervisor.snapshot().equity);
    }
    out.final_equity=supervisor.snapshot().equity;
    out.net_pnl=out.final_equity-out.initial_equity;
    return out;
}
} // namespace devai::strategy
