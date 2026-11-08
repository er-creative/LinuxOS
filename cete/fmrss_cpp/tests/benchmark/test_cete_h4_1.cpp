#include "devai/benchmark/DevReplayBridge.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace devai::benchmark;
void check(bool ok){if(!ok)throw std::runtime_error("test failure");}
int main(){try{
 std::vector<ReplayEvent> rows={{"Fixture","DevManager_001","RELIANCE","2026-10-29","2026-10-29 09:20:00","LONG","OPEN",100,true,true,true},{"Fixture","DevManager_002","SBIN","2026-10-29","2026-10-29 09:20:00","SHORT","OPEN",100,true,true,true},{"Fixture","DevManager_001","RELIANCE","2026-10-29","2026-10-29 15:15:00","LONG","EXIT",102,true,true,true},{"Fixture","DevManager_002","SBIN","2026-10-29","2026-10-29 15:15:00","SHORT","EXIT",98,true,true,true}};
 auto r=DevReplayBridge::evaluate(rows);check(r.trades.size()==2&&r.reports.size()==2);
 for(auto& t:r.trades)check(t.quantity==52&&std::abs(t.net-64)<1e-9);
 auto bad=rows;bad[0].past_only=false;bool failed=false;try{DevReplayBridge::evaluate(bad);}catch(const std::exception&){failed=true;}check(failed);
 bad=rows;bad.pop_back();failed=false;try{DevReplayBridge::evaluate(bad);}catch(const std::exception&){failed=true;}check(failed);
 std::cout<<"CETE H4.1 REPLAY LEDGER BRIDGE PASS\nLONG/SHORT isolation: PASS\nCost and equity accounting: PASS\nLook-ahead flag rejection: PASS\nUnclosed positions rejection: PASS\nCETE replay engine adapter: NOT CONNECTED\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
