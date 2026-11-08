#include "devai/strategy/dev_ai_strategy/DevExecutionGatewayD9C.hpp"
#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <utility>
namespace devai::strategy {
namespace {
bool isEntry(DevIntent a){return a==DevIntent::OPEN_LONG||a==DevIntent::OPEN_SHORT;}
bool isExit(DevIntent a){return a==DevIntent::CLOSE_LONG||a==DevIntent::CLOSE_SHORT||a==DevIntent::FORCE_EXIT;}
DevOrderResult result(DevOrderStatus s,const std::string&why){return {s,why,{}};}
}
const char* toString(DevOrderStatus s){switch(s){case DevOrderStatus::SUGGESTION_ONLY:return "SUGGESTION_ONLY";case DevOrderStatus::SIMULATED:return "SIMULATED";case DevOrderStatus::PENDING_APPROVAL:return "PENDING_APPROVAL";case DevOrderStatus::BLOCKED:return "BLOCKED";case DevOrderStatus::SUBMITTED:return "SUBMITTED";case DevOrderStatus::REJECTED:return "REJECTED";}return "UNKNOWN";}
DevExecutionGatewayD9C::DevExecutionGatewayD9C(DevGatewayConfig c,DevBrokerAdapter*b,DevTestCostConfig costs):config_(std::move(c)),broker_(b),costs_(costs){
 if(config_.new_entry_cutoff>config_.force_exit_minute||config_.force_exit_minute!=915)throw std::invalid_argument("D9C mandatory exit must be 15:15 IST");
 if(config_.max_quantity==0)throw std::invalid_argument("max_quantity must be positive");
}
DevOrderResult DevExecutionGatewayD9C::process(const DevTradeSuggestion& s,bool admin_approved){
 if(s.symbol.empty()||s.decision_key<=0)return result(DevOrderStatus::BLOCKED,"invalid symbol/time");
 if(s.intent==DevIntent::HOLD)return result(DevOrderStatus::SUGGESTION_ONLY,"HOLD; no broker order");
 if(s.decision_id.empty())return result(DevOrderStatus::BLOCKED,"unique decision_id required");
 if(auto it=processed_.find(s.decision_id);it!=processed_.end())return it->second;
 const auto finish=[&](DevOrderResult r){if(r.status!=DevOrderStatus::PENDING_APPROVAL)processed_.emplace(s.decision_id,r);return r;};
 const int minute=static_cast<int>((s.decision_key%1440+1440)%1440);
 if(config_.allowed_symbols.count(s.symbol)==0)return finish(result(DevOrderStatus::BLOCKED,"symbol not allowlisted"));
 if(s.quantity==0||s.quantity>config_.max_quantity)return finish(result(DevOrderStatus::BLOCKED,"quantity exceeds limits"));
 if(!s.model_approved||!s.risk_approved)return finish(result(DevOrderStatus::BLOCKED,"model/risk authorization absent"));
 if(isEntry(s.intent)){
  if(minute<555||minute>=config_.new_entry_cutoff)return finish(result(DevOrderStatus::BLOCKED,"outside entry window"));
  if(s.position_exists)return finish(result(DevOrderStatus::BLOCKED,"position already open"));
  if(s.intent==DevIntent::OPEN_LONG&&!config_.enable_long)return finish(result(DevOrderStatus::BLOCKED,"long disabled"));
  if(s.intent==DevIntent::OPEN_SHORT&&(!config_.enable_short||!s.shortable))return finish(result(DevOrderStatus::BLOCKED,"short disabled/ineligible"));
 } else if(isExit(s.intent)) {
  if(!s.position_exists)return finish(result(DevOrderStatus::BLOCKED,"no position to exit"));
  if(minute<555||minute>config_.force_exit_minute)return finish(result(DevOrderStatus::BLOCKED,"outside trading window; invoke emergency broker reconciliation"));
 } else return finish(result(DevOrderStatus::BLOCKED,"unsupported intent"));
 if(config_.mode==DevRunMode::ANALYSE)return finish(result(DevOrderStatus::SUGGESTION_ONLY,"ANALYSE mode: broker calls prohibited"));
 if(config_.mode==DevRunMode::TESTING)return finish(simulate(s));
 if(config_.kill_switch)return finish(result(DevOrderStatus::BLOCKED,"kill switch active; reconcile open positions separately"));
 if(!config_.broker_connected||!config_.live_armed||!config_.market_data_fresh||!config_.broker_state_reconciled||!broker_)return finish(result(DevOrderStatus::BLOCKED,"live execution not armed/healthy/reconciled"));
 if(config_.approval==DevApprovalMode::MANUAL&&!admin_approved)return result(DevOrderStatus::PENDING_APPROVAL,"awaiting explicit admin approval");
 auto submitted=broker_->submit(s);
 if(submitted.status!=DevOrderStatus::SUBMITTED&&submitted.status!=DevOrderStatus::REJECTED)return finish(result(DevOrderStatus::REJECTED,"broker returned invalid submission status"));
 return finish(submitted);
}
}

namespace devai::strategy {
DevOrderResult DevExecutionGatewayD9C::simulate(const DevTradeSuggestion& s){
 if(!std::isfinite(s.execution_price)||s.execution_price<=0)return {DevOrderStatus::BLOCKED,"TESTING requires a positive causal execution_price",{}};
 auto &p=test_positions_[s.symbol];
 const bool entry=(s.intent==DevIntent::OPEN_LONG||s.intent==DevIntent::OPEN_SHORT);
 const int side=s.intent==DevIntent::OPEN_LONG?1:s.intent==DevIntent::OPEN_SHORT?-1:0;
 if(entry&&p.side!=0)return {DevOrderStatus::BLOCKED,"simulated position already exists",{}};
 if(!entry&&p.side==0)return {DevOrderStatus::BLOCKED,"no simulated position to close",{}};
 if(!entry&&s.quantity!=p.qty)return {DevOrderStatus::BLOCKED,"TESTING close quantity must equal simulated position quantity",{}};
 if(!entry&&s.intent==DevIntent::CLOSE_LONG&&p.side!=1)return {DevOrderStatus::BLOCKED,"CLOSE_LONG position mismatch",{}};
 if(!entry&&s.intent==DevIntent::CLOSE_SHORT&&p.side!=-1)return {DevOrderStatus::BLOCKED,"CLOSE_SHORT position mismatch",{}};
 const bool buy=entry?(side==1):(p.side==-1);
 const double slip=costs_.slippage_bps/10000.0;
 const double price=s.execution_price*(buy?1.0+slip:1.0-slip);
 const double turnover=price*static_cast<double>(s.quantity);
 // F3E-F2C.4A: delegate every order-side charge to the existing CETE engine.
 brokerage::BrokerageChargesConfig fee_cfg{};
 fee_cfg.brokerage_rate=costs_.brokerage_rate;
 fee_cfg.brokerage_cap_per_order=costs_.brokerage_cap;
 fee_cfg.stt_sell_rate=costs_.sell_stt_rate;
 fee_cfg.exchange_transaction_rate=costs_.exchange_rate;
 fee_cfg.sebi_rate=costs_.sebi_rate;
 fee_cfg.stamp_duty_buy_rate=costs_.stamp_buy_rate;
 fee_cfg.gst_rate=costs_.gst_rate;
 const brokerage::BrokerageChargesEngine fee_engine(fee_cfg);
 const double fees=fee_engine.calculate(
     buy ? brokerage::ExecutionSide::BUY : brokerage::ExecutionSide::SELL,
     turnover).total;
 if(!std::isfinite(fees)||fees<0)return {DevOrderStatus::BLOCKED,"invalid simulated fees",{}};
 double pnl=0;
 if(entry){p={side,s.quantity,price,fees};}
 else {pnl=(price-p.entry)*static_cast<double>(s.quantity)*p.side-p.entry_fees-fees;p={};test_realized_pnl_+=pnl;}
 test_fees_+=fees;
 test_fills_.push_back({s.symbol,s.decision_id,s.intent,s.decision_key,s.quantity,price,fees,pnl});
 return {DevOrderStatus::SIMULATED,"TESTING simulated fill; no broker call",{}};
}
}
