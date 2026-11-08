#include "devai/strategy/dev_ai_strategy/DevRealHistoricalMultiStockOOS.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace devai::strategy {
namespace {
void require(bool ok, const char* msg) { if (!ok) throw std::invalid_argument(msg); }
void priceCheck(double p) { require(std::isfinite(p) && p > 0, "3.19B: invalid price"); }
int minuteIST(std::int64_t t) {
    constexpr std::int64_t day = 86400;
    std::int64_t local = (t + 19800) % day;
    if (local < 0) local += day;
    return static_cast<int>(local / 60);
}
struct Target { std::string symbol; int direction{0}; };
Target decode(const std::string& s) {
    if (s.empty()) return {};
    if (s.rfind("SHORT:", 0) == 0) {
        require(s.size() > 6, "3.19B: empty short symbol");
        return {s.substr(6), -1};
    }
    require(s.find(':') == std::string::npos, "3.19B: malformed instruction");
    return {s, 1};
}
struct Costs { double brokerage{}, slippage{}, other{}; };
Costs costsFor(const DevOOSLedgerConfig& c, DevTradeSide side,
               std::int64_t qty, double price) {
    if (qty <= 0 || c.cost_mode == DevOOSCostMode::ZERO_COST) return {};
    const double notional = static_cast<double>(qty) * price;
    if (c.cost_mode == DevOOSCostMode::LEGACY_PERCENT)
        return {notional*c.brokerage_rate, notional*c.slippage_rate,
                notional*c.other_cost_rate};
    auto rates = c.zerodha_rates;
    // In Zerodha mode the ledger's slippage_rate is the scenario assumption.
    rates.slippage_rate = c.slippage_rate;
    const auto z = DevZerodhaIntradayCosts(rates).calculate(side, qty, price, c.exchange);
    return {z.brokerage, z.slippage, z.statutory_total()};
}
double total(const Costs& c) { return c.brokerage+c.slippage+c.other; }
}
DevRealHistoricalMultiStockOOS::DevRealHistoricalMultiStockOOS(DevOOSLedgerConfig c):config_(c) {
    require(std::isfinite(c.initial_capital)&&c.initial_capital>0,"3.19B: capital");
    require(std::isfinite(c.max_allocation_fraction)&&c.max_allocation_fraction>0&&c.max_allocation_fraction<=1,"3.19B: allocation");
    for(double r:{c.brokerage_rate,c.slippage_rate,c.other_cost_rate})
        require(std::isfinite(r)&&r>=0&&r<1,"3.19B: costs");
    if (c.cost_mode == DevOOSCostMode::ZERODHA_EQUITY_INTRADAY)
        (void)DevZerodhaIntradayCosts(c.zerodha_rates);
    require(c.short_square_off_hour_ist>=9&&c.short_square_off_hour_ist<=15&&
        c.short_square_off_minute_ist>=0&&c.short_square_off_minute_ist<60,"3.19B: cutoff");
    state_.cash=state_.equity=state_.peak_equity=c.initial_capital;
}
const DevOOSLedgerState& DevRealHistoricalMultiStockOOS::state() const noexcept {return state_;}
DevOOSExecutionResult DevRealHistoricalMultiStockOOS::execute(
    std::int64_t t,const std::string& desired,const DevOOSPriceProvider& prices) {
    require(static_cast<bool>(prices),"3.19B: missing price provider");
    require(t>state_.last_time,"3.19B: nonchronological decision");
    Target target=decode(desired);
    const int cutoff=config_.short_square_off_hour_ist*60+config_.short_square_off_minute_ist;
    const int now=minuteIST(t);
    if (!config_.allow_intraday_short && target.direction<0)
        throw std::invalid_argument("3.19B: short disabled");
    // No new positions or rotations after 14:45; mandatory flat at 15:15.
    // Before 09:15, permit only liquidation, never an entry.
    if (now >= cutoff) target = {};
    else if ((now < 9*60+15 || now >= 14*60+45) &&
             target.symbol != state_.held_symbol) target = {};
    const int old_dir=(state_.quantity>0)-(state_.quantity<0);
    const bool change=(target.symbol!=state_.held_symbol || target.direction!=old_dir);
    const double old_price=old_dir?prices(state_.held_symbol,t):0.0;
    if(old_dir) priceCheck(old_price);
    const double new_price=(change&&target.direction)?prices(target.symbol,t):0.0;
    if(change&&target.direction) priceCheck(new_price);
    DevOOSLedgerState next=state_;
    DevOOSExecutionResult r{};
    r.decision_time=t; r.equity_before=state_.equity;
    const double market_equity=state_.cash + static_cast<double>(state_.quantity)*old_price;
    r.gross_pnl=market_equity-state_.equity;
    auto charge=[&](DevTradeSide side, std::int64_t qty, double price) {
        const Costs x=costsFor(config_,side,qty,price);
        r.brokerage+=x.brokerage;
        r.slippage+=x.slippage;
        r.other_costs+=x.other;
    };
    if(change&&old_dir) {
        const auto qty=std::abs(state_.quantity);
        const double notional=static_cast<double>(qty)*old_price;
        next.cash+=static_cast<double>(state_.quantity)*old_price;
        charge(old_dir>0?DevTradeSide::SELL:DevTradeSide::BUY,qty,old_price);
        next.realized_net_pnl+=static_cast<double>(state_.quantity)*(old_price-state_.entry_price);
        next.quantity=0;next.held_symbol.clear();next.entry_price=0;next.entry_time=0;
        next.restricted_short_proceeds=0;
        ++r.executed_legs;
        (void)notional;
    }
    if(change&&target.direction) {
        const double existing_costs=r.brokerage+r.slippage+r.other_costs;
        const double free_capital=next.cash-next.restricted_short_proceeds-existing_costs;
        const double budget=std::max(0.0,std::min(free_capital,
                           market_equity*config_.max_allocation_fraction));
        const auto side=target.direction>0?DevTradeSide::BUY:DevTradeSide::SELL;
        const auto affordable=[&](std::int64_t qty) {
            const double notional=static_cast<double>(qty)*new_price;
            const double charges=total(costsFor(config_,side,qty,new_price));
            return std::isfinite(notional+charges) && notional+charges<=budget+1e-9;
        };
        const double raw=std::floor(budget/new_price);
        require(std::isfinite(raw) && raw>=0 && raw<static_cast<double>(std::numeric_limits<std::int64_t>::max()),
                "3.19B: invalid allocation quantity");
        std::int64_t lo=0,hi=static_cast<std::int64_t>(raw);
        while(lo<hi) {
            const auto mid=lo+(hi-lo)/2+1;
            if(affordable(mid))lo=mid;
            else hi=mid-1;
        }
        const auto qty=lo;
        if(qty>0) {
            const double notional=static_cast<double>(qty)*new_price;
            next.quantity=qty*target.direction;
            next.cash-=static_cast<double>(next.quantity)*new_price;
            charge(side,qty,new_price);
            next.held_symbol=target.symbol;next.entry_price=new_price;next.entry_time=t;
            next.restricted_short_proceeds=target.direction<0?notional:0.0;
            ++r.executed_legs;
        }
    }
    const double costs=r.brokerage+r.slippage+r.other_costs;
    next.cash-=costs;next.realized_net_pnl-=costs;
    require(next.cash-next.restricted_short_proceeds>=-1e-7,
            "3.19B: insufficient unrestricted cash");
    r.net_pnl=r.gross_pnl-costs;
    r.equity_after=next.cash+static_cast<double>(next.quantity)*
                  (next.quantity?(change&&target.direction?new_price:old_price):0.0);
    require(std::isfinite(r.equity_after)&&r.equity_after>0,"3.19B: invalid equity");
    next.equity=r.equity_after;next.peak_equity=std::max(next.peak_equity,next.equity);
    next.last_time=t;
    r.drawdown=(next.peak_equity-next.equity)/next.peak_equity;
    r.numerical_integrity=true;
    state_=std::move(next);
    return r;
}
DevManagerPortfolioState DevRealHistoricalMultiStockOOS::portfolioAt(
    const DevOOSLedgerState& ledger,double marked_price,std::int64_t t,double capital) {
    require(std::isfinite(capital)&&capital>0&&t>ledger.last_time,"3.19D: portfolio time/capital");
    DevManagerPortfolioState result{};
    result.current_symbol=ledger.held_symbol;
    auto& p=result.portfolio;
    const bool holding=ledger.quantity!=0;
    if(holding) {
        priceCheck(marked_price);
        require(ledger.entry_price>0&&ledger.entry_time>0&&t>=ledger.entry_time,
                "3.19D: invalid entry");
    }
    const double equity=ledger.cash+(holding?ledger.quantity*marked_price:0.0);
    const double peak=std::max(ledger.peak_equity,equity);
    p.position=ledger.quantity>0?DEV_POSITION_LONG:
               ledger.quantity<0?DEV_POSITION_SHORT:DEV_POSITION_FLAT;
    p.unrealized_return=holding?ledger.quantity*(marked_price-ledger.entry_price)/capital:0;
    p.realized_return=ledger.realized_net_pnl/capital;
    p.equity_return=(equity-capital)/capital;
    p.drawdown=(peak-equity)/peak;
    p.holding_period=holding?static_cast<double>(t-ledger.entry_time)/60.0:0;
    result.numerical_integrity=std::isfinite(p.unrealized_return)&&
        std::isfinite(p.realized_return)&&std::isfinite(p.equity_return)&&
        std::isfinite(p.drawdown)&&std::isfinite(p.holding_period);
    require(result.numerical_integrity,"3.19D: invalid portfolio");
    return result;
}
DevOOSDecisionPoint DevRealHistoricalMultiStockOOS::toDecisionPoint(
    const DevHistoricalDecisionRecord& record,const DevManagerPortfolioState& portfolio) {
    DevHistoricalTrainingDatasetValidator::validateRecord(record);
    DevOOSDecisionPoint point{};
    point.decision_time=record.decision_time;
    point.benchmark_symbol=record.benchmark_symbol;
    point.portfolio=portfolio;
    point.numerical_integrity=record.numerical_integrity;
    for(const auto& candidate:record.candidates) {
        DevOOSCandidate item{};
        item.assignment.assigned=true;
        item.assignment.slot_index=candidate.slot_index;
        item.assignment.candidate_rank=candidate.candidate_rank;
        item.assignment.agent_name=candidate.agent_name;
        item.assignment.symbol=candidate.symbol;
        item.assignment.decision_time=record.decision_time;
        item.assignment.candidate_score=candidate.candidate_score;
        item.observation=candidate.observation;
        point.candidates.push_back(std::move(item));
    }
    DevMultiStockOOSValidator::validatePoint(point);
    return point;
}
}
