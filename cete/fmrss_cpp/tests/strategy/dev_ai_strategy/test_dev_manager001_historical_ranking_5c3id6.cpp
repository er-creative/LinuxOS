#include "devai/strategy/dev_ai_strategy/DevManager001HistoricalRanking.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
static DevHistoricalStockFrame stock(const char* name, double z) {
    DevHistoricalStockFrame s;
    s.symbol=name;
    s.one_minute.completed_key=100;
    s.five_minute.completed_key=100;
    s.fifteen_minute.completed_key=100;
    s.close_1m.ready=s.close_5m.ready=s.close_15m.ready=true;
    s.close_15m.zscore=z;
    return s;
}
int main() {
    DevManager001HistoricalRanking manager({"DevManager_001",2},
        [](const DevHistoricalStockFrame& s,const DevHistoricalManagerFrame&) {
            // Synthetic fixture policy only; NOT a production trading strategy.
            return DevManager001Assessment{true,-s.close_15m.zscore,"fixture"};
        });
    DevHistoricalManagerFrame f;
    f.manager_id="DevManager_001"; f.decision_key=100;
    f.stocks={stock("SBIN",-1.0),stock("RELIANCE",-2.0),stock("HDFCBANK",0.5)};
    const auto r=manager.evaluate(f);
    assert(r.evaluated==3 && r.selected.size()==2);
    assert(r.selected[0].symbol=="RELIANCE" && r.selected[1].symbol=="SBIN");
    f.stocks[1].fifteen_minute.completed_key=101;
    bool threw=false;
    try { (void)manager.evaluate(f); } catch (const std::invalid_argument&) { threw=true; }
    assert(threw);
    f.stocks[1].fifteen_minute.completed_key=100;
    f.stocks[1].symbol="SBIN";
    threw=false;
    try { (void)manager.evaluate(f); } catch (const std::invalid_argument&) { threw=true; }
    assert(threw);
    std::cout << "PASS 5C-3I-D6 manager-owned strategy ranking contract\n";
}
