// F3E-F2C.5D: offline-only, one frozen shared neural network, real completed candles.
#include "devai/strategy/dev_ai_strategy/DevFrozenDirectionalDecision_F3EF2C5D.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalOOSRunner.hpp"
#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include "devai/strategy/dev_ai_strategy/DevAgentManagerContract.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"
#include "devai/strategy/dev_ai_strategy/DevTopCandidateSelector.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace ds=devai::strategy;
namespace dd=devai::strategy::f3ef2c5d;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static int minuteIST(std::int64_t t){ auto s=(t+19800)%86400; if(s<0)s+=86400;return int(s/60); }
static std::set<std::string> readSymbols(const std::string& file){
    std::ifstream in(file);check(bool(in),"cannot open shortability file");
    std::set<std::string> result;std::string s;
    while(std::getline(in,s)){if(!s.empty()&&s.back()=='\r')s.pop_back();if(s.empty()||s[0]=='#')continue;
        check(s.find_first_of(" \t,:/")==std::string::npos,"invalid shortability symbol");result.insert(s);}
    check(!result.empty(),"empty shortability list");return result;
}
int main(int argc,char** argv){
 try{
    if(argc!=6 && argc!=9) throw std::invalid_argument(
      "Usage: run_dev_f3ef2c5d_frozen_oos <oos.cache> <minute_folder> <shared.chk> <audit.csv> <train_last_unix_time> [--verified-short <eligible_symbols.txt> <explicit_training_evidence.txt>]");
    const std::string cache=argv[1],folder=argv[2],checkpoint=argv[3],output=argv[4];
    const std::int64_t train_last=std::stoll(argv[5]);
    check(train_last>0,"invalid training cutoff");
    const bool allow_short=argc==9;
    std::set<std::string> eligible;
    if(allow_short){
       check(std::string(argv[6])=="--verified-short","invalid SHORT opt-in flag");
       eligible=readSymbols(argv[7]);
       // This file must be manually reviewed evidence from an actual SHORT-training run.
       // Presence of this file is NOT cryptographic proof or certification.
       std::ifstream evidence(argv[8]);check(bool(evidence),"missing SHORT training evidence");
       std::string first;std::getline(evidence,first);
       check(first=="SHORT_TRAINING_REVIEWED=YES","SHORT evidence not explicitly reviewed");
    }
    const auto data=ds::DevHistoricalTrainingCache::load(cache);
    ds::DevHistoricalTrainingDatasetValidator::validate(data);
    check(!data.records.empty(),"empty OOS cache");
    check(data.records.front().decision_time>train_last,"OOS starts within training period");
    ds::DevSharedNeuralNetwork net;
    net.load(checkpoint); // No calls to train() anywhere in this executable.
    ds::DevHistoricalCloseStore store;
    std::set<std::string> all_symbols;
    for(const auto& record:data.records){
        check(record.decision_time>train_last,"training/OOS timestamp overlap");
        for(const auto& c:record.candidates)all_symbols.insert(c.symbol);
    }
    check(!all_symbols.empty(),"no OOS candidate symbols");
    store.loadMinuteFolder(folder,{all_symbols.begin(),all_symbols.end()});
    const ds::DevOOSPriceProvider prices=[&](const std::string& s,std::int64_t t){return store.completedClose(s,t);};
    ds::DevManager manager;
    ds::DevAgentManagerContract contract;
    ds::DevManagerStateEncoder encoder;
    std::size_t manager_wait=0,keep=0,select=0,open_long=0,open_short=0,exit=0;
    ds::DevFrozenManagerPolicy policy=[&](const ds::DevOOSDecisionPoint& point,const ds::DevOOSLedgerState& ledger){
        std::vector<ds::DevAgentReport> reports;
        reports.reserve(point.candidates.size());
        const auto& names=ds::DevTopCandidateSelector::agentNames();
        for(std::size_t slot=0;slot<point.candidates.size();++slot){
            const auto& c=point.candidates[slot];
            check(c.assignment.slot_index==slot && c.assignment.candidate_rank==slot+1 &&
                  c.assignment.agent_name==names[slot],"invalid ranked Top-5 assignment");
            check(c.observation.decision_time==point.decision_time && c.observation.ready &&
                  c.observation.numerical_integrity,"invalid causal agent observation");
            const auto q=net.predictAgent(c.observation);
            check(q.numerical_integrity,"agent model numerical failure");
            const auto preferred=q.greedyAction();
            ds::DevAgentReport r{};
            r.slot_index=slot;r.agent_name=c.assignment.agent_name;r.assigned=true;
            r.symbol=c.assignment.symbol;r.decision_time=point.decision_time;
            r.candidate_score=c.assignment.candidate_score;r.candidate_rank=c.assignment.candidate_rank;
            r.q_values=q;r.preferred_action=preferred;r.preferred_q_value=q.value(preferred);
            r.numerical_integrity=std::isfinite(r.preferred_q_value);
            reports.push_back(r);
        }
        const auto packet=contract.makePacket(point.decision_time,reports);
        contract.validatePacket(packet);
        const auto state=contract.buildManagerState(packet,point.portfolio,manager);
        manager.validateState(state);
        auto encoded=encoder.encode(state);
        // Fail closed at square-off and block selection of non-held symbols while invested.
        const int minute=minuteIST(point.decision_time);
        if(minute>=915) return std::string{};
        if(ledger.quantity!=0){
            for(std::size_t i=0;i<reports.size();++i)
                if(reports[i].symbol!=ledger.held_symbol)encoded.valid_actions[2+i]=false;
        }else if(minute>=885){
            for(std::size_t i=2;i<encoded.valid_actions.size();++i)encoded.valid_actions[i]=false;
        }
        const auto manager_q=net.predictManager(encoded);
        const std::size_t action=static_cast<std::size_t>(manager_q.greedyAction(encoded.valid_actions));
        dd::Input input{};input.manager_action=action;input.held=ledger.held_symbol;
        input.signed_quantity=ledger.quantity;input.minute_ist=minute;
        input.short_enabled=allow_short;input.model_short_verified=allow_short;
        if(action==0)++manager_wait;
        else if(action==1)++keep;
        else{
            ++select;
            const auto slot=action-2;
            check(slot<reports.size(),"manager selected missing agent");
            input.candidate=reports[slot].symbol;
            input.agent_action=static_cast<std::size_t>(reports[slot].preferred_action);
            input.candidate_shortable=eligible.contains(input.candidate);
            // Greedy Q values may favor SHORT but only if explicit external eligibility is provided.
        }
        const auto desired=dd::resolve(input);
        if(ledger.quantity==0 && !desired.empty()){
            if(desired.rfind("SHORT:",0)==0)++open_short;else ++open_long;
        }else if(ledger.quantity!=0 && desired.empty())++exit;
        return desired;
    };
    ds::DevOOSLedgerConfig config{};
    config.initial_capital=7000.0;config.max_allocation_fraction=.75;
    config.allow_intraday_short=allow_short;
    config.short_square_off_hour_ist=15;config.short_square_off_minute_ist=15;
    // Apply Zerodha equity-intraday statutory charges during frozen evaluation.
    // Training remains ZERO_COST; this executable never updates model weights.
    config.cost_mode=ds::DevOOSCostMode::ZERODHA_EQUITY_INTRADAY;
    config.exchange=ds::DevExchange::NSE;
    config.brokerage_rate=0.0; // Ignored in Zerodha mode; calculator determines brokerage.
    config.other_cost_rate=0.0; // Ignored in Zerodha mode; calculator determines statutory costs.
    config.slippage_rate=0.0005; // 5 bps per executed side, scenario assumption.
    std::cout<<"[CETE][FROZEN_OOS][COST_MODE] ZERODHA_EQUITY_INTRADAY NSE"
             <<" slippage_rate="<<config.slippage_rate<<'\n';
    const auto report=ds::DevHistoricalOOSRunner(config).run(data,prices,policy);
    check(!report.steps.empty(),"no replay steps");
    check(report.steps.back().quantity==0,"OOS ended with open position: require 15:15 records");
    ds::DevHistoricalOOSRunner::writeCsv(output,report);
    std::cout<<"F3E-F2C.5D FROZEN OOS REPLAY COMPLETED (NOT LIVE)\n"
             <<"Model objects: 1 | OOS records: "<<report.steps.size()<<"\n"
             <<"Manager WAIT: "<<manager_wait<<" KEEP: "<<keep<<" SELECT: "<<select<<"\n"
             <<"Proposed LONG: "<<open_long<<" SHORT: "<<open_short<<" EXIT: "<<exit<<"\n"
             <<"Executed legs: "<<report.executed_legs<<" | Final equity: "<<report.final_equity
             <<" | Simulated net PnL: "<<report.net_pnl<<" | Max drawdown: "<<report.max_drawdown
             <<"\nAudit: "<<output<<"\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"F3E-F2C.5D FAILED: "<<e.what()<<'\n';return 1;}
}
