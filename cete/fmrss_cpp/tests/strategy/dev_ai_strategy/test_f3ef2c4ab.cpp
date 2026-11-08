#include "devai/strategy/brokerage/BrokerageChargesEngine.hpp"
#include "devai/strategy/dev_ai_strategy/DevExecutionGatewayD9C.hpp"
#include "devai/strategy/dev_ai_strategy/DevIntradayDirectionPolicy_F3EF2C4B.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace devai::strategy;
static void verify(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static bool near(double a, double b, double tol=1e-8) {return std::abs(a-b)<tol;}
static DevTradeSuggestion order(const std::string& id, DevIntent intent,
                                bool exists, int minute, double price=500.0) {
    DevTradeSuggestion s{};
    s.symbol="SBIN";
    s.decision_key=200000*1440+minute;
    s.intent=intent;
    s.quantity=10;
    s.shortable=true;
    s.position_exists=exists;
    s.model_approved=true;
    s.risk_approved=true;
    s.decision_id=id;
    s.execution_price=price;
    return s;
}
int main() {
    brokerage::BrokerageChargesEngine engine;
    const auto buy=engine.calculate(brokerage::ExecutionSide::BUY,5250.0);
    const auto sell=engine.calculate(brokerage::ExecutionSide::SELL,5250.0);
    verify(near(buy.brokerage,1.575) && near(sell.brokerage,1.575),"brokerage mismatch");
    verify(near(buy.exchange_transaction_charge,0.161175),"NSE rate mismatch");
    verify(near(sell.stt,1.3125) && near(buy.stt,0),"STT side mismatch");
    verify(near(buy.stamp_duty,0.1575) && near(sell.stamp_duty,0),"stamp side mismatch");
    verify(near(buy.sebi_charge,0.00525),"SEBI mismatch");
    verify(near(buy.gst,0.18*(1.575+0.161175+0.00525)),"GST mismatch");
    verify(near(buy.total+sell.total,5.579763),"round-trip raw fee mismatch");

    DevGatewayConfig config{};
    config.mode=DevRunMode::TESTING;
    config.enable_short=true;
    config.enable_long=true;
    config.allowed_symbols.insert("SBIN");
    config.new_entry_cutoff=14*60+45;
    config.force_exit_minute=15*60+15;
    DevTestCostConfig costs{};
    costs.slippage_bps=0;
    DevExecutionGatewayD9C gateway(config,nullptr,costs);
    auto open=order("short-entry",DevIntent::OPEN_SHORT,false,10*60,500);
    verify(gateway.process(open).status==DevOrderStatus::SIMULATED,"SHORT entry failed");
    auto close=order("short-cover",DevIntent::CLOSE_SHORT,true,10*60+10,490);
    verify(gateway.process(close).status==DevOrderStatus::SIMULATED,"SHORT cover failed");
    verify(gateway.testFills().size()==2,"missing fills");
    const auto& entry=gateway.testFills()[0];
    const auto& exit=gateway.testFills()[1];
    verify(near(entry.fees,engine.calculate(brokerage::ExecutionSide::SELL,5000).total),"SHORT entry SELL fee mismatch");
    verify(near(exit.fees,engine.calculate(brokerage::ExecutionSide::BUY,4900).total),"SHORT cover BUY fee mismatch");
    verify(near(gateway.testRealizedPnl(),100-entry.fees-exit.fees),"SHORT net PnL mismatch");
    verify(near(gateway.testFees(),entry.fees+exit.fees),"fee double-count mismatch");
    verify(gateway.process(open).status==DevOrderStatus::SIMULATED && gateway.testFills().size()==2,"dedup failed");

    DevExecutionGatewayD9C long_gateway(config,nullptr,costs);
    verify(long_gateway.process(order("long-entry",DevIntent::OPEN_LONG,false,10*60,500)).status==DevOrderStatus::SIMULATED,"LONG entry failed");
    verify(long_gateway.process(order("long-exit",DevIntent::CLOSE_LONG,true,10*60+10,490)).status==DevOrderStatus::SIMULATED,"LONG exit failed");
    verify(near(long_gateway.testRealizedPnl(),-100-long_gateway.testFees()),"LONG net PnL mismatch");

    using namespace f3ef2c4b;
    Config rules{};
    Position flat{};
    Request r{Decision::OPEN_SHORT,10*60,500,10,true,true,true};
    verify(check(rules,flat,r)==Verdict::ALLOW,"SHORT rule denied");
    r.shortable=false;
    verify(check(rules,flat,r)==Verdict::BLOCK,"nonshortable allowed");
    r.shortable=true;
    r.minute_of_day_ist=14*60+45;
    verify(check(rules,flat,r)==Verdict::BLOCK,"late entry allowed");
    r.minute_of_day_ist=10*60;
    r.quantity=11;
    verify(check(rules,flat,r)==Verdict::BLOCK,"overallocation allowed");
    Position held{true,Direction::SHORT,10,500};
    r.quantity=10;
    r.decision=Decision::SWITCH_LONG;
    verify(check(rules,held,r)==Verdict::BLOCK,"atomic switch should require confirmed exit");
    r.decision=Decision::EXIT;
    r.minute_of_day_ist=15*60+15;
    verify(check(rules,held,r)==Verdict::ALLOW,"mandatory exit denied");
    r.decision=Decision::HOLD;
    verify(check(rules,held,r)==Verdict::BLOCK,"hold at forced flat time allowed");
    verify(near(gross_pnl(Direction::SHORT,500,490,10),100),"SHORT gross mismatch");
    std::cout << "F3E-F2C.4A/B PASS (shared CETE fees, LONG/SHORT gateway, policy guards)\n";
}
