#include "devai/strategy/dev_ai_strategy/DevPortfolioEnvironment.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace devai::strategy {
namespace {
void positiveFinite(double x, const char* name) {
    if (!std::isfinite(x) || x <= 0.0) throw std::invalid_argument(name);
}
}
DevPortfolioEnvironment::DevPortfolioEnvironment(DevPortfolioConfig c):config_(std::move(c)) {
    positiveFinite(config_.initial_cash,"Initial cash must be positive and finite");
    if(!std::isfinite(config_.max_exposure_fraction) || config_.max_exposure_fraction<=0 || config_.max_exposure_fraction>1 ||
       !std::isfinite(config_.transaction_cost_bps) || config_.transaction_cost_bps<0 ||
       !std::isfinite(config_.slippage_bps) || config_.slippage_bps<0 || config_.slippage_bps>=10000 ||
       config_.max_gap_seconds<=0) throw std::invalid_argument("Invalid portfolio configuration");
    reset();
}
const DevPortfolioRuntimeState& DevPortfolioEnvironment::reset() {
    state_={}; state_.cash=config_.initial_cash;state_.equity=config_.initial_cash;
    peak_equity_=config_.initial_cash;started_=false;return state_;
}
double DevPortfolioEnvironment::executablePrice(double raw,int direction) const {
    return raw*(1.0+direction*config_.slippage_bps/10000.0);
}
double DevPortfolioEnvironment::fee(double quantity,double price) const {
    return quantity*price*config_.transaction_cost_bps/10000.0;
}
const DevPortfolioQuote& DevPortfolioEnvironment::quoteFor(const DevPortfolioFrame& f,const std::string& symbol) const {
    auto it=std::find_if(f.quotes.begin(),f.quotes.end(),[&](const auto& q){return q.symbol==symbol;});
    if(it==f.quotes.end()) throw std::invalid_argument("Missing quote for "+symbol);
    return *it;
}
void DevPortfolioEnvironment::closePosition(const DevPortfolioFrame& f,bool at_close) {
    if(!state_.position) return;
    const auto p=*state_.position;
    const auto& q=quoteFor(f,p.symbol);
    const double raw=at_close?q.close:q.open;
    const double px=executablePrice(raw,-p.side);
    const double amount=static_cast<double>(p.shares)*px;
    const double costs=fee(static_cast<double>(p.shares),px);
    if(p.side==1) state_.cash+=amount-costs;
    else state_.cash-=amount+costs;
    // Entry cash movements already included. Short proceeds are held in cash for accounting,
    // but never counted as extra buying power for a second position.
    state_.realized_pnl+=p.side*static_cast<double>(p.shares)*(px-p.entry_price)-costs;
    state_.position.reset();
}
void DevPortfolioEnvironment::markToMarket(const DevPortfolioFrame& f) {
    state_.equity=state_.cash;
    if(state_.position) {
        const auto& p=*state_.position;
        const double px=quoteFor(f,p.symbol).close;
        state_.equity+=p.side*static_cast<double>(p.shares)*px;
    }
    if(!std::isfinite(state_.equity)) throw std::runtime_error("Non-finite equity");
    peak_equity_=std::max(peak_equity_,state_.equity);
    state_.drawdown_fraction=(peak_equity_-state_.equity)/peak_equity_;
}
DevPortfolioStep DevPortfolioEnvironment::step(const DevPortfolioFrame& f,const DevPortfolioDecision& d) {
    if(state_.terminal) throw std::logic_error("Episode ended; call reset");
    if(f.time<=0 || (started_ && (f.time<=state_.time || f.time-state_.time>config_.max_gap_seconds)))
        throw std::invalid_argument("Non-monotonic timestamp or unhandled market-data gap");
    std::unordered_set<std::string> seen;
    for(const auto& q:f.quotes) {
        if(q.symbol.empty() || !seen.insert(q.symbol).second) throw std::invalid_argument("Duplicate/empty quote symbol");
        positiveFinite(q.open,"Invalid open");positiveFinite(q.close,"Invalid close");
    }
    if(f.quotes.empty()) throw std::invalid_argument("Empty market frame");
    // Validate required data before mutating portfolio state.
    if(state_.position) (void)quoteFor(f,state_.position->symbol);
    if(d.action==DevPortfolioAction::LONG || d.action==DevPortfolioAction::SHORT) {
        if(d.agent_name.empty() || d.symbol.empty()) throw std::invalid_argument("Missing agent/symbol");
        (void)quoteFor(f,d.symbol);
    }
    DevPortfolioStep result;result.before=state_;result.requested=d;
    const double old_equity=state_.equity;
    if(d.action==DevPortfolioAction::EXIT) {
        if(state_.position && (d.symbol.empty() || d.symbol==state_.position->symbol)) {
            closePosition(f,false);result.executed=true;result.reason="exited_at_open";
        } else result.reason="no_matching_position";
    } else if(d.action==DevPortfolioAction::LONG || d.action==DevPortfolioAction::SHORT) {
        const int side=d.action==DevPortfolioAction::LONG?1:-1;
        if(state_.position) result.reason="one_position_limit";
        else if(side==-1 && !config_.allow_short) result.reason="short_not_allowed";
        else {
            const double px=executablePrice(quoteFor(f,d.symbol).open,side);
            // Use current equity, not cash inflated by short-sale proceeds.
            const double max_notional=std::min(config_.initial_cash*config_.max_exposure_fraction,
                                               state_.equity*config_.max_exposure_fraction);
            const double per_share=px*(1.0+config_.transaction_cost_bps/10000.0);
            const auto qty=static_cast<std::int64_t>(std::floor(std::min(max_notional,state_.cash)/per_share));
            if(qty<1) result.reason="insufficient_capital_for_whole_share";
            else {
                const double amount=static_cast<double>(qty)*px;
                const double costs=fee(static_cast<double>(qty),px);
                state_.cash+=side==1?-(amount+costs):(amount-costs);
                state_.realized_pnl-=costs;
                state_.position=DevPortfolioPosition{d.symbol,d.agent_name,side,f.time,qty,px};
                result.executed=true;result.reason="opened_at_open";
            }
        }
    } else result.reason="held";
    state_.time=f.time;
    markToMarket(f);
    if(f.session_end) {
        // Terminal close is executed at the close, AFTER the candle's mark.
        closePosition(f,true);
        markToMarket(f);
        state_.terminal=true;
    }
    result.reward=(state_.equity-old_equity)/config_.initial_cash;
    result.after=state_;
    started_=true;
    return result;
}
}
