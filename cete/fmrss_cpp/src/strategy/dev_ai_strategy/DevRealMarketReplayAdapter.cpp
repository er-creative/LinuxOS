#include "devai/strategy/dev_ai_strategy/DevRealMarketReplayAdapter.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace devai::strategy {
namespace {
std::int64_t istEpoch(const std::string& date, const std::string& time) {
    std::tm tm{};
    std::istringstream ss(date+" "+time);
    ss >> std::get_time(&tm,"%Y-%m-%d %H:%M:%S");
    if(ss.fail()) throw std::invalid_argument("invalid IST date/time: "+date+" "+time);
    // timegm treats the calendar as UTC; subtract IST's UTC+05:30 offset.
    const auto utc=::timegm(&tm);
    if(utc == static_cast<std::time_t>(-1)) throw std::invalid_argument("invalid timestamp");
    return static_cast<std::int64_t>(utc)-19800;
}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::istringstream ss(line);std::string s;
    while(std::getline(ss,s,',')) out.push_back(s);
    return out;
}
double number(const std::string& s) {
    std::size_t used=0;const auto v=std::stod(s,&used);
    if(used!=s.size()||!std::isfinite(v)) throw std::invalid_argument("invalid OHLCV number");
    return v;
}
std::string orderId(std::int64_t t,const std::string& id,const std::string& action){
    return "realreplay:"+std::to_string(t)+":"+id+":"+action;
}
}
void DevRealMinuteMarket::load(const std::string& symbol,const std::filesystem::path& path) {
    if(symbol.empty()) throw std::invalid_argument("empty symbol");
    std::ifstream in(path);if(!in) throw std::runtime_error("cannot open: "+path.string());
    std::vector<DevRealMinuteBar> v;std::string line;
    while(std::getline(in,line)) {
        if(line.empty()) continue;
        const auto f=split(line);
        if(f.size()!=8) throw std::invalid_argument("expected Symbol,Date,Time,Open,High,Low,Close,Volume");
        if(f[0]=="Symbol") continue;
        if(f[0]!=symbol) throw std::invalid_argument("symbol mismatch in "+path.string());
        DevRealMinuteBar b{istEpoch(f[1],f[2]),number(f[3]),number(f[4]),number(f[5]),number(f[6]),number(f[7])};
        if(b.open<=0||b.low<=0||b.high<b.low||b.high<std::max(b.open,b.close)||
           b.low>std::min(b.open,b.close)||b.volume<0)
            throw std::invalid_argument("invalid minute candle");
        if(!v.empty()&&b.start_epoch<=v.back().start_epoch)
            throw std::invalid_argument("minute data not strictly chronological");
        v.push_back(b);
    }
    if(v.empty()) throw std::invalid_argument("no bars for "+symbol);
    bars_[symbol]=std::move(v);
}
std::size_t DevRealMinuteMarket::barCount(const std::string& symbol) const {
    return bars_.at(symbol).size();
}
double DevRealMinuteMarket::completedClose(const std::string& symbol,std::int64_t t) const {
    const auto& v=bars_.at(symbol);
    const auto it=std::upper_bound(v.begin(),v.end(),t-60,
        [](std::int64_t x,const DevRealMinuteBar& b){return x<b.start_epoch;});
    if(it==v.begin()) throw std::out_of_range("no completed bar: "+symbol);
    return std::prev(it)->close;
}
DevRealMinuteBar DevRealMinuteMarket::nextExecutionBar(const std::string& symbol,std::int64_t t) const {
    const auto& v=bars_.at(symbol);
    const auto it=std::lower_bound(v.begin(),v.end(),t,
        [](const DevRealMinuteBar& b,std::int64_t x){return b.start_epoch<x;});
    if(it==v.end()||it->start_epoch>t+60)
        throw std::out_of_range("no immediate execution bar: "+symbol);
    return *it;
}
std::vector<DevReplayTick> makeRealMultiManagerTicks(
    const std::map<std::string,DevHistoricalTrainingDataset>& datasets,
    std::int64_t begin,std::int64_t end) {
    if(datasets.empty()||begin<=0||end<begin) throw std::invalid_argument("invalid replay interval");
    std::map<std::int64_t,DevReplayTick> merged;
    for(const auto& [manager,dataset]:datasets) {
        if(manager.empty()) throw std::invalid_argument("empty manager id");
        DevHistoricalTrainingDatasetValidator::validate(dataset);
        for(const auto& r:dataset.records) {
            if(r.decision_time<begin||r.decision_time>end) continue;
            std::vector<DevSharedCandidate> candidates;
            for(const auto& c:r.candidates) {
                DevSharedCandidate x{};x.slot=c.slot_index;x.symbol=c.symbol;
                x.candidate_score=c.candidate_score;x.candidate_rank=c.candidate_rank;
                x.observation=c.observation; // NEVER copy c.outcomes into inference
                candidates.push_back(std::move(x));
            }
            auto& tick=merged[r.decision_time];tick.decision_time=r.decision_time;
            for(const auto& m:tick.managers)
                if(m.manager_id==manager) throw std::invalid_argument("duplicate manager/time");
            tick.managers.push_back({manager,std::move(candidates)});
        }
    }
    std::vector<DevReplayTick> ticks;
    for(auto& [t,v]:merged) ticks.push_back(std::move(v));
    if(ticks.empty()) throw std::invalid_argument("no replay ticks");
    return ticks;
}
DevRealReplayResult DevRealMarketMultiManagerReplay::run(
    const std::vector<DevReplayTick>& ticks,DevSharedModelMultiManagerRuntime& runtime,
    DevinderjeetSupervisor& supervisor,const DevRealMinuteMarket& market,double fraction) const {
    if(ticks.empty()||!std::isfinite(fraction)||fraction<=0||fraction>1)
        throw std::invalid_argument("invalid replay input");
    DevRealReplayResult result{};auto& out=result.replay;
    out.initial_equity=supervisor.snapshot().equity;
    if(out.initial_equity<=0) throw std::invalid_argument("no allocated capital");
    double peak=out.initial_equity;std::int64_t previous=0;
    std::set<std::string> managers;
    auto mark=[&](std::int64_t t) {
        for(const auto& id:managers) {
            for(const auto& [symbol,pos]:supervisor.managerAccount(id).positions)
                supervisor.updateMark(symbol,market.completedClose(symbol,t));
        }
    };
    auto report=[&](std::int64_t t,const std::string& manager,const std::string& symbol,
                    const std::string& action,DevOrderReport r) {
        if(r.status==DevOrderStatus::FILLED) ++out.filled;
        if(r.status==DevOrderStatus::REJECTED) ++out.rejected;
        out.events.push_back({t,manager,symbol,action,std::move(r)});
    };
    for(const auto& tick:ticks) {
        if(tick.decision_time<=previous) throw std::invalid_argument("nonchronological decisions");
        previous=tick.decision_time;
        std::set<std::string> current;
        for(const auto& m:tick.managers) {
            if(!current.insert(m.manager_id).second) throw std::invalid_argument("duplicate manager");
            managers.insert(m.manager_id);
        }
        mark(previous);
        for(const auto& input:tick.managers) {
            const auto cycle=runtime.evaluate(input.manager_id,previous,input.candidates);
            if(cycle.decision.type!=DevManagerDecisionType::SELECT_AGENT) continue;
            const auto& symbol=cycle.decision.selected_symbol;
            const auto slot=cycle.decision.selected_slot_index;
            if(symbol.empty()||!slot||!cycle.state.agent_reports[*slot]||
               cycle.state.agent_reports[*slot]->preferred_action!=DevAction::LONG) continue;
            const auto account=supervisor.managerAccount(input.manager_id);
            if(!account.positions.empty()) continue; // rotation not yet supported
            const auto bar=market.nextExecutionBar(symbol,previous);
            ++result.execution_bar_checks;
            // Gap guard: reject if an entire minute or more is unavailable.
            if(bar.start_epoch<previous||bar.start_epoch>previous+60)
                throw std::logic_error("invalid execution chronology");
            const auto shares=static_cast<std::int64_t>(std::floor(account.cash*fraction/bar.open));
            if(shares<=0) continue;
            const auto r=runtime.submit(cycle,DevSupervisedAction::BUY,
                orderId(previous,input.manager_id,"buy"),shares,bar.open);
            report(bar.start_epoch,input.manager_id,symbol,"BUY_NEXT_OPEN",r);
        }
        // Mark-to-market only with candles completed by this boundary.
        mark(previous);
        const double equity=supervisor.snapshot().equity;
        out.equity_curve.push_back(equity);
        peak=std::max(peak,equity);
        out.max_drawdown_fraction=std::max(out.max_drawdown_fraction,(peak-equity)/peak);
        ++result.input_ticks;
    }
    // Terminal settlement at last fully completed minute CLOSE. Explicitly
    // separated from decisions; it is NOT an inferred model exit signal.
    mark(previous);
    for(const auto& id:managers) {
        const auto account=supervisor.managerAccount(id);
        for(const auto& [symbol,pos]:account.positions) {
            DevSupervisedOrder order{};
            order.order_id=orderId(previous,id,"terminal:"+symbol);
            order.manager_id=id;order.agent_id=pos.agent_id;order.symbol=symbol;
            order.action=DevSupervisedAction::EXIT;order.timestamp=previous;
            order.market_price=market.completedClose(symbol,previous);
            report(previous,id,symbol,"TERMINAL_EXIT",supervisor.execute(order));
        }
    }
    out.final_equity=supervisor.snapshot().equity;
    out.equity_curve.push_back(out.final_equity);
    out.net_pnl=out.final_equity-out.initial_equity;
    return result;
}
} // namespace devai::strategy
