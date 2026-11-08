#include "devai/benchmark/DevCETEReplayAdapter.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>
namespace devai::benchmark {
namespace {
void check(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
bool near(double a,double b) {return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-7*(1+std::abs(a)+std::abs(b));}
}
std::vector<CETEReplayRow> DevCETEReplayAdapter::convert(
 const devai::strategy::d9f4c::ReplayResult& replay,
 const std::string& candidate,const std::string& manager) {
 check(!candidate.empty(),"candidate required");
 // The existing D9F4C replay explicitly disables SHORT: never label it as SHORT.
 check(manager=="DevManager_001","D9F4C real replay is LONG-only; SHORT integration is not certified");
 check(!replay.steps.empty(),"empty replay");
 check(replay.steps.back().decision.terminal,"terminal replay step required");
 check(replay.final_portfolio.quantity==0 && replay.final_portfolio.held_symbol.empty(),"replay not flat");
 std::vector<CETEReplayRow> rows;
 rows.reserve(replay.steps.size());
 std::int64_t previous=0;
 for(const auto& s:replay.steps) {
  const auto& d=s.decision;const auto& x=s.execution;
  check(d.decision_time>previous,"out-of-order replay decisions");previous=d.decision_time;
  check(d.manager_action<d.legal_actions.size()&&d.legal_actions[d.manager_action],"illegal manager action");
  check(x.numerical_integrity,"ledger integrity failure");
  check(x.decision_time==d.decision_time,"execution timestamp mismatch");
  check(near(x.equity_after-x.equity_before,x.net_pnl),"net equity reconciliation failure");
  check(near(x.gross_pnl-x.net_pnl,x.brokerage+x.slippage+x.other_costs),"cost reconciliation failure");
  check(near(x.equity_after,s.portfolio.equity),"portfolio equity mismatch");
  check(s.portfolio.quantity>=0,"negative quantity in LONG replay");
  if(d.terminal) check(s.portfolio.quantity==0,"terminal position not flat");
  rows.push_back({candidate,manager,s.portfolio.held_symbol,d.decision_time,d.manager_action,
   x.executed_legs,x.equity_before,x.equity_after,x.gross_pnl,x.brokerage,x.slippage,
   x.other_costs,x.net_pnl,x.drawdown,d.terminal});
 }
 check(near(rows.back().equity_after,replay.final_portfolio.equity),"final equity mismatch");
 return rows;
}
CETEReplaySummary DevCETEReplayAdapter::summarize(const std::vector<CETEReplayRow>& rows) {
 check(!rows.empty(),"empty rows");CETEReplaySummary out{};
 out.decisions=rows.size();out.initial_equity=rows.front().equity_before;
 check(out.initial_equity>0,"invalid initial equity");
 double peak=out.initial_equity;
 for(std::size_t i=0;i<rows.size();++i) {
  const auto& r=rows[i];
  check(i==0||r.timestamp>rows[i-1].timestamp,"out-of-order rows");
  check(i==0||near(r.equity_before,rows[i-1].equity_after),"equity continuity failure");
  check(near(r.equity_after-r.equity_before,r.net),"net mismatch");
  check(near(r.gross-r.net,r.brokerage+r.slippage+r.other_costs),"cost mismatch");
  out.executed_legs+=r.legs;out.gross+=r.gross;
  out.costs+=r.brokerage+r.slippage+r.other_costs;out.net+=r.net;
  peak=std::max(peak,r.equity_after);
  out.max_drawdown_pct=std::max(out.max_drawdown_pct,100.0*(peak-r.equity_after)/peak);
 }
 check(rows.back().terminal,"terminal row required");
 out.final_equity=rows.back().equity_after;
 check(near(out.final_equity-out.initial_equity,out.net),"summary reconciliation failure");
 return out;
}
void DevCETEReplayAdapter::writeCsv(const std::filesystem::path& p,const std::vector<CETEReplayRow>& rows) {
 check(!rows.empty(),"no rows to write");std::ofstream out(p);
 check(bool(out),"cannot open output CSV");out<<std::setprecision(17);
 out<<"candidate,manager,symbol,timestamp,action,legs,equity_before,equity_after,gross,brokerage,slippage,other_costs,net,drawdown,terminal\n";
 for(const auto& r:rows) out<<r.candidate<<','<<r.manager<<','<<r.symbol<<','<<r.timestamp<<','<<r.action<<','<<r.legs<<','<<r.equity_before<<','<<r.equity_after<<','<<r.gross<<','<<r.brokerage<<','<<r.slippage<<','<<r.other_costs<<','<<r.net<<','<<r.drawdown<<','<<(r.terminal?1:0)<<'\n';
 check(bool(out),"CSV write failed");
}
}
