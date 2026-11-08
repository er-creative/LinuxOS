#include "devai/strategy/dev_ai_strategy/DevAISupervisedRuntime.hpp"
#include "Dev5C3HTestFixture.hpp"
#include <array>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace devai::strategy;
namespace fs=std::filesystem;
namespace fx=cete_5c3h_test;
static void check(bool c,const std::string& why){if(!c)throw std::runtime_error("5C-3H check failed: "+why);}
static void denied(const std::function<void()>& f,const std::string& why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,why);}
static std::array<std::string,DEV_AGENT_SLOT_COUNT> agents(int start){
    std::array<std::string,DEV_AGENT_SLOT_COUNT> a{};
    for(std::size_t i=0;i<a.size();++i){auto n=std::to_string(start+static_cast<int>(i));a[i]="DevAgent_"+std::string(3-n.size(),'0')+n;}
    return a;
}
static DevSharedCandidate candidate(const std::string& symbol,std::int64_t t){
    DevSharedCandidate c{};c.slot=0;c.symbol=symbol;c.candidate_score=0.9;c.candidate_rank=1;
    c.observation.symbol=symbol;c.observation.decision_time=t;
    c.observation.ready=true;c.observation.numerical_integrity=true;return c;
}
static void approveExisting(fx::Fixture& f,const std::string& id){
    const auto m=f.registry.load_manager(id);
    const auto mp=f.root/"models"/id/"v1.bin";
    const auto op=f.root/"oos"/id/"oos1.report";
    const auto ap=f.root/"approvals"/(id+".approval");
    fx::write(mp,"fixture frozen model "+id);
    fx::write(op,"fixture OOS evidence "+id);
    const fx::Evidence e{fx::DevModelApprovalVerifier::sha256File(mp),
                         fx::DevModelApprovalVerifier::sha256File(op),
                         fx::DevModelApprovalVerifier::sha256Universe(m.symbols)};
    f.expected[id]=e;
    fx::write(ap,"manager_id="+id+"\nmodel_id=v1\nmodel_sha256="+e.model+
              "\noos_sha256="+e.oos+"\nuniverse_sha256="+e.universe+
              "\napproval_id=oos1\ndecision=APPROVED\n");
    check(f.execution.eligible(id,m.symbols.front()),"fixture authorization for "+id);
}
int main(){
    DevinderjeetSupervisor central({100000.0,0.75,5.0,2.0,2});
    fx::Fixture f(fs::temp_directory_path()/"cete_5c3h_supervised_runtime_integration",central);
    DevAISupervisor admin(f.registry,central,
        [](const std::string&,const std::string&){return true;});
    DevQNetwork agent_model({42U,true});
    DevManagerQNetwork manager_model({42U,true});
    DevSharedModelMultiManagerRuntime raw(agent_model,manager_model,central,f.execution);
    DevAISupervisedRuntime gateway(admin,f.registry,central,raw);
    const auto a=agents(1),b=agents(6);
    const auto m1=admin.createManager({"RELIANCE","SBIN"},a,40000.0);
    const auto m2=admin.createManager({"HDFCBANK","ICICIBANK"},b,40000.0);
    check(m1.manager_id!=m2.manager_id,"distinct manager identities");
    approveExisting(f,m1.manager_id);
    approveExisting(f,m2.manager_id);
    gateway.attachManager(m1.manager_id,a);
    gateway.attachManager(m2.manager_id,b);
    denied([&]{gateway.attachManager(m1.manager_id,b);},"wrong global roster");
    const auto c1=gateway.evaluate(m1.manager_id,1000,{candidate("RELIANCE",1000)});
    const auto c2=gateway.evaluate(m2.manager_id,1000,{candidate("HDFCBANK",1000)});
    check(c1.global_agent_reports.size()==1 && c2.global_agent_reports.size()==1,"shared model generated both reports");
    check(c1.global_agent_reports[0].agent_name==a[0],"first global agent identity");
    check(c2.global_agent_reports[0].agent_name==b[0],"second global agent identity");
    check(c1.action==DevManagerAction::NO_TRADE && c2.action==DevManagerAction::NO_TRADE,"seeded untrained networks no-trade");
    denied([&]{(void)gateway.evaluate(m1.manager_id,1000,{candidate("HDFCBANK",1000)});},"cross-manager symbol");
    denied([&]{(void)gateway.evaluate(m1.manager_id,1000,{candidate("RELIANCE",999)});},"stale timestamp");
    denied([&]{(void)gateway.submit(c1,DevSupervisedAction::BUY,"bad-buy",10,2500.0);},"no-trade cannot buy");
    // 5C-3H-2: HOLD must remain local and must not mutate CETE accounting.
    const auto before=central.snapshot();
    const auto before_a1=central.managerAccount(m1.manager_id);
    const auto before_a2=central.managerAccount(m2.manager_id);
    const auto audit_before=central.audit().size();
    const auto h1=gateway.submit(c1,DevSupervisedAction::HOLD,"hold-1",0,0.0);
    check(h1.status==DevOrderStatus::HELD,"HOLD must report HELD");
    check(h1.fee==0.0 && h1.realized_pnl==0.0 && h1.shares==0,
          "HOLD report must have zero cost, PnL and shares");
    const auto h2=gateway.submit(c2,DevSupervisedAction::HOLD,"hold-2",0,0.0);
    check(h2.status==DevOrderStatus::HELD,"second manager HOLD must report HELD");
    check(h2.fee==0.0 && h2.realized_pnl==0.0 && h2.shares==0,
          "second HOLD report must have zero cost, PnL and shares");
    const auto after=central.snapshot();
    check(after.cash==before.cash && after.equity==before.equity &&
          after.gross_exposure==before.gross_exposure &&
          after.realized_pnl==before.realized_pnl &&
          after.open_positions==before.open_positions,
          "HOLD must not mutate consolidated portfolio");
    const auto after_a1=central.managerAccount(m1.manager_id);
    const auto after_a2=central.managerAccount(m2.manager_id);
    check(after_a1.cash==before_a1.cash &&
          after_a1.realized_pnl==before_a1.realized_pnl &&
          after_a1.positions.size()==before_a1.positions.size(),
          "HOLD must not mutate manager 1 account");
    check(after_a2.cash==before_a2.cash &&
          after_a2.realized_pnl==before_a2.realized_pnl &&
          after_a2.positions.size()==before_a2.positions.size(),
          "HOLD must not mutate manager 2 account");
    check(central.audit().size()==audit_before,
          "HOLD must not append central execution audit");
    admin.setActive(m1.manager_id,false);
    denied([&]{(void)gateway.evaluate(m1.manager_id,1001,{candidate("RELIANCE",1001)});},"inactive evaluate");
    denied([&]{(void)gateway.submit(c1,DevSupervisedAction::HOLD,"hold-inactive",0,0.0);},"inactive submit");
    admin.setActive(m1.manager_id,true);
    admin.assignShares(m1.manager_id,{"SBIN"});
    denied([&]{(void)gateway.evaluate(m1.manager_id,1002,{candidate("RELIANCE",1002)});},"reassigned symbol");
    check(!f.execution.eligible(m1.manager_id,"SBIN"),"approval invalid after universe change");
    central.setHalted(true);
    denied([&]{(void)gateway.evaluate(m2.manager_id,1003,{candidate("HDFCBANK",1003)});},"global halt evaluate");
    denied([&]{(void)gateway.submit(c2,DevSupervisedAction::HOLD,"hold-halted",0,0.0);},"global halt submit");
    check(central.snapshot().open_positions==0,"no unauthorized position");
    std::cout<<"PASS 5C-3H-2 supervisor HOLD portfolio/audit safety and authorization\n";
}
