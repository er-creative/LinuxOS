#include "devai/strategy/dev_ai_strategy/DevEventDrivenRotationReplay.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace devai::strategy {
namespace {
struct Pending {std::int64_t at; DevRotationIntent intent; std::string exit_symbol;};
std::string orderId(std::int64_t t, const std::string& manager, const std::string& side, const std::string& symbol) {
    return "event:"+manager+":"+std::to_string(t)+":"+side+":"+symbol;
}
}
DevRotationReplayResult DevEventDrivenRotationReplay::run(
    const std::vector<std::int64_t>& times,const std::string& manager,
    const DevRotationPolicy& policy,DevinderjeetSupervisor& supervisor,
    const DevRealMinuteMarket& market,double fraction,bool settle) const {
    if(times.empty()||manager!="DevManager_001"||!policy||!std::isfinite(fraction)||fraction<=0||fraction>1)
        throw std::invalid_argument("invalid single-manager replay configuration");
    const auto registered=supervisor.managerAccount(manager);
    if(registered.registration.manager_id!=manager || registered.registration.agents.size()!=5)
        throw std::invalid_argument("register DevManager_001 with five agents first");
    if(!registered.positions.empty()) throw std::invalid_argument("replay requires flat initial account");
    DevRotationReplayResult result{};
    auto& out=result.replay;
    out.initial_equity=supervisor.snapshot().equity;
    if(!(out.initial_equity>0)) throw std::invalid_argument("initial equity must be positive");
    double peak=out.initial_equity;
    std::optional<Pending> pending;
    std::int64_t previous=0;
    auto mark=[&](std::int64_t t){
        for(const auto& [sym,pos]:supervisor.managerAccount(manager).positions)
            supervisor.updateMark(sym,market.completedClose(sym,t));
    };
    auto record=[&](std::int64_t t,const std::string& sym,const std::string& label,DevOrderReport r){
        if(r.status==DevOrderStatus::FILLED) ++out.filled;
        if(r.status==DevOrderStatus::REJECTED) ++out.rejected;
        out.events.push_back({t,manager,sym,label,std::move(r)});
    };
    auto order=[&](std::int64_t t,const std::string& sym,const std::string& agent,
                   DevSupervisedAction action,std::int64_t shares,double px,const std::string& label){
        DevSupervisedOrder o{};
        o.order_id=orderId(t,manager,label,sym);o.manager_id=manager;
        o.agent_id=agent;o.symbol=sym;o.action=action;o.timestamp=t;
        o.shares=shares;o.market_price=px;
        auto r=supervisor.execute(o);
        const bool filled=r.status==DevOrderStatus::FILLED;
        record(t,sym,label,std::move(r));
        return filled;
    };
    auto fill=[&](const Pending& p){
        // Do not use this bar OPEN until the simulated clock reaches p.at.
        if(!p.exit_symbol.empty()) {
            const auto acct=supervisor.managerAccount(manager);
            const auto it=acct.positions.find(p.exit_symbol);
            if(it!=acct.positions.end()) {
                const auto b=market.nextExecutionBar(p.exit_symbol,p.at);
                if(b.start_epoch!=p.at) throw std::logic_error("exit bar shifted");
                const bool exited=order(p.at,p.exit_symbol,it->second.agent_id,
                    DevSupervisedAction::EXIT,0,b.open,"EXIT_NEXT_OPEN");
                if(!exited) return;
            }
        }
        if(p.intent.action!=DevRotationAction::BUY && p.intent.action!=DevRotationAction::ROTATE) return;
        const auto acct=supervisor.managerAccount(manager);
        if(!acct.positions.empty()) return;
        const auto b=market.nextExecutionBar(p.intent.symbol,p.at);
        if(b.start_epoch!=p.at) throw std::logic_error("entry bar shifted");
        const auto qty=static_cast<std::int64_t>(std::floor(acct.cash*fraction/b.open));
        if(qty>0) {
            const bool bought=order(p.at,p.intent.symbol,p.intent.agent_id,
                DevSupervisedAction::BUY,qty,b.open,"BUY_NEXT_OPEN");
            if(bought && p.intent.action==DevRotationAction::ROTATE) ++result.rotation_count;
        }
    };
    for(const auto t:times) {
        if(t<=previous) throw std::invalid_argument("non-increasing decision times");
        if(pending && pending->at<t) {
            fill(*pending); pending.reset();
        }
        // A pending fill at t executes before the decision at t; its OPEN
        // is never supplied to the policy. Portfolio accounting is separate.
        if(pending && pending->at==t) {fill(*pending);pending.reset();}
        mark(t);
        const auto intent=policy(t);
        ++result.decision_count;
        const auto acct=supervisor.managerAccount(manager);
        if(acct.positions.size()>1) throw std::logic_error("single-position invariant violated");
        const bool has_position=!acct.positions.empty();
        const std::string held=has_position?acct.positions.begin()->first:"";
        const bool wants_entry=intent.action==DevRotationAction::BUY||intent.action==DevRotationAction::ROTATE;
        if(wants_entry) {
            if(intent.symbol.empty()||intent.agent_id.empty()||
               std::find(acct.registration.agents.begin(),acct.registration.agents.end(),intent.agent_id)==acct.registration.agents.end())
                throw std::invalid_argument("invalid target or agent");
        }
        if(intent.action==DevRotationAction::ROTATE && !has_position)
            throw std::invalid_argument("ROTATE requires held position");
        if(intent.action==DevRotationAction::BUY && has_position)
            throw std::invalid_argument("BUY requires flat account");
        if(intent.action==DevRotationAction::ROTATE && intent.symbol==held)
            throw std::invalid_argument("rotation target equals held symbol");
        if(intent.action!=DevRotationAction::HOLD && !(intent.action==DevRotationAction::EXIT&&!has_position)) {
            std::int64_t at=0;
            try {
                if(has_position) at=market.nextExecutionBar(held,t).start_epoch;
                if(wants_entry) {
                    const auto entry_at=market.nextExecutionBar(intent.symbol,t).start_epoch;
                    if(at && at!=entry_at) {++result.skipped_no_bar;at=0;}
                    else at=entry_at;
                }
            } catch(const std::out_of_range&) {++result.skipped_no_bar;at=0;}
            if(at) {
                if(at<t||at>t+60) throw std::logic_error("non-causal execution bar");
                // Schedule the execution event; NEVER fill during inference.
                pending=Pending{at,intent,has_position?held:""};
            }
        }
        const double equity=supervisor.snapshot().equity;
        out.equity_curve.push_back(equity);
        peak=std::max(peak,equity);
        out.max_drawdown_fraction=std::max(out.max_drawdown_fraction,(peak-equity)/peak);
        previous=t;
    }
    // No orders may fill after the requested historical horizon.
    pending.reset();
    mark(previous);
    if(settle) {
        const auto acct=supervisor.managerAccount(manager);
        for(const auto& [sym,pos]:acct.positions)
            order(previous,sym,pos.agent_id,DevSupervisedAction::EXIT,0,
                market.completedClose(sym,previous),"TERMINAL_EXIT");
    }
    out.final_equity=supervisor.snapshot().equity;
    out.equity_curve.push_back(out.final_equity);
    out.net_pnl=out.final_equity-out.initial_equity;
    return result;
}
} // namespace devai::strategy
