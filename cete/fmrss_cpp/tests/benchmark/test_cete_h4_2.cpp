#include "devai/benchmark/DevCETEReplayAdapter.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace devai::benchmark;
using namespace devai::strategy::d9f4c;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try {
 ReplayResult replay{};
 ReplayStep step{};
 step.decision.decision_time=1780000000;
 step.decision.manager_action=0;step.decision.legal_actions[0]=true;step.decision.terminal=true;
 step.execution.decision_time=step.decision.decision_time;
 step.execution.equity_before=7000;step.execution.equity_after=6998;
 step.execution.gross_pnl=0;step.execution.brokerage=2;step.execution.net_pnl=-2;
 step.execution.numerical_integrity=true;
 step.portfolio.equity=6998;
 replay.steps.push_back(step);replay.final_portfolio=step.portfolio;
 auto rows=DevCETEReplayAdapter::convert(replay,"Candidate0");
 auto report=DevCETEReplayAdapter::summarize(rows);
 check(report.decisions==1&&std::abs(report.net+2)<1e-9,"wrong report");
 bool rejected=false;try{(void)DevCETEReplayAdapter::convert(replay,"Candidate0","DevManager_002");}
 catch(const std::runtime_error&){rejected=true;}check(rejected,"SHORT incorrectly accepted");
 replay.steps[0].execution.equity_after=7001;
 rejected=false;try{(void)DevCETEReplayAdapter::convert(replay,"Candidate0");}
 catch(const std::runtime_error&){rejected=true;}check(rejected,"bad reconciliation accepted");
 auto path=std::filesystem::temp_directory_path()/"cete_h42_replay.csv";
 DevCETEReplayAdapter::writeCsv(path,rows);check(std::filesystem::file_size(path)>100,"empty CSV");
 std::filesystem::remove(path);
 std::cout<<"CETE H4.2 D9F4C REPLAY ADAPTER PASS\n"
 <<"Actual ReplayResult type compatibility: PASS\n"
 <<"Equity/cost reconciliation: PASS\n"
 <<"Unsupported SHORT fail-closed: PASS\n"
 <<"CSV export: PASS\n"
 <<"Historical market-data/model runner: NOT CONNECTED\n";
 }catch(const std::exception& e){std::cerr<<"H4.2 FAIL: "<<e.what()<<'\n';return 1;} }
