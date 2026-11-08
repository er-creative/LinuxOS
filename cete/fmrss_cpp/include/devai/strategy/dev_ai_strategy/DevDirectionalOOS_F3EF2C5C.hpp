#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace devai::strategy::f3ef2c5c {
// Offline, frozen-policy replay. This intentionally does NOT call a trainer or broker.
// Timestamps are UTC epoch seconds. Each decision must use only completed data.
struct Decision {
 std::int64_t time{};
 std::int64_t latest_observation_time{};
 std::string symbol; // empty=FLAT, otherwise target symbol
 int direction{}; // -1 SHORT, 0 FLAT, +1 LONG
 bool shortable{};
 bool model_short_certified{};
};
using Price = std::function<double(const std::string&,std::int64_t)>;
struct Config {
 double initial_capital{7000.};
 double allocation{.75};
 double proportional_cost_per_leg{0.}; // conservative TEST assumption, not Zerodha exact fees
 int entry_cutoff_minute_ist{14*60+45};
 int flat_minute_ist{15*60+15};
 bool allow_short{false};
 bool require_contiguous_minutes{true};
};
struct Trade {
 std::int64_t time{};
 std::string symbol;
 int direction{};
 std::int64_t signed_quantity{};
 double fill_price{};
 double costs{};
 double equity_after{};
 std::string event;
};
struct Result {
 double initial_equity{}, final_equity{}, gross_pnl{}, total_costs{}, net_pnl{}, max_drawdown{};
 std::size_t long_entries{}, short_entries{}, exits{}, forced_exits{}, decisions{};
 std::vector<Trade> trades;
};
inline void require(bool x,const char* why) {if(!x)throw std::invalid_argument(why);}
inline bool valid(double x){return std::isfinite(x);}
inline int minuteIST(std::int64_t t) {
 std::int64_t m=((t+19800)%86400+86400)%86400;
 return int(m/60);
}
inline Result replay(const Config& c,const std::vector<Decision>& decisions,const Price& price) {
 require(valid(c.initial_capital)&&c.initial_capital>0&&valid(c.allocation)&&c.allocation>0&&c.allocation<=1,
         "invalid capital/allocation");
 require(valid(c.proportional_cost_per_leg)&&c.proportional_cost_per_leg>=0&&c.proportional_cost_per_leg<1,
         "invalid cost rate");
 require(c.entry_cutoff_minute_ist<c.flat_minute_ist&&c.flat_minute_ist<=24*60,"invalid session cutoffs");
 require(bool(price),"missing price provider");
 require(!decisions.empty(),"empty OOS decisions");
 Result out{};out.initial_equity=c.initial_capital;
 double cash=c.initial_capital,peak=c.initial_capital,costs=0,prior_equity=c.initial_capital;
 std::string held;std::int64_t qty=0;double last_mark=0;
 std::int64_t previous=0;
 auto get=[&](const std::string& s,std::int64_t t){double p=price(s,t);require(valid(p)&&p>0,"invalid completed price");return p;};
 auto mark=[&](std::int64_t t){return cash+(qty?double(qty)*get(held,t):0.);};
 for(const auto& d:decisions) {
  require(d.time>0&&d.time>previous,"nonchronological OOS decisions");
  require(d.latest_observation_time>0&&d.latest_observation_time<=d.time,"future-data leakage");
  require(d.direction>=-1&&d.direction<=1,"invalid direction");
  require((d.direction==0)==d.symbol.empty(),"invalid target identity");
  if(previous&&c.require_contiguous_minutes&&d.time!=previous+60)
   require(qty==0,"open position crosses missing minute/session");
  const int now=minuteIST(d.time);
  require(now>=9*60+15&&now<=15*60+30,"decision outside cash session");
  if(qty)last_mark=get(held,d.time);
  const double marked=cash+double(qty)*last_mark;
  peak=std::max(peak,marked);
  out.max_drawdown=std::max(out.max_drawdown,(peak-marked)/peak);
  int target_dir=d.direction;
  std::string target=d.symbol;
  if(now>=c.flat_minute_ist){target_dir=0;target.clear();}
  const int old_dir=(qty>0)-(qty<0);
  const bool change=(target!=held||target_dir!=old_dir);
  if(change&&old_dir) {
   const double notional=std::abs(double(qty))*last_mark;
   const double fee=notional*c.proportional_cost_per_leg;
   cash+=double(qty)*last_mark-fee;costs+=fee;
   out.trades.push_back({d.time,held,old_dir,qty,last_mark,fee,cash,"EXIT"});
   qty=0;held.clear();++out.exits;
   if(now>=c.flat_minute_ist)++out.forced_exits;
  }
  if(change&&target_dir) {
   require(now<c.entry_cutoff_minute_ist,"entry at/after cutoff");
   if(target_dir<0)require(c.allow_short&&d.shortable&&d.model_short_certified,"SHORT not certified/eligible");
   // No same-minute atomic switch/reversal; caller must explicitly flatten first.
   require(old_dir==0,"atomic reversal/switch prohibited");
   const double p=get(target,d.time);
   const double equity=cash;
   const double budget=std::min(cash,equity*c.allocation);
   const auto units=std::int64_t(std::floor(budget/(p*(1+c.proportional_cost_per_leg))));
   if(units>0) {
    qty=units*target_dir;held=target;
    const double notional=double(units)*p;
    const double fee=notional*c.proportional_cost_per_leg;
    cash-=double(qty)*p+fee;costs+=fee;
    out.trades.push_back({d.time,held,target_dir,qty,p,fee,cash+double(qty)*p,"ENTRY"});
    if(target_dir>0)++out.long_entries;else ++out.short_entries;
   }
  }
  const double equity=mark(d.time);
  require(valid(equity)&&equity>0,"invalid portfolio equity");
  peak=std::max(peak,equity);
  out.max_drawdown=std::max(out.max_drawdown,(peak-equity)/peak);
  prior_equity=equity;previous=d.time;++out.decisions;
 }
 require(qty==0,"OOS ended with an open position: add a final flatten decision");
 out.final_equity=prior_equity;out.total_costs=costs;out.net_pnl=out.final_equity-out.initial_equity;
 out.gross_pnl=out.net_pnl+costs;
 require(std::abs(out.gross_pnl-out.total_costs-out.net_pnl)<1e-7,"PnL reconciliation failure");
 return out;
}
// Require the OOS interval to start strictly after the last training timestamp.
inline void validateSplit(std::int64_t last_train_time,std::int64_t first_oos_time) {
 require(last_train_time>0&&first_oos_time>last_train_time,"train/OOS chronology overlap");
}
} // namespace devai::strategy::f3ef2c5c
