#include "devai/runtime/DevHistoricalRouterBackend.hpp"
#include "devai/strategy/dev_ai_strategy/DevFrozenOOSPolicy.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devai::runtime {
namespace {
void check(bool ok,const std::string& why) {
    if (!ok) throw std::runtime_error("Historical router: "+why);
}
bool validCost(double x) { return std::isfinite(x) && x>=0; }
}
void DevHistoricalRouterBackend::setInputs(const std::string& manager,RouteKind kind,HistoricalRunInputs inputs) {
    check(!manager.empty(),"missing manager");
    check(kind==RouteKind::BACKTEST || kind==RouteKind::FORWARD,"historical backend supports BACKTEST/FORWARD only");
    inputs_[{manager,kind}]=std::move(inputs);
}
void DevHistoricalRouterBackend::connect(DevModeAwareExecutionRouter& router) {
    router.connectTestingBackend(RouteKind::BACKTEST,[this](const ManagerCycleContext& ctx){(void)run(ctx,RouteKind::BACKTEST);});
    router.connectTestingBackend(RouteKind::FORWARD,[this](const ManagerCycleContext& ctx){(void)run(ctx,RouteKind::FORWARD);});
}
strategy::DevHistoricalOOSReport DevHistoricalRouterBackend::run(const ManagerCycleContext& ctx,RouteKind kind) const {
    check(kind==RouteKind::BACKTEST || kind==RouteKind::FORWARD,"unsupported route");
    check(ctx.direction=="LONG" || ctx.direction=="SHORT","invalid direction");
    check(ctx.top_k==5,"frozen OOS policy currently requires top_k=5");
    check(std::isfinite(ctx.capital) && ctx.capital>0,"invalid capital");
    check(std::isfinite(ctx.allocation_percent) && ctx.allocation_percent>0 && ctx.allocation_percent<=100,"invalid allocation");
    const auto it=inputs_.find({ctx.manager_id,kind});
    check(it!=inputs_.end(),"missing inputs for "+ctx.manager_id);
    const auto& in=it->second;
    check(!in.cache.empty() && !in.minute_folder.empty() && !in.agent_checkpoint.empty() &&
          !in.manager_checkpoint.empty() && !in.output_csv.empty(),"missing paths");
    check(validCost(in.brokerage_rate) && validCost(in.slippage_rate) && validCost(in.other_cost_rate),"invalid costs");
    check(in.output_csv!=in.cache && in.output_csv!=in.agent_checkpoint && in.output_csv!=in.manager_checkpoint,
          "output would overwrite an input");
    auto dataset=strategy::DevHistoricalTrainingCache::load(in.cache);
    strategy::DevHistoricalTrainingDatasetValidator::validate(dataset);
    check(!dataset.records.empty(),"empty historical dataset");
    if(kind==RouteKind::FORWARD) {
        check(in.last_training_decision_time>0,"FORWARD requires verified training cutoff");
        check(dataset.records.front().decision_time>in.last_training_decision_time,
              "FORWARD dataset overlaps training period");
    }
    std::set<std::string> symbols;
    for(const auto& record:dataset.records)
        for(const auto& c:record.candidates) symbols.insert(c.symbol);
    check(!symbols.empty(),"no candidates");
    strategy::DevHistoricalCloseStore prices;
    prices.loadMinuteFolder(in.minute_folder,{symbols.begin(),symbols.end()});
    strategy::DevFrozenOOSPolicyAdapter policy(in.agent_checkpoint,in.manager_checkpoint);
    strategy::DevOOSLedgerConfig ledger{};
    ledger.initial_capital=ctx.capital;
    ledger.max_allocation_fraction=ctx.allocation_percent/100.0;
    ledger.brokerage_rate=in.brokerage_rate;
    ledger.slippage_rate=in.slippage_rate;
    ledger.other_cost_rate=in.other_cost_rate;
    ledger.allow_intraday_short=(ctx.direction=="SHORT");
    strategy::DevHistoricalOOSRunner runner(ledger);
    auto report=runner.run(dataset,
        [&](const std::string& symbol,std::int64_t t){return prices.completedClose(symbol,t);},
        [&](const strategy::DevOOSDecisionPoint& point,const strategy::DevOOSLedgerState& state){
            const auto desired=policy(point,state);
            if(ctx.direction=="LONG") check(desired.rfind("SHORT:",0)!=0,"LONG manager requested SHORT");
            else if(!desired.empty()) check(desired.rfind("SHORT:",0)==0,"SHORT manager requested LONG");
            return desired;
        });
    strategy::DevHistoricalOOSRunner::writeCsv(in.output_csv,report);
    return report;
}
}
