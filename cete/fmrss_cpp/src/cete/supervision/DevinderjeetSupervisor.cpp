#include "cete/supervision/DevinderjeetSupervisor.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cete::supervision {
namespace {
bool finitePositive(double v) { return std::isfinite(v) && v > 0.0; }
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
}
DevinderjeetSupervisor::DevinderjeetSupervisor(DevSupervisorConfig c) : config_(c) {
    require(finitePositive(c.initial_capital), "invalid initial capital");
    require(std::isfinite(c.global_max_exposure_fraction) && c.global_max_exposure_fraction > 0.0 && c.global_max_exposure_fraction <= 1.0, "invalid exposure fraction");
    require(std::isfinite(c.transaction_cost_bps) && c.transaction_cost_bps >= 0.0 && std::isfinite(c.slippage_bps) && c.slippage_bps >= 0.0, "invalid execution costs");
    require(c.max_open_positions > 0, "invalid max positions");
}
void DevinderjeetSupervisor::registerManager(const DevManagerRegistration& r) {
    std::lock_guard<std::mutex> lock(mutex_);
    require(!r.manager_id.empty() && r.agents.size() == 5 && finitePositive(r.allocated_capital), "manager needs ID, exactly five agents and positive allocation");
    require(!managers_.count(r.manager_id), "duplicate manager");
    for (std::size_t i=0;i<r.agents.size();++i) {
        require(!r.agents[i].empty(), "empty agent ID");
        for (std::size_t j=0;j<i;++j) require(r.agents[i]!=r.agents[j], "duplicate agent in manager");
        for (const auto& [id,m]:managers_)
            require(std::find(m.registration.agents.begin(),m.registration.agents.end(),r.agents[i])==m.registration.agents.end(), "agent assigned to another manager");
    }
    double allocated=r.allocated_capital;
    for(const auto& [id,m]:managers_) allocated+=m.registration.allocated_capital;
    require(allocated <= config_.initial_capital + 1e-7, "manager allocations exceed global capital");
    DevManagerAccount account; account.registration=r; account.cash=r.allocated_capital;
    managers_.emplace(r.manager_id,std::move(account));
}
DevSupervisorSnapshot DevinderjeetSupervisor::snapshotUnlocked() const {
    DevSupervisorSnapshot s; s.halted=halted_;
    for(const auto& [id,m]:managers_) {
        s.cash+=m.cash; s.realized_pnl+=m.realized_pnl;
        for(const auto& [symbol,p]:m.positions) {
            const auto it=marks_.find(symbol);
            const double mark=it==marks_.end()?p.entry_price:it->second;
            s.gross_exposure+=static_cast<double>(p.shares)*mark;
            ++s.open_positions;
        }
    }
    s.equity=s.cash+s.gross_exposure;
    return s;
}
DevSupervisorSnapshot DevinderjeetSupervisor::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_); return snapshotUnlocked();
}
DevManagerAccount DevinderjeetSupervisor::managerAccount(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it=managers_.find(id);
    if(it==managers_.end()) throw std::out_of_range("unknown manager");
    return it->second;
}
std::vector<DevOrderReport> DevinderjeetSupervisor::audit() const {
    std::lock_guard<std::mutex> lock(mutex_); return audit_;
}
void DevinderjeetSupervisor::setHalted(bool halted) {
    std::lock_guard<std::mutex> lock(mutex_); halted_=halted;
}
void DevinderjeetSupervisor::updateMark(const std::string& symbol,double price) {
    require(!symbol.empty() && finitePositive(price), "invalid mark");
    std::lock_guard<std::mutex> lock(mutex_); marks_[symbol]=price;
}
DevOrderReport DevinderjeetSupervisor::execute(const DevSupervisedOrder& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    require(!order.order_id.empty(), "order ID required");
    const auto existing=order_results_.find(order.order_id);
    if(existing!=order_results_.end()) return existing->second; // idempotent retry
    auto report=executeUnlocked(order);
    order_results_.emplace(order.order_id,report);
    audit_.push_back(report);
    return report;
}
DevOrderReport DevinderjeetSupervisor::executeUnlocked(const DevSupervisedOrder& o) {
    DevOrderReport r; r.order_id=o.order_id; r.manager_id=o.manager_id;
    r.agent_id=o.agent_id; r.symbol=o.symbol; r.action=o.action; r.timestamp=o.timestamp;
    const auto reject=[&](const std::string& why){r.reason=why;return r;};
    const auto mi=managers_.find(o.manager_id);
    if(mi==managers_.end()) return reject("unknown_manager");
    auto& m=mi->second;
    if(std::find(m.registration.agents.begin(),m.registration.agents.end(),o.agent_id)==m.registration.agents.end()) return reject("agent_not_assigned");
    if(o.action==DevSupervisedAction::HOLD) {r.status=DevOrderStatus::HELD;r.reason="held";return r;}
    if(o.timestamp<=0 || o.symbol.empty() || !finitePositive(o.market_price)) return reject("invalid_order");
    const double fee_rate=config_.transaction_cost_bps/10000.0;
    const double slip_rate=config_.slippage_bps/10000.0;
    if(o.action==DevSupervisedAction::BUY) {
        if(halted_) return reject("global_halt");
        if(o.shares<=0) return reject("invalid_quantity");
        if(m.positions.count(o.symbol)) return reject("manager_position_already_open");
        const auto s=snapshotUnlocked();
        if(s.open_positions>=config_.max_open_positions) return reject("global_position_limit");
        const double px=o.market_price*(1.0+slip_rate);
        const double notional=static_cast<double>(o.shares)*px;
        const double fees=notional*fee_rate;
        if(!std::isfinite(notional) || !std::isfinite(fees)) return reject("nonfinite_notional");
        if(notional+fees > m.cash+1e-8) return reject("manager_cash_limit");
        if(s.gross_exposure+notional > config_.initial_capital*config_.global_max_exposure_fraction+1e-8) return reject("global_exposure_limit");
        m.cash-=notional+fees; m.realized_pnl-=fees;
        m.positions.emplace(o.symbol,DevSupervisedPosition{o.manager_id,o.agent_id,o.symbol,o.shares,px,fees});
        marks_[o.symbol]=o.market_price;
        r.status=DevOrderStatus::FILLED; r.reason="bought";r.shares=o.shares;r.execution_price=px;r.fee=fees;r.realized_pnl=-fees;
        return r;
    }
    const auto it=m.positions.find(o.symbol);
    if(it==m.positions.end()) return reject("no_matching_position");
    const auto p=it->second;
    // No partial exits in this foundation; an EXIT/SELL liquidates the manager's position.
    if(o.shares!=0 && o.shares!=p.shares) return reject("partial_exit_not_supported");
    const double px=o.market_price*(1.0-slip_rate);
    const double proceeds=static_cast<double>(p.shares)*px;
    const double fees=proceeds*fee_rate;
    const double pnl=static_cast<double>(p.shares)*(px-p.entry_price)-fees;
    m.cash+=proceeds-fees; m.realized_pnl+=pnl;
    m.positions.erase(it); marks_[o.symbol]=o.market_price;
    r.status=DevOrderStatus::FILLED;r.reason="exited";r.shares=p.shares;r.execution_price=px;r.fee=fees;r.realized_pnl=pnl;
    return r;
}
}
