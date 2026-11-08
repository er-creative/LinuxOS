#include "devai/strategy/dev_ai_strategy/DevSharedReplayIntegrity.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
namespace {
bool sameNumber(double a,double b) { return std::isfinite(a) && std::isfinite(b) && a==b; }
bool sameVector(const std::vector<double>& a,const std::vector<double>& b) {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i) if(!sameNumber(a[i],b[i])) return false;
    return true;
}
bool same(const DevQNetworkParameters& a,const DevQNetworkParameters& b) {
    return sameVector(a.weights1,b.weights1)&&sameVector(a.biases1,b.biases1)&&
        sameVector(a.weights2,b.weights2)&&sameVector(a.biases2,b.biases2)&&
        sameVector(a.weights3,b.weights3)&&sameVector(a.biases3,b.biases3);
}
template<class P> bool sameManagerParams(const P& a,const P& b) {
    return sameVector(a.weights1,b.weights1)&&sameVector(a.biases1,b.biases1)&&
        sameVector(a.weights2,b.weights2)&&sameVector(a.biases2,b.biases2)&&
        sameVector(a.weights3,b.weights3)&&sameVector(a.biases3,b.biases3);
}
void compare(const DevRealReplayResult& a,const DevRealReplayResult& b) {
    const auto& x=a.replay;const auto& y=b.replay;
    if(a.input_ticks!=b.input_ticks || a.execution_bar_checks!=b.execution_bar_checks ||
       x.events.size()!=y.events.size() || x.equity_curve.size()!=y.equity_curve.size() ||
       x.filled!=y.filled || x.rejected!=y.rejected ||
       !sameNumber(x.initial_equity,y.initial_equity) ||
       !sameNumber(x.final_equity,y.final_equity) ||
       !sameNumber(x.net_pnl,y.net_pnl) ||
       !sameNumber(x.max_drawdown_fraction,y.max_drawdown_fraction))
        throw std::runtime_error("replay summaries differ");
    for(std::size_t i=0;i<x.equity_curve.size();++i)
        if(!sameNumber(x.equity_curve[i],y.equity_curve[i]))
            throw std::runtime_error("equity curves differ");
    for(std::size_t i=0;i<x.events.size();++i) {
        const auto& e=x.events[i];const auto& f=y.events[i];
        if(e.time!=f.time || e.manager_id!=f.manager_id ||
           e.symbol!=f.symbol || e.action!=f.action ||
           e.report.order_id!=f.report.order_id ||
           e.report.manager_id!=f.report.manager_id ||
           e.report.agent_id!=f.report.agent_id ||
           e.report.symbol!=f.report.symbol ||
           e.report.reason!=f.report.reason ||
           e.report.action!=f.report.action ||
           e.report.status!=f.report.status ||
           e.report.timestamp!=f.report.timestamp ||
           e.report.shares!=f.report.shares ||
           !sameNumber(e.report.execution_price,f.report.execution_price) ||
           !sameNumber(e.report.fee,f.report.fee) ||
           !sameNumber(e.report.realized_pnl,f.report.realized_pnl))
            throw std::runtime_error("execution event traces differ");
    }
}
}
DevSharedReplayIntegrityReport DevSharedReplayIntegrity::verify(
    const std::vector<DevUniverseRankingTick>& ranking,
    const std::vector<std::string>& universe,
    const DevQNetwork& agent_model,
    const DevManagerQNetwork& manager_model,
    const DevFreshReplayRun& run_fresh) {
    DevSharedReplayIntegrityReport report{};
    try {
        if(!run_fresh) throw std::invalid_argument("fresh replay callback required");
        const auto ticks=DevSingleManagerTop5Replay::prepare(ranking,universe);
        report.ticks=ticks.size();
        for(const auto& tick:ticks) {
            if(tick.managers.size()!=1 || tick.managers.front().manager_id!="DevManager_001" ||
               tick.managers.front().candidates.size()!=DEV_AGENT_SLOT_COUNT)
                throw std::runtime_error("single-manager top-five contract violated");
            for(std::size_t i=0;i<DEV_AGENT_SLOT_COUNT;++i) {
                const auto& c=tick.managers.front().candidates[i];
                if(c.slot!=i || c.candidate_rank!=i+1 ||
                   c.observation.decision_time!=tick.decision_time ||
                   c.observation.symbol!=c.symbol)
                    throw std::runtime_error("candidate mapping or time mismatch");
            }
        }
        const auto agent_before=agent_model.parameters();
        const auto manager_before=manager_model.parameters();
        const auto first=run_fresh(ticks);
        if(!same(agent_before,agent_model.parameters()) ||
           !sameManagerParams(manager_before,manager_model.parameters()))
            throw std::runtime_error("model parameters changed during first replay");
        const auto second=run_fresh(ticks);
        if(!same(agent_before,agent_model.parameters()) ||
           !sameManagerParams(manager_before,manager_model.parameters()))
            throw std::runtime_error("model parameters changed during second replay");
        if(first.input_ticks!=ticks.size() || second.input_ticks!=ticks.size())
            throw std::runtime_error("replay omitted input ticks");
        compare(first,second);
        report.events=first.replay.events.size();
        report.passed=true;
        report.reason="PASS: immutable models, top-five assignments, deterministic replay summaries and events";
    } catch(const std::exception& e) { report.reason=e.what(); }
    return report;
}
} // namespace devai::strategy
