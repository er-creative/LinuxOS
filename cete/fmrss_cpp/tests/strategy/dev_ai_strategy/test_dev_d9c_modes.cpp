#include "devai/strategy/dev_ai_strategy/DevExecutionGatewayD9C.hpp"
#include <cassert>
#include <iostream>
using namespace devai::strategy;
struct MockBroker final:DevBrokerAdapter {int calls=0;DevOrderResult submit(const DevTradeSuggestion&) override {++calls;return {DevOrderStatus::SUBMITTED,"accepted for processing","MOCK-123"};}};
int main(){
 MockBroker broker;DevGatewayConfig cfg;cfg.allowed_symbols={"RELIANCE"};
 DevTradeSuggestion s{"RELIANCE",739738*1440+555,DevIntent::OPEN_LONG,10,false,false,true,true,"a"};
 DevExecutionGatewayD9C analyse(cfg,&broker);
 assert(analyse.process(s).status==DevOrderStatus::SUGGESTION_ONLY&&broker.calls==0);
 cfg.mode=DevRunMode::GO_LIVE;DevExecutionGatewayD9C disarmed(cfg,&broker);
 assert(disarmed.process(s).status==DevOrderStatus::BLOCKED&&broker.calls==0);
 cfg.broker_connected=cfg.live_armed=cfg.market_data_fresh=cfg.broker_state_reconciled=true;
 DevExecutionGatewayD9C live(cfg,&broker);
 assert(live.process(s).status==DevOrderStatus::PENDING_APPROVAL&&broker.calls==0);
 assert(live.process(s,true).status==DevOrderStatus::SUBMITTED&&broker.calls==1);
 assert(live.process(s,true).status==DevOrderStatus::SUBMITTED&&broker.calls==1);
 s.decision_id="b";s.intent=DevIntent::OPEN_SHORT;s.shortable=true;
 assert(live.process(s,true).status==DevOrderStatus::BLOCKED);
 s.decision_id="c";s.intent=DevIntent::OPEN_LONG;s.decision_key=739738*1440+915;
 assert(live.process(s,true).status==DevOrderStatus::BLOCKED);
 s.decision_id="d";s.intent=DevIntent::FORCE_EXIT;s.position_exists=true;
 assert(live.process(s,true).status==DevOrderStatus::SUBMITTED&&broker.calls==2);
 cfg.approval=DevApprovalMode::AUTO;DevExecutionGatewayD9C autoLive(cfg,&broker);
 s.decision_id="e";s.intent=DevIntent::OPEN_LONG;s.position_exists=false;s.decision_key=739738*1440+600;
 assert(autoLive.process(s).status==DevOrderStatus::SUBMITTED&&broker.calls==3);
 cfg.mode=DevRunMode::TESTING;
 DevExecutionGatewayD9C testing(cfg,&broker);
 s.decision_id="test_open";s.intent=DevIntent::OPEN_LONG;s.decision_key=739738*1440+600;
 s.quantity=10;s.position_exists=false;s.execution_price=100.0;
 assert(testing.process(s).status==DevOrderStatus::SIMULATED&&broker.calls==3);
 assert(testing.process(s).status==DevOrderStatus::SIMULATED&&testing.testFills().size()==1);
 s.decision_id="test_exit";s.intent=DevIntent::FORCE_EXIT;s.decision_key=739738*1440+915;
 s.position_exists=true;s.execution_price=102.0;
 assert(testing.process(s).status==DevOrderStatus::SIMULATED&&broker.calls==3);
 assert(testing.testFills().size()==2&&testing.testFees()>0);
 assert(testing.testRealizedPnl()<20.0&&testing.testRealizedPnl()>0.0);
 std::cout<<"PASS D9C ANALYSE/GO_LIVE gateway: no broker in analyse, live gates, admin approval, short disabled, 15:15 cutoff, dedup\n";
}
